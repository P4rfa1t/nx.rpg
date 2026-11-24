# EasyRPG Player 补丁队列精确规格

基于上游 `EasyRPG/Player` 提交 `a99ca834b8560f6a39a4df5de2d3972523ba2f30` 源码审计，制定以下 3 个外科手术式补丁：

---

## `0001-switch-nxrpg-manifest-and-chainload.patch`

- **修改文件**：`src/platform/switch/main.cpp`
- **现状问题**：
  - 上游 `main.cpp` 仅检查 `romfs:/RPG_RT.lmt` 并调用 `Player::Init` / `Player::Run`，既不识别 `--nxrpg-manifest`，退出时在 `atexit` 清理完 `romfsExit()` / `socketExit()` / `appletUnlockExit()` 后也不会调用 `envSetNextLoad()`。
- **修改内容**：
  1. 在 `Player::Init` 之前检测 `--nxrpg-manifest <path>`，读取 `launch.json` 并拼装上游原生支持的 CLI 参数：
     - `--project-path <game.contentRoot>`
     - `--save-path <game.savePath>`
     - `--encoding <runtime.encoding>`（非 `auto` 时注入）
     - `--rtp-path <assets.rtpPath>`（非空时注入）
     - `--font1 <assets.fallbackFontPath>`（非空时注入）
     - `--log-file <logPath>`（替代上游默认的 `EasyRPG-Player.log`）
     - 视频参数：`--vsync` / `--no-vsync`、`--show-fps` / `--no-show-fps`、`--stretch` / `--no-stretch`
  2. 根据 `runtime.cpuProfile` 可选调用 `clkrst` / `pcv` 设置 Switch CPU 频率，并在退出清理阶段恢复 `1020 MHz`。
  3. 在 `atexit` 资源释放末尾（`appletUnlockExit()` 前）若 `manifest.returnTo` 存在，调用：
     ```cpp
     envSetNextLoad(return_to.c_str(), (return_to + " --from-core").c_str());
     ```
  4. 增加全局 `exitCombo`（默认 `L + R + PLUS`）轮询钩子，置位 `Player::exit_flag = true` 以触发安全退出。

---

## `0002-switch-enable-custom-rtp-path.patch`（关键修复）

- **修改文件**：`src/filefinder_rtp.cpp`
- **现状问题（源码审计发现）**：
  - 尽管 `src/player.cpp` 解析了 `--rtp-path <path>`，但在 `src/filefinder_rtp.cpp` 中，`#ifdef PLAYER_NINTENDO` 分支在挂载完 `./rtp/<version>` 和 `/switch/easyrpg-player/rtp/<version>` 后**提前 `return`**，导致 `--rtp-path` 命令行参数以及 `RPG2K_RTP_PATH` / `RPG2K3_RTP_PATH` 环境变量在 Switch 上完全不生效！
- **修改内容**：
  - 移除或调整 `PLAYER_NINTENDO` 的提前 `return`，允许 `Player::rtp_path`（即 `--rtp-path` 传入的 `sdmc:/switch/nx.rpg/rtp/rm2000` 或 `rm2003`）优先或追加进入 RTP 搜索列表，同时保留 `/switch/easyrpg-player/rtp/<version>` 作为兼容回退。

---

## `0003-switch-swkbd-name-input.patch`（Phase 4 体验增强）

- **修改文件**：`src/platform/switch/ui.cpp`、`src/window_keyboard.cpp`（可选挂钩）
- **修改内容**：
  - 当进入主角起名场景且 `runtime.enableSoftwareKeyboard == true` 时，提供按钮（如按下 `X` 或点击虚拟键盘唤起键）调用 `libnx` 原生 `swkbdCreate` / `swkbdShow`，支持直接输入中文/日文姓名并回填至 `Window_Keyboard`。
