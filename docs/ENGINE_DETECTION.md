# 7 代 RPG Maker 引擎自动识别与资产治理规范

用户只需将游戏目录拷贝到 `sdmc:/switch/nx.rpg/games/<GameFolder>/`，`nx.rpg` 的扫描器（位于 `common/`）会按照确定性优先级自动识别引擎版本、提取元数据并选择对应核心。

---

## 1. 自动检测优先级与特征规则

检测器按 **特征唯一性从高到低** 依次匹配，避免目录中残留无关文件导致误判：

| 优先级 | 目标引擎 | 核心 | 决定性特征文件 / 规则 | 备注 |
| :---: | :--- | :--- | :--- | :--- |
| **1** | **RPG Maker MZ** (`rmmz`) | `nxjs-mvz` | 存在 `js/rmmz_core.js`（且通常存在 `index.html` + `data/System.json`） | `js/libs/effekseer.wasm` 为可选特征 |
| **2** | **RPG Maker MV** (`rmmv`) | `nxjs-mvz` | 存在 `www/js/rpg_core.js` **或** `js/rpg_core.js`（兼容桌面打包结构与 Web 部署结构） | 需记录 `contentRoot`（`www/` 或游戏根目录） |
| **3** | **RPG Maker VX Ace** (`rmvxa`) | `mkxp-z` | 满足以下任一：<br>1. `Game.ini` 中 `Library` 匹配 `RGSS3*.dll`<br>2. 存在 `Data/Scripts.rvdata2` 或 `Data/System.rvdata2`<br>3. 存在 `Game.rgss3a` | `rgssVersion = 3` |
| **4** | **RPG Maker VX** (`rmvx`) | `mkxp-z` | 满足以下任一：<br>1. `Game.ini` 中 `Library` 匹配 `RGSS2*.dll`<br>2. 存在 `Data/Scripts.rvdata` 或 `Data/System.rvdata`<br>3. 存在 `Game.rgss2a` | `rgssVersion = 2` |
| **5** | **RPG Maker XP** (`rmxp`) | `mkxp-z` | 满足以下任一：<br>1. `Game.ini` 中 `Library` 匹配 `RGSS1*.dll`<br>2. 存在 `Data/Scripts.rxdata` 或 `Data/System.rxdata`<br>3. 存在 `Game.rgssad` | `rgssVersion = 1` |
| **6** | **RPG Maker 2003** (`rm2k3`) | `easyrpg` | 存在 `RPG_RT.ldb` + `RPG_RT.lmt`，且满足以下任一：<br>1. 解析 `RPG_RT.ldb` 头部的引擎版本字段为 `2003`<br>2. 存在 `Battle2/` 或 `BattleCharSet/` 目录或 `RPG_RT.ini` 指向 2003 RTP | 精确区分 2000/2003 供 RTP 选择与前端徽章展示 |
| **7** | **RPG Maker 2000** (`rm2k`) | `easyrpg` | 存在 `RPG_RT.ldb` + `RPG_RT.lmt` 且不满足 RM2003 特征 | 默认回退为 `rm2k` |

### 特殊打包结构兼容

- **MV 桌面部署目录**：PC 版 RPG Maker MV 游戏往往将实际游戏资源放在 `<GameFolder>/www/` 下（外层是 NW.js 的 `Game.exe` 等无关文件），检测器必须自动将 `contentRoot` 指向 `<GameFolder>/www/`。
- **macOS `.app` 包 / 单层嵌套目录**：若游戏目录仅包含单一子目录且顶层无识别特征，允许向下探测 1 层目录。
- **用户手动覆写**：若自动识别失败或误判，用户可在单游戏配置 `config/games/<game_id>.json` 中显式指定 `engineOverride`。

---

## 2. 游戏元数据与封面提取

扫描器会为每个识别出的游戏生成稳定的 `gameId`（基于规范化相对路径的短哈希），并尝试提取以下展示信息：

| 引擎系列 | 游戏标题来源 | 封面 / 图标提取来源 |
| :--- | :--- | :--- |
| **RM2000 / 2003** | `RPG_RT.ini` 中的 `GameTitle`（按检测编码转 UTF-8） | `Title/` 下的首张标题图（通过 `RPG_RT.ldb` 指定或首个 `.png`/`.xyz`/`.bmp`） |
| **XP / VX / VXAce** | `Game.ini` 中的 `Title` | `Graphics/Titles/` 或 `Graphics/Titles1/`（加密包 `.rgss*` 后续通过轻量索引器提取） |
| **MV / MZ** | `data/System.json` 中的 `gameTitle` | `img/titles1/<title1Name>.png` 与 `icon/icon.png` |

---

## 3. 字符编码治理（CJK 游戏关键问题）

传统 RPG Maker（2000 / 2003 / XP / VX / VX Ace）的配置文件与旧工程大量使用非 UTF-8 编码：

- **默认策略**：
  - `rmmv` / `rmmz`：固定 `UTF-8`。
  - `rm2k` / `rm2k3`：默认 `auto`（交由 EasyRPG 自动检测），支持单游戏覆写 `Shift_JIS`（CP932）、`GBK`（CP936）、`Big5`（CP950）、`CP949`、`CP1252`。
  - `rmxp` / `rmvx` / `rmvxa`：`Game.ini` 读取支持自动编码探测与覆写；RGSS3（VXAce）内部脚本为 UTF-8，RGSS1/2 常见问题通过字体回退与字符集提示处理。

---

## 4. 共享 RTP（Run Time Package）管理

多个老世代 RPG Maker 游戏往往共享同一套 RTP 素材。为避免每个游戏目录重复拷贝几百 MB RTP，`nx.rpg` 在 SD 卡提供集中式 RTP 目录：

```text
sdmc:/switch/nx.rpg/rtp/
├── rm2000/     # RPG Maker 2000 RTP
├── rm2003/     # RPG Maker 2003 RTP
├── rmxp/       # RPG Maker XP RTP
├── rmvx/       # RPG Maker VX RTP
└── rmvxa/      # RPG Maker VX Ace RTP
```

Launcher 在生成 `launch.json` 时，会根据检测到的 `engine` 自动附带对应的 `rtpPath`，由对应核心作为只读回退资源层挂载（游戏自有资源优先级始终高于共享 RTP）。
