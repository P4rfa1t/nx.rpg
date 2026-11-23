# cores/easyrpg — EasyRPG Player 核心适配层

- **上游源码路径**：`third_party/EasyRPG-Player`
- **构建系统**：CMake（上游官方 Preset：`switch-release`）
- **目标引擎**：RPG Maker 2000 / RPG Maker 2003
- **规划文档**：[docs/cores/easyrpg-plan.md](../../docs/cores/easyrpg-plan.md)

## 目录用途

- `patches/`：针对 `EasyRPG/Player` 的最小补丁队列（`--nxrpg-manifest` 启动参数解析、退出时调用 `envSetNextLoad` 返回 Launcher、日志同步）。
