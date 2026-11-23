# launcher — nx.rpg 统一前端（`nx.rpg.nro`）

`launcher/` 负责构建运行在 Nintendo Switch 上的主界面程序 `nx.rpg.nro`。

## 核心职责

1. **运行环境自检**：检测 Switch 当前处于 Title Takeover（全内存模式）还是 Applet Mode（低内存模式），并检查 `cores/` 下三大核心 `.nro` 与 `rtp/` 资源是否就绪。
2. **游戏库扫描与管理**：扫描 `sdmc:/switch/nx.rpg/games/`，调用 `common/` 的引擎检测器自动归类 7 代 RPG Maker 游戏，提取标题与封面图。
3. **配置管理**：编辑全局设置（`config/global.json`）与单游戏覆写配置（`config/games/<game_id>.json`）。
4. **核心调度与错误回收**：生成 `runtime/launch.json` 并调用 `libnx` 的 `envSetNextLoad()` 启动对应核心；返回前端时检查 `runtime/last_run.log` 并在异常时展示排错信息。

## 选型说明

推荐基于 **C++ + Borealis（Switch 原生风格 UI 框架）** 构建，确保冷启动与切换延迟极低、内存占用可控（详见 [docs/ARCHITECTURE.md](../docs/ARCHITECTURE.md)）。
