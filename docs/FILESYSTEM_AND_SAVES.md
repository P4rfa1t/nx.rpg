# 文件系统、加密归档解包与存档隔离规范

Nintendo Switch 自制软件运行在 SD 卡（`sdmc:/`）之上，面临 **Horizon OS exFAT 驱动掉电损坏风险**、**Windows 游戏大小写混乱** 以及 **7 代引擎截然不同的资源加密与存档机制**。本文档定义统一的工程处理准则。

---

## 1. Switch SD 卡文件系统铁律（FAT32 vs exFAT）

### 风险背景
Horizon OS 内置的 `exFAT` 驱动在进程异常中止（崩溃、强行按电源键、OOM）且存在未同步的目录项写入时，极易发生 **FAT 表簇链断裂导致整个目录文件归零或损坏**。这也是 Switch 自制软件圈长期强烈推荐 **FAT32** 的根本原因。

### `nx.rpg` 防损工程规范

1. **游戏本体目录严格只读（Immutable Game Folder）**：
   - `sdmc:/switch/nx.rpg/games/<GameFolder>/` 在运行期视为**只读介质**，任何核心均禁止直接向游戏目录写入存档、临时配置或日志文件。
2. **全链路原子写入（Atomic Write-Temp-Rename）**：
   - 所有配置（`global.json`、`games/<id>.json`）、启动清单（`launch.json`）、扫描缓存（`library.json`）以及 `nxjs-mvz` / `mkxp-z` 的存档落盘，必须遵循三步原子写协议：
     1. 写入同目录临时文件 `<filename>.tmp`
     2. 调用 `fflush()` + `fsync(fd)` 确保数据块落盘
     3. 调用 `rename("<filename>.tmp", "<filename>")` 原子替换目标文件
3. **前端环境医生巡检**：
   - `EnvironmentDoctor` 在诊断页提示用户优先使用 FAT32 格式 SD 卡，并在每次启动时自动清理残留的孤立 `.tmp` 文件。

---

## 2. 大小写不敏感路径解析（Case-Insensitive VFS）

绝大多数 RPG Maker 游戏在 Windows（NTFS 大小写不敏感）上制作，经常出现脚本请求 `Audio/BGM/Town1.ogg`，而实际文件名为 `audio/bgm/TOWN1.OGG` 的情况。三大核心分别采取以下零开销方案：

| 核心 | 大小写兼容实现机制 |
| :--- | :--- |
| **EasyRPG Player** | 上游 `DirectoryTree` 已原生对目录条目建立小写键映射缓存，天然大小写无关。 |
| **mkxp-z** | 上游 `mkxp.json` 默认开启 `"pathCache": true`，启动时通过 PhysFS 枚举并建立全小写路径映射表，零额外开发成本。 |
| **nxjs-mvz** | 由 `cores/nxjs-mvz/shim/src/vfs/` 在启动阶段一次性扫描 `contentRoot` 构建 `Map<lowercaseRelPath, actualSdPath>`，拦截所有 `fetch()`、`XMLHttpRequest`、`Image.src` 与 `Audio.src` 请求。 |

---

## 3. 7 代引擎加密资源与特殊格式支持矩阵

| 引擎 | 常见归档 / 加密格式 | 密钥 / 格式结构 | 核心解密支持方式 |
| :--- | :--- | :--- | :--- |
| **RM 2000 / 2003** | `.ldb` / `.lmt` / `.lmu`（LCF 二进制）、`.xyz`（zlib 压缩位图） | 无加密，强依赖代码页编码 | `EasyRPG/Player` + `liblcf` 原生完整支持 |
| **RM XP** | `Game.rgssad`（或 `<execName>.rgssad`） | `RGSSAD\0\x01` 魔数 + 线性同余 `0xDEADCAFE` 滚动异或 | `mkxp-z` 内置 PhysFS RGSS1 归档驱动原生支持 |
| **RM VX** | `Game.rgss2a`（或 `<execName>.rgss2a`） | `RGSSAD\0\x01` 同系归档结构 | `mkxp-z` 内置 PhysFS RGSS2 归档驱动原生支持 |
| **RM VX Ace** | `Game.rgss3a`（或 `<execName>.rgss3a`） | `RGSSAD\0\x03` 魔数 + 种子表异或 | `mkxp-z` 内置 PhysFS RGSS3 归档驱动原生支持 |
| **RM MV** | `.rpgmvp`（PNG）、`.rpgmvo`（OGG）、`.rpgmvm`（M4A） | 16 字节伪头 `52 50 47 4D 56...` + 载荷前 16 字节与 `System.json -> encryptionKey` 异或 | `nxjs-mvz/shim/src/crypto/rpgm-decryptor.ts` 流式剥头异或还原（兼容 MV 原生解密器与 `<img>` 直读场景） |
| **RM MZ** | `.png_`、`.ogg_`、`.m4a_` | 与 MV 相同的 16 字节伪头 + `encryptionKey` 前 16 字节异或 | 同上，由 `rpgm-decryptor.ts` 统一处理 |

