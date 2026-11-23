# 第三方核心与许可证合规说明

`nx.rpg` 项目整合了三个独立的开源运行核心。由于各核心采用不同的开源许可证，`nx.rpg` 在架构层面严格遵循 **独立可执行进程（独立 `.nro` 二进制）** 隔离原则，不将互不兼容的 GPL 代码静态链接到同一可执行文件中。

---

## 上游核心许可清单

| 核心组件 | 上游项目 | 许可证 | 独立产物 | 源码分发要求 |
| :--- | :--- | :--- | :--- | :--- |
| **EasyRPG Player** | [EasyRPG/Player](https://github.com/EasyRPG/Player) | **GPL-3.0** | `cores/easyrpg.nro` | 分发二进制时须同步提供对应版本源码及 `cores/easyrpg/patches/` 补丁 |
| **mkxp-z** | [mkxp-z/mkxp-z](https://github.com/mkxp-z/mkxp-z) | **GPL-2.0**（启用 `extra_shaders` 时为 **GPL-3.0**） | `cores/mkxp-z.nro` | 分发二进制时须同步提供对应版本源码、Ruby/依赖配置及 `cores/mkxp-z/patches/` 补丁 |
| **nx.js** | [TooTallNate/nx.js](https://github.com/TooTallNate/nx.js) | **MIT** | `cores/nxjs-mvz.nro` | 保留原始版权与 MIT 许可声明（含 V8 / Skia / FFmpeg 等内嵌组件声明） |

---

## 合规隔离原则

1. **进程级边界**：`nx.rpg.nro`（前端）与三个核心 `.nro` 之间仅通过 SD 卡上的只读/读写数据文件（`launch.json`）及 `libnx` 的 `envSetNextLoad()` 命令行参数通信，构成独立程序之间的调用（Aggregate），不产生跨核心静态/动态链接。
2. **补丁可追溯**：任何对 GPL 核心的 Switch 适配修改均以 `.patch` 文件或独立公开 Fork 形式保存在本仓库中，并在每次 Release 中标注对应的上游 Commit SHA。
3. **不分发受版权保护的游戏资产**：本仓库**不包含**任何 RPG Maker 官方 RTP（Run Time Package）、商业字体或游戏数据文件；用户需自行准备合法获取的游戏与 RTP 资源并放置于 SD 卡对应目录。
