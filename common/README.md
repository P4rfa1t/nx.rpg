# common — 跨模块共享协议与基础库

本目录存放 `nx.rpg` 前端（`launcher/`）与各核心适配层（`cores/`）共享的数据契约与通用组件：

- `schemas/launch-manifest.schema.json`：Launcher → Core 启动清单（`runtime/launch.json`）的正式 JSON Schema。
- `schemas/game-config.schema.json`：单游戏覆写配置（`config/games/<game_id>.json`）的正式 JSON Schema。

## 后续实现规划（Phase 1）

- `detector/`：C++ 实现的 7 代 RPG Maker 引擎自动检测器与元数据提取器（见 [docs/ENGINE_DETECTION.md](../docs/ENGINE_DETECTION.md)）。
- `manifest/`：轻量 C++ / TypeScript 共享解析器，用于校验与读写 `launch.json`。
