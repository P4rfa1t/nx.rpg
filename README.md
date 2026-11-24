# nx.rpg

**nx.rpg** 是一款面向 **Nintendo Switch** 的全世代 RPG Maker 统一运行器（All-in-One Runner），基于 **C++20 + Borealis** 原生风格统一前端与三个独立 `.nro` 核心链式加载架构，支持 **RPG Maker 2000、2003、XP、VX、VX Ace、MV、MZ** 共 7 代引擎作品。

> **当前阶段**：`Phase 0 完成 — 深度架构规范、JSON 契约、上游锁文件与各子模块接口骨架已就绪`（即将进入 Phase 1 编码实现）。

---

## 支持矩阵与锁定基线

三大上游核心通过 [third_party/upstreams.lock.json](third_party/upstreams.lock.json) 精确锁定 Commit SHA，配合补丁队列（`cores/*/patches/SERIES`）实现低侵入可复现构建：

| RPG Maker 世代 | 脚本 / 运行时 | 调度核心 | 上游基线 | 底层图形 / 音频栈（Switch） | 适配复杂度 |
| :--- | :--- | :--- | :--- | :--- | :---: |
| **RM 2000 / 2003** | LCF 事件解释器 | `cores/easyrpg.nro` | [EasyRPG/Player](https://github.com/EasyRPG/Player) (`a99ca834`) | EGL/OpenGL (`glad`) + `libnx` `audout` | 低（Phase 1） |
| **RM MV** | JS / PixiJS v4 | `cores/nxjs-mvz.nro` | [TooTallNate/nx.js](https://github.com/TooTallNate/nx.js) (`462203af`) | V8 (`>=15.0.243-9`) + Skia 2D / GLES3 WebGL2 + FFmpeg `audren` | 中（Phase 2） |
| **RM MZ** | JS / PixiJS v5 / Effekseer WASM | `cores/nxjs-mvz.nro` | [TooTallNate/nx.js](https://github.com/TooTallNate/nx.js) (`462203af`) | 同上 + 全内存模式 V8 WASM JIT | 中（Phase 2） |
| **RM XP / VX / VX Ace** | Ruby MRI 3.1 (RGSS 1/2/3) | `cores/mkxp-z.nro` | [mkxp-z/mkxp-z](https://github.com/mkxp-z/mkxp-z) (`a8f9d9ee`) | SDL2 + Mesa EGL/GLES2 + OpenAL Soft + PhysFS | 高（Phase 3） |

---

## 仓库目录全景

```text
nx.rpg/
├── common/                         # 跨平台 C++20 公共库骨架与权威 JSON Schemas
│   ├── CMakeLists.txt              # Switch + 宿主机双目标构建骨架
│   ├── schemas/                    # 5 份 JSON Schema (global / game / launch / cache / upstreams-lock)
│   └── include/nxrpg/              # 引擎识别、三层配置合并、原子写与 envSetNextLoad 接口头文件
├── launcher/                       # 统一前端 nx.rpg.nro (C++20 + Borealis)
│   ├── CMakeLists.txt              # Switch NRO 与桌面预览双模式构建配置
│   ├── npdm.json                   # Switch TitleID (0x016e787270670000) 与服务权限声明
│   ├── include/launcher/           # 增量游戏库服务、环境医生、启动调度器与 Borealis 视图层头文件
│   └── resources/                  # 多语言文案 (zh-CN / en-US) 与 XML 布局目录
├── cores/                          # 三大核心的补丁队列、交叉配置与兼容垫片
│   ├── easyrpg/                    # EasyRPG CMake 叠加配置与 3 枚补丁规格（含 Switch 自定义 RTP 修复）
│   ├── nxjs-mvz/                   # nx.js (--fat) 配置、补丁及模块化 TypeScript MV/MZ Shim 接口骨架
│   └── mkxp-z/                     # devkitA64 Meson 交叉配置、静态 Ruby 3.1 清单与 RGSS 预加载脚本骨架
├── docs/                           # 完整工程设计规范文档集
│   ├── ARCHITECTURE.md             # 总体拓扑、Applet vs 全内存 V8/WASM 机制、CPU 调频与 SD 布局
│   ├── ENGINE_DETECTION.md         # 7 代引擎识别优先级、重命名 execName 推断、编码与 RTP 探测
│   ├── LAUNCH_PROTOCOL.md          # 三层配置合并、按核心区分的 argv 规则与回跳协议
│   ├── INPUT_AND_UI_SPEC.md        # 跨 7 代手柄按键矩阵、触屏双指取消、swkbd 软键盘与前端交互
│   ├── FILESYSTEM_AND_SAVES.md     # FAT32/exFAT 原子写防损、大小写无关 VFS、7 代解密与存档完全隔离
│   ├── BUILD_AND_CI.md             # 容器化工具链、TitleID/图标规范与 GPL 合规源码归档流水线
│   ├── ROADMAP.md                  # Phase 0 ~ Phase 5 实施路线图
│   └── cores/                      # EasyRPG / nxjs-mvz / mkxp-z 三份专项集成方案
├── sdcard/switch/nx.rpg/           # Switch SD 卡部署目录结构模板与示例配置
├── tests/                          # 宿主机无硬件单元测试与 7 代合成测试夹具规范
├── scripts/                        # 环境自检、锁定 SHA 检出、补丁队列应用与全仓契约校验脚本
└── third_party/                    # upstreams.lock.json 锁文件与上游源码挂载点（源码已 gitignore）
```

---

## 快速校验与上游检出

```bash
# 查看可用命令
make help

# 校验全仓 JSON Schema、示例配置、锁文件、补丁队列与 Shell 脚本语法
make validate

# 检查本地 devkitPro / CMake / Meson / Node / pnpm 工具链状态
make check-env

# 按 third_party/upstreams.lock.json 锁定的精确 Commit SHA 拉取三大上游核心源码
make bootstrap-cores
```

---

## 许可证

- 本仓库自有代码（`common/`、`launcher/`、`cores/nxjs-mvz/shim/`、`scripts/`、文档与 Schema）采用 **[MIT License](LICENSE)**。
- 三大上游核心作为独立 `.nro` 可执行进程分发，严格遵循各自开源许可证（`EasyRPG`: GPL-3.0、`mkxp-z`: GPL-2.0、`nx.js`: MIT），详见 [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md)。
