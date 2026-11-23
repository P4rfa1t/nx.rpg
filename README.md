# nx.rpg

**nx.rpg** 是一款面向 **Nintendo Switch** 的多引擎 RPG Maker 统一运行器（All-in-One Runner），旨在通过整合三个成熟开源运行核心，在一套统一的前端与配置体系下支持全世代主流 RPG Maker 游戏。

> **当前阶段**：`Phase 0 — 项目架构设计与目录布局`（规划与脚手架初始化中，尚未发布可运行 `.nro` 构建产物）。

---

## 支持目标矩阵

| RPG Maker 版本 | 脚本 / 运行时 | 对应核心 | 上游仓库 | Switch 适配现状与难度 |
| :--- | :--- | :--- | :--- | :--- |
| **RPG Maker 2000** | LCF / 事件解释器 | **EasyRPG Player** | [EasyRPG/Player](https://github.com/EasyRPG/Player) | **低** — 上游原生支持 `devkitA64` / `Switch.cmake` |
| **RPG Maker 2003** | LCF / 事件解释器 | **EasyRPG Player** | [EasyRPG/Player](https://github.com/EasyRPG/Player) | **低** — 上游原生支持 `devkitA64` / `Switch.cmake` |
| **RPG Maker MV** | JS / PixiJS v4 / WebAudio | **nx.js** + MV/MZ Shim | [TooTallNate/nx.js](https://github.com/TooTallNate/nx.js) | **中** — 上游原生提供 V8 / Skia Canvas2D / WebGL2 / WebAudio，需补齐 DOM 与 NW.js 兼容层 |
| **RPG Maker MZ** | JS / PixiJS v5 / WebGL2 / Effekseer WASM | **nx.js** + MV/MZ Shim | [TooTallNate/nx.js](https://github.com/TooTallNate/nx.js) | **中** — 依托 nx.js 的 WebGL2 + WASM 能力，需补齐 DOM / FontFace / 插件兼容层 |
| **RPG Maker XP** | Ruby (RGSS1) | **mkxp-z** | [mkxp-z/mkxp-z](https://github.com/mkxp-z/mkxp-z) | **高** — 需移植 Meson + 静态 Ruby MRI 3.1 + SDL2/GLES2/OpenAL Soft 至 `devkitA64` |
| **RPG Maker VX** | Ruby (RGSS2) | **mkxp-z** | [mkxp-z/mkxp-z](https://github.com/mkxp-z/mkxp-z) | **高** — 同上（RGSS2） |
| **RPG Maker VX Ace** | Ruby (RGSS3) | **mkxp-z** | [mkxp-z/mkxp-z](https://github.com/mkxp-z/mkxp-z) | **高** — 同上（RGSS3） |

---

## 核心架构设计

`nx.rpg` 采用 **统一前端 + 独立核心 NRO 链式加载（`envSetNextLoad`）** 的多进程式隔离架构，而非将三大引擎硬链接为单个庞大二进制：

```text
+-----------------------------------------------------------------------+
|                    nx.rpg.nro (统一前端 Launcher)                      |
|  游戏扫描 / 7代引擎自动识别 / 封面与元数据 / RTP与字体管理 / 按键配置  |
+-----------------------------------+-----------------------------------+
                                    |
        写入 sdmc:/switch/nx.rpg/runtime/launch.json
        并通过 libnx envSetNextLoad() 链式加载对应核心
                                    |
        +---------------------------+---------------------------+
        v                           v                           v
+------------------+     +--------------------+     +--------------------+
| cores/easyrpg    |     | cores/nxjs-mvz     |     | cores/mkxp-z       |
| (easyrpg.nro)    |     | (nxjs-mvz.nro)     |     | (mkxp-z.nro)       |
| RM 2000 / 2003   |     | RM MV / MZ         |     | RM XP / VX / VXAce |
| C++ / CMake      |     | V8 / Skia / WebGL2 |     | C++ / Ruby / Meson |
| GPL-3.0          |     | MIT                |     | GPL-2.0 (+GPL-3.0) |
+------------------+     +--------------------+     +--------------------+
        |                           |                           |
        +---------------------------+---------------------------+
                                    |
               退出游戏后自动 envSetNextLoad 返回 nx.rpg.nro
```

### 为什么采用多 NRO 隔离架构？

1. **开源许可证严格合规**：`mkxp-z`（GPL-2.0）、`EasyRPG Player`（GPL-3.0）与 `nx.js`（MIT）存在许可边界差异；拆分为独立可执行 `.nro` 可避免单二进制静态链接带来的许可证冲突。
2. **Switch 内存预算最大化**：V8 堆内存（MV/MZ）与 Ruby/位图缓存（XP/VX/VXAce）对内存敏感，独立核心能让当前运行的游戏独占全部可用内存（Title Takeover 全内存模式下约 3.2 GB+）。
3. **保留上游原生构建系统**：CMake（EasyRPG）、Makefile + pnpm（nx.js）、Meson（mkxp-z）互不干扰，通过 `third_party/` + `patches/` 队列持续同步上游更新。
4. **崩溃隔离与可维护性**：单个核心的崩溃、图形上下文（GLES2 vs GLES3/Skia）或音频后端（`audren` vs OpenAL vs SDL2）互不污染。

---

## 目录结构

```text
nx.rpg/
├── docs/                       # 架构设计、路线图、引擎识别、启动协议与各核心规划文档
│   ├── ARCHITECTURE.md         # 系统总体架构、内存策略与 SD 卡布局
│   ├── ROADMAP.md              # Phase 0 ~ Phase 5 里程碑规划与风险评估
│   ├── ENGINE_DETECTION.md     # 7 代 RPG Maker 自动检测规则与字符编码/RTP 策略
│   ├── LAUNCH_PROTOCOL.md      # Launcher <-> Core 通信协议与 launch.json 规范
│   └── cores/                  # 三大核心各自的移植/集成专项方案
├── launcher/                   # 主前端应用（nx.rpg.nro）
├── cores/                      # 三大核心的适配层、补丁队列与交叉编译配置
│   ├── easyrpg/                # EasyRPG Player 适配与补丁
│   ├── nxjs-mvz/               # nx.js 的 RPG Maker MV/MZ DOM/NW.js 兼容层与补丁
│   └── mkxp-z/                 # mkxp-z 的 Switch/devkitA64 Meson 交叉配置与补丁
├── common/                     # 共享 Schema、协议定义与通用配置结构
│   └── schemas/                # launch-manifest 与 game-config 的 JSON Schema
├── assets/                     # 应用图标、默认封面与 NACP 资源
├── sdcard/                     # Switch SD 卡目标目录结构模板（sdmc:/switch/nx.rpg/）
├── scripts/                    # 环境检查与上游核心仓库拉取脚本
└── third_party/                # 上游核心源码挂载目录（通过脚本拉取，默认不提交源码）
```

---

## 规划文档索引

- [系统总体架构（ARCHITECTURE.md）](docs/ARCHITECTURE.md)
- [分阶段开发路线图（ROADMAP.md）](docs/ROADMAP.md)
- [引擎自动识别、编码与 RTP 规范（ENGINE_DETECTION.md）](docs/ENGINE_DETECTION.md)
- [Launcher 与 Core 启动协议（LAUNCH_PROTOCOL.md）](docs/LAUNCH_PROTOCOL.md)
- 核心集成专项规划：
  - [EasyRPG Player 核心规划（RM2000 / RM2003）](docs/cores/easyrpg-plan.md)
  - [nx.js 核心规划（RM MV / MZ）](docs/cores/nxjs-mvz-plan.md)
  - [mkxp-z 核心规划（RM XP / VX / VX Ace）](docs/cores/mkxp-z-plan.md)

---

## 开发环境准备（预览）

后续进入核心构建阶段将依赖以下工具链：

- [devkitPro](https://devkitpro.org/)（`devkitA64`、`libnx`、`switch-portlibs`、`switch-tools`）
- `cmake` / `ninja`（用于 EasyRPG Player）
- `node` / `pnpm`（用于 nx.js 运行时与 MV/MZ Shim 构建）
- `meson` / `pkg-config`（用于 mkxp-z 交叉编译）

可通过以下脚本检查当前环境或拉取上游仓库：

```bash
make help
./scripts/check-env.sh
./scripts/bootstrap-cores.sh
```

---

## 许可证

- `nx.rpg` 仓库自身的前端代码、兼容层脚本、协议定义与文档采用 **MIT License**（见 [LICENSE](LICENSE)）。
- 三个运行核心保留各自上游开源许可证，详细隔离策略与署名见 [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md)。
