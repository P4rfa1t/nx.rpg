# mkxp-z 补丁队列精确规格

基于上游 `mkxp-z/mkxp-z` 提交 `a8f9d9ee331034b7778d0f6c3a5603de8b87388f`（v2.4.2-dev）源码审计制定：

---

## `0001-meson-horizon-host-and-nro-target.patch`

- **修改文件**：`meson.build`、`src/meson.build`、`binding/meson.build`
- **内容**：
  - 识别 `host_machine.system() == 'horizon'`：默认置 `use_gles_by_default = true`，跳过桌面专用动态库 `rpath` / `dl` / X11 / D-Bus 探测。
  - 支持链接预编译静态 `libruby-static.a` 及 `extinit.o`（见 `cores/mkxp-z/ruby-switch/`）。
  - 增加构建后处理步骤：调用 `nacptool` 与 `elf2nro`（挂载内置 `assets/` + `scripts/preload/` + `cores/mkxp-z/preload/` 至 RomFS）生成 `mkxp-z.nro`。

---

## `0002-switch-nxrpg-manifest-config-loader.patch`

- **修改文件**：`src/main.cpp`、`src/config.cpp`
- **内容**：
  - 在 `Config::read` 阶段解析 `--nxrpg-manifest <path>`，直接将 `launch.json` 映射为 `mkxp-z` 内部 `Config` 字段：
    - `gameFolder` ← `game.contentRoot`
    - `execName` ← `game.execName`
    - `rgssVersion` ← `runtime.rgssVersion`（1 / 2 / 3）
    - `RTP` ← `[assets.rtpPath]`
    - `preloadScript` ← 按序注入 `romfs:/preload/00_nxrpg_save_redirect.rb`、上游 `win32_wrap.rb` 与 `01_nxrpg_win32_ext.rb`
    - `fixedAspectRatio`、`integerScalingActive`、`vsync`、`displayFPS`、`smoothScaling`、`frameSkip`、`subImageFix`、`solidFonts`
  - 设置环境变量 `NXRPG_SAVE_DIR=<game.savePath>` 供预加载脚本执行存档重定向。

---

## `0003-switch-sdl2-gamepad-exitcombo-and-chainload.patch`

- **修改文件**：`src/main.cpp`、`src/eventthread.cpp`
- **内容**：
  - 初始化 `libnx` `romfsInit()`、可选 `clkrst` CPU 升频，以及退出时恢复频率。
  - 在 `EventThread` 中监听 Switch SDL2 `SDL_CONTROLLERBUTTONDOWN` 的 `exitCombo`（默认 `L + R + PLUS`），触发 RGSS 安全退出中断。
  - 在 OpenAL / SDL2 / PhysFS 释放完毕后调用 `envSetNextLoad(returnTo, "--from-core")`。

---

## `0004-switch-swkbd-text-input-binding.patch`（Phase 4）

- **修改文件**：`binding/input-binding.cpp`
- **内容**：暴露 `Input.nx_swkbd(prompt, default_text, max_len)` 可选辅助接口供需要文本输入的脚本调用。
