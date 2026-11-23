# cores — 运行核心适配层与补丁队列

本目录仅存放 `nx.rpg` 对三个上游核心的**适配代码、补丁队列（`patches/`）与构建配置**。上游核心完整源码通过 `./scripts/bootstrap-cores.sh` 独立拉取至 `third_party/`，以保持主仓库轻量化与许可证边界清晰。

| 目录 | 对应上游 | 支持引擎 | 产物 | 详细规划 |
| :--- | :--- | :--- | :--- | :--- |
| [easyrpg/](easyrpg/) | [EasyRPG/Player](https://github.com/EasyRPG/Player) | RM 2000 / 2003 | `easyrpg.nro` | [easyrpg-plan.md](../docs/cores/easyrpg-plan.md) |
| [nxjs-mvz/](nxjs-mvz/) | [TooTallNate/nx.js](https://github.com/TooTallNate/nx.js) | RM MV / MZ | `nxjs-mvz.nro` | [nxjs-mvz-plan.md](../docs/cores/nxjs-mvz-plan.md) |
| [mkxp-z/](mkxp-z/) | [mkxp-z/mkxp-z](https://github.com/mkxp-z/mkxp-z) | RM XP / VX / VX Ace | `mkxp-z.nro` | [mkxp-z-plan.md](../docs/cores/mkxp-z-plan.md) |

## 统一核心约定

每个核心在构建为独立 `.nro` 后，均需满足 [docs/LAUNCH_PROTOCOL.md](../docs/LAUNCH_PROTOCOL.md) 定义的最小契约：
1. 识别 `--nxrpg-manifest <path>` 参数并解析 `launch.json`。
2. 支持独立存档目录 `game.savePath` 与回退资源（`assets.rtpPath`、`assets.fallbackFontPath`）。
3. 退出时通过 `envSetNextLoad(returnTo, "--from-core")` 平滑返回 `nx.rpg.nro`。
