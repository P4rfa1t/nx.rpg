# EasyRPG Player 核心集成规划（RM 2000 / RM 2003）

- **上游锁定提交**：`EasyRPG/Player` @ `a99ca834b8560f6a39a4df5de2d3972523ba2f30`（`master`）
- **核心定位**：支持 RPG Maker 2000（`rm2k`）与 RPG Maker 2003（`rm2k3`）
- **输出产物**：`sdmc:/switch/nx.rpg/cores/easyrpg.nro`（由上游目标 `easyrpg-player.nro` 重命名安装）
- **许可证**：GPL-3.0-or-later
- **集成难度**：★☆☆☆☆（Phase 1 首个端到端闭环核心）

---

## 1. 上游 Switch 后端真实机制（经源码核实）

经对上游 `src/platform/switch/` 与 `CMakeLists.txt` 的逐文件审计：

- **非 SDL2 平台层**：EasyRPG 在 Switch 上使用的是专属原生后端（`audio.cpp`, `input_buttons.cpp`, `main.cpp`, `ui.cpp`）：
  - **渲染**：`ui.cpp` 直接初始化 **EGL / OpenGL + `glad`**，全屏运行，自动处理底座/掌机模式分辨率切换与焦点暂停，并内嵌 `resources/switch/touch_ui.png` 提供掌机侧边触控键。
  - **音频**：`audio.cpp` 直接调用 `libnx` **`audout`**（48 kHz 立体声有符号 16-bit 双缓冲独立工作线程）。
- **原生支持的 CLI 参数**：`src/player.cpp` 与 `src/game_config.cpp` 已原生支持 `--project-path`、`--save-path`、`--encoding`、`--font1`、`--font2`、`--log-file`（默认文件名为 `EasyRPG-Player.log`）、`--vsync`、`--show-fps`、`--stretch`、`--scaling`。
- **关键上游陷阱（`src/filefinder_rtp.cpp`）**：
  - 在当前锁定提交中，`src/filefinder_rtp.cpp` 的 `#ifdef PLAYER_NINTENDO` 分支仅硬编码检查 `./rtp/<version>` 与 `/switch/easyrpg-player/rtp/<version>` 并**直接提前 `return`**，导致命令行 `--rtp-path` 与环境变量 `RPG2K_RTP_PATH` / `RPG2K3_RTP_PATH` 在 Switch 上完全被忽略！

---

## 2. 精确补丁队列（`cores/easyrpg/patches/`）

详见 [cores/easyrpg/patches/README.md](../../cores/easyrpg/patches/README.md)：

1. **`0001-switch-nxrpg-manifest-and-chainload.patch`**（`src/platform/switch/main.cpp`）
   - 解析 `--nxrpg-manifest <path>`，转换为 `--project-path`、`--save-path`、`--encoding`、`--rtp-path`、`--font1`、`--log-file` 等内部参数。
   - 按 `runtime.cpuProfile` 可选调用 `clkrst` 升频，退出时复位。
   - 监听 `input.exitCombo` 置位安全退出标志，并在 `atexit` 末尾调用 `envSetNextLoad(returnTo, "... --from-core")`。
2. **`0002-switch-enable-custom-rtp-path.patch`**（`src/filefinder_rtp.cpp`）
   - 修复 `PLAYER_NINTENDO` 分支提前返回问题，使 `--rtp-path sdmc:/switch/nx.rpg/rtp/rm2000|rm2003` 在 Switch 上正常挂载。
3. **`0003-switch-swkbd-name-input.patch`**（`src/platform/switch/ui.cpp`，Phase 4）
   - 为主角起名界面接入 `libnx` `swkbd` 系统中文/日文输入法。

---

## 3. 验收标准（Phase 1）

- [ ] 通过 `cmake --preset switch-release -C cores/easyrpg/cmake/nxrpg-easyrpg-flags.cmake` 无报错产出 `easyrpg.nro`。
- [ ] 从 Borealis 前端启动 RM2000 与 RM2003 游戏，直接跳过 EasyRPG 内置启动目录列表进入游戏标题。
- [ ] 验证 `0002` 补丁生效：不拷贝 RTP 进游戏目录，仅依靠 `sdmc:/switch/nx.rpg/rtp/rm2000` 和 `rm2003` 即可无缺图缺音效运行。
- [ ] 验证存档落入 `saves/<game_id>/Save01.lsd`，按下 `L + R + PLUS` 稳定返回 `nx.rpg.nro` 并恢复卡片焦点。
