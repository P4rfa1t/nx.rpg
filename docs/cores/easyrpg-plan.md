# EasyRPG Player 核心集成规划（RM 2000 / RM 2003）

- **上游仓库**：[EasyRPG/Player](https://github.com/EasyRPG/Player)（`master` 分支）
- **核心定位**：支持 RPG Maker 2000（`rm2k`）与 RPG Maker 2003（`rm2k3`）
- **输出产物**：`sdmc:/switch/nx.rpg/cores/easyrpg.nro`
- **许可证**：GPL-3.0
- **集成难度**：★☆☆☆☆（最低，作为 Phase 1 首个闭环验证核心）

---

## 1. 上游 Switch 支持现状

`EasyRPG/Player` 已具备成熟的 Nintendo Switch 平台后端：
- 平台适配代码位于上游 `src/platform/switch/`。
- 官方 `CMakePresets.json` 已内置 `switch-release` / `switch-debug` 预设，直接引用 `$DEVKITPRO/cmake/Switch.cmake`。
- 核心依赖（`liblcf`、`SDL2`、`libpng`、`zlib`、`pixman`、`freetype`、`harfbuzz`、`libogg`、`libvorbis`、`mpg123`、`speexdsp`、`wildmidi`/`fluidsynth` 等）在 devkitPro portlibs 及 EasyRPG buildscripts 生态中均已验证。

---

## 2. `nx.rpg` 所需的最小补丁集（`cores/easyrpg/patches/`）

保持上游代码改动极小化，计划仅维护以下轻量补丁：

1. **支持 `--nxrpg-manifest <path>` 启动参数**：
   - 启动入口解析 `launch.json`，将以下参数自动映射至 EasyRPG 内部启动选项：
     - `--project-path <game.contentRoot>`
     - `--save-path <game.savePath>`
     - `--encoding <runtime.encoding>`（当不为 `auto` 时生效）
     - RTP 搜索路径挂载 `assets.rtpPath`（以及全局回退 `sdmc:/switch/nx.rpg/rtp/rm2000` 或 `rm2003`）
2. **退出返回 Launcher**：
   - 当 EasyRPG 正常退出（玩家选“返回标题→退出”或触发 `exitCombo`）时，在释放 SDL/libnx 资源后调用 `envSetNextLoad(returnTo, "--from-core")`。
3. **异常日志重定向**：
   - 将 EasyRPG 的 `easyrpg_log.txt` 同步输出/镜像至 `manifest.logPath`，方便前端排查缺失 RTP 或编码错误。

---

## 3. 验收标准（Phase 1）

- [ ] 通过 `devkitpro/devkita64` 工具链无报错编出 `easyrpg.nro`。
- [ ] 从 `nx.rpg.nro` 选中一款 RM2000 与一款 RM2003 游戏，能够跳过 EasyRPG 自带文件浏览器直接进入游戏标题画面。
- [ ] 验证共享 RTP（`rtp/rm2000`、`rtp/rm2003`）生效且独立存档写入 `saves/<game_id>/`。
- [ ] 退出游戏后稳定自动回到 `nx.rpg.nro`，连续切换 5 次游戏无内存泄漏或崩溃。
