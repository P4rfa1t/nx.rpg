# 7 代 RPG Maker 引擎自动识别与资产治理规范

用户只需将游戏目录放入 `sdmc:/switch/nx.rpg/games/`（支持按系列或作者建立最多 `maxScanDepth` 层分类子目录），`nxrpg::EngineDetector`（接口见 [engine_detector.hpp](../common/include/nxrpg/engine_detector.hpp)）即可自动识别 7 代引擎、推断被重命名的工程入口茎并提取展示元数据。

---

## 1. 确定性识别优先级表（由高到低）

所有文件探测均通过 `nxrpg::paths::resolve_case_insensitive` 进行大小写无关匹配：

| 优先级 | 引擎枚举 | 调度核心 | 决定性特征规则 | 衍生参数提取 |
| :---: | :--- | :--- | :--- | :--- |
| **1** | `rmmz` | `nxjs-mvz` | `<root>/js/rmmz_core.js` 存在，或 `<root>/www/js/rmmz_core.js` 存在 | `contentRoot` 指向含 `js/rmmz_core.js` 的目录；`rgssVersion = 0` |
| **2** | `rmmv` | `nxjs-mvz` | `<root>/www/js/rpg_core.js` 存在（PC NW.js 部署结构）**或** `<root>/js/rpg_core.js` 存在（Web 部署结构） | `contentRoot` 自动剥离外层壳指向 `www/` 或 `<root>`；`rgssVersion = 0` |
| **3** | `rmvxa` | `mkxp-z` | 满足任一：<br>1. 任意 `<Stem>.rgss3a` 存在<br>2. `Data/Scripts.rvdata2` 或 `Data/System.rvdata2` 存在<br>3. `<Stem>.ini` 中 `Library` 匹配 `RGSS3*.dll` | `rgssVersion = 3`；记录匹配的 `<Stem>` 为 `execName` |
| **4** | `rmvx` | `mkxp-z` | 满足任一：<br>1. 任意 `<Stem>.rgss2a` 存在<br>2. `Data/Scripts.rvdata` 或 `Data/System.rvdata` 存在<br>3. `<Stem>.ini` 中 `Library` 匹配 `RGSS2*.dll` | `rgssVersion = 2`；记录匹配的 `<Stem>` 为 `execName` |
| **5** | `rmxp` | `mkxp-z` | 满足任一：<br>1. 任意 `<Stem>.rgssad` 存在<br>2. `Data/Scripts.rxdata` 或 `Data/System.rxdata` 存在<br>3. `<Stem>.ini` 中 `Library` 匹配 `RGSS1*.dll` | `rgssVersion = 1`；记录匹配的 `<Stem>` 为 `execName` |
| **6** | `rm2k3` | `easyrpg` | `RPG_RT.ldb` + `RPG_RT.lmt` 存在，且满足任一：<br>1. `RPG_RT.ldb` LCF 数据库版本字段为 RM2003（如 `>= 2003` 特征块）<br>2. 存在 `Battle2/`、`BattleCharSet/`、`BattleWeapon/` 目录<br>3. `RPG_RT.ini` 声明 `FullPackageFlag` / 2003 RTP 键 | 挂载 `rtp/rm2003` |
| **7** | `rm2k` | `easyrpg` | `RPG_RT.ldb` + `RPG_RT.lmt` 存在且不命中 `rm2k3` 规则 | 挂载 `rtp/rm2000` |

### 特殊打包形态与重命名工程处理

1. **重命名可执行文件茎（`execName` 推断）**：
   - 大量 XP / VX / VX Ace 作者会将 `Game.exe`、`Game.ini`、`Game.rgss3a` 改为游戏名（如 `Ib.exe`、`Ib.ini`、`Ib.rgssad`）。
   - 检测器优先检查 `Game.ini`；若不存在，则枚举根目录所有 `.rgss3a` / `.rgss2a` / `.rgssad` 或包含 `[Game]` 节的 `.ini` 文件，自动提取其文件名茎写入 `game.execName` 并交给 `mkxp-z`。
2. **单层压缩包解压嵌套目录**：
   - 若用户解压后形成 `games/GameA/GameA/RPG_RT.ldb` 双层同名目录，扫描器在 `maxScanDepth`（默认 `2`）范围内自动向下穿透，将外层或内层识别为单一游戏条目，不会重复展示或漏检。
3. **手动强制指定**：
   - 若极端魔改工程（如自定义脚本加载器）导致自动识别偏差，玩家可在 `config/games/<id>.json` 设置 `"engineOverride"` 与 `"execNameOverride"` 强制覆盖。

---

## 2. 稳定 `game.id` 生成算法

为确保游戏拷贝、重排顺序或前端升级后存档目录 `saves/<game_id>/` 永不丢失：
- 以游戏目录相对于 `sdmc:/switch/nx.rpg/games/` 的规范化路径（统一 `/` 分隔符）计算 **FNV-1a / xxHash32 6 位十六进制短指纹**，拼接清洗后的 ASCII 目录名前缀（例如 `ib-remake-8f3a21`）。
- 即使两款不同子目录下的同人游戏文件夹同名，短哈希后缀也能保证 `saves/<game_id>/` 绝对不冲突。

---

## 3. 元数据、标题转码与共享 RTP 检测

- **标题编码探测**：
  - `rmmv` / `rmmz`：直接按 UTF-8 解析 `data/System.json -> gameTitle`（去除控制码）。
  - `rmxp` / `rmvx` / `rmvxa` / `rm2k` / `rm2k3`：读取 `<execName>.ini` 或 `RPG_RT.ini` 的 `Title` / `GameTitle` 字节流；首先验证合法 UTF-8，否则结合字节分布特征（或 `uchardet`）在 `GBK`、`Shift_JIS`、`Big5`、`CP949` 间择优解码为 UTF-8，并将推断出的编码记录到 `detectedEncoding`。
- **`needsRtp` 提示标记**：
  - 若 `RPG_RT.ini` 未设置 `FullPackageFlag=1`（RM2k/2k3）或 `<execName>.ini` 中声明了非空 `RTP1=/RTP2=/RTP3=`（XP/VX/VXAce），则标记 `needsRtp = true`。若对应的 `sdmc:/switch/nx.rpg/rtp/<engine>/` 目录为空，Launcher 在详情页提前给出黄色提示。