> **重命名归档特别处理**：不少同人作者会将 `Game.exe`、`Game.ini`、`Game.rgss3a` 同步改名为例如 `MyGame.exe`、`MyGame.ini`、`MyGame.rgss3a`。扫描器会自动识别配对的文件名茎写入 `launch.json -> game.execName`，并传递给 `mkxp-z` 的 `Config::execName` 以确保加密包成功挂载。

---

## 4. 三大核心存档完全隔离方案（`sdmc:/switch/nx.rpg/saves/<game_id>/`）

所有存档统一按 `game.id` 存放在 `saves/` 下，方便用户使用 JKSV / FTP 一键备份或升级游戏本体而不丢档：

1. **EasyRPG (`rm2k` / `rm2k3`)**：
   - 原生支持命令行 `--save-path sdmc:/switch/nx.rpg/saves/<game_id>`，`Save01.lsd ~ Save15.lsd` 直接落入隔离目录。
2. **nxjs-mvz (`rmmv` / `rmmz`)**：
   - **NACP 层关闭系统存档**：`package.json` 设置 `"userAccountSaveDataSize": 0`，阻止 `nx.js` 默认挂载 Switch NAND 账户存档（避免每次进游戏弹出系统选人界面，且避免多款 MV 游戏互相覆盖 `RPG File1` 键名）。
   - **双通道接管**：
     - Web 存储模式：自定义 `window.localStorage` 读写 `saves/<game_id>/localstorage.json`。
     - NW.js 本地文件模式（`pretendNwjs: true`）：`nwjs` 垫片将 `StorageManager` 对 `save/file1.rpgsave` 或 `save/file0.rmmzsave` 的 `fs` 调用透明映射至 `saves/<game_id>/save/`。
   - **已有旧档兼容（Copy-on-Write）**：若检测到用户拷入的游戏目录自身携带初始 `save/` 存档且 `saves/<game_id>/` 为空，首次读取自动从原目录回退读取，写入时落入隔离目录。
3. **mkxp-z (`rmxp` / `rmvx` / `rmvxa`)**：
   - RGSS 脚本通常直接硬编码 `File.open("Save#{index + 1}.rxdata", "wb")` 写往游戏根目录。
   - 通过预加载脚本 [00_nxrpg_save_redirect.rb](../cores/mkxp-z/preload/00_nxrpg_save_redirect.rb) 读取环境变量 `NXRPG_SAVE_DIR`，拦截针对存档文件模式的 `File` / `Dir.glob` 操作并重定向至 `saves/<game_id>/`，同样具备旧档只读回退（Copy-on-Write）能力。

---

## 5. 游戏库增量扫描缓存（`cache/library.json`）

在 FAT32 SD 卡上递归打开上百个游戏目录并解析 `RPG_RT.ldb` / `System.json` / `Game.ini` 可能需要数秒时间。为保证每次从核心退出返回前端都做到瞬时响应：

- 首次进入前端时执行目录扫描，记录每个游戏目录的 `dirMtime` 并写入 `sdmc:/switch/nx.rpg/cache/library.json`（Schema 见 [library-cache.schema.json](../common/schemas/library-cache.schema.json)），同时将提取的标题图缩放转存为 `cache/covers/<game_id>.jpg`。
- 后续启动（尤其是 `--from-core` 返回时）仅比对一级/二级子目录 `stat().st_mtime`：目录未变动则直接复用缓存元数据，仅对新增或修改过的文件夹触发增量重扫。
