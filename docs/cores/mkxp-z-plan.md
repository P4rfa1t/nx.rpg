# mkxp-z 核心移植与集成规划（RPG Maker XP / VX / VX Ace）

- **上游锁定提交**：`mkxp-z/mkxp-z` @ `a8f9d9ee331034b7778d0f6c3a5603de8b87388f`（`dev`，v2.4.2，要求 `meson >= 1.4.0`）
- **核心定位**：支持 RPG Maker XP（RGSS1）、RPG Maker VX（RGSS2）、RPG Maker VX Ace（RGSS3）
- **输出产物**：`sdmc:/switch/nx.rpg/cores/mkxp-z.nro`
- **许可证**：GPL-2.0-or-later（默认关闭 `openssl` 与 `extra_shaders`，保持纯正 GPL-2.0 兼容性）
- **集成难度**：★★★★★（Phase 3 重点攻坚核心）

---

## 1. 上游源码核实的重要利好与关键触点

通过对上游 `meson.build`、`mkxp.json` 与 `scripts/preload/` 的源码核查，确认以下上游已有能力可直接复用，大幅缩减重复造轮子工作量：

1. **内置大小写无关路径缓存（`pathCache: true`）**：
   - 上游默认开启 `pathCache`，启动时自动索引游戏目录与加密包内所有资源的小写路径，天然解决 Windows 游戏在 Switch 上的大小写崩溃问题。
2. **内置重命名归档茎支持（`execName`）**：
   - 当游戏将 `Game.ini` / `Game.rgss3a` 改名为 `<Stem>.ini` / `<Stem>.rgss3a` 时，无需重命名用户文件，只需把 `launch.json -> game.execName` 赋给 `mkxp-z` 的 `Config::execName`。
3. **内置跨平台 `Win32API` 预加载实现（`scripts/preload/win32_wrap.rb`，CC0）**：
   - 上游已提供完整的 `User32` 键盘状态（`GetKeyState` / `GetAsyncKeyState` / `GetKeyboardState`）、鼠标坐标（`GetCursorPos`）、窗口矩形（`GetClientRect` / `FindWindowA`）以及未实现 API 容错开关（`TOLERATE_ERRORS = true`）！
   - 我们只需在 [01_nxrpg_win32_ext.rb](../../cores/mkxp-z/preload/01_nxrpg_win32_ext.rb) 中叠加 `Kernel32` 的 INI 读写（`GetPrivateProfileStringA` / `WritePrivateProfileStringA`）等高频同人脚本接口即可。
4. **RGSS 存档目录隔离（关键补充）**：
   - 与 EasyRPG 不同，`mkxp-z` 的 `dataPathOrg`/`dataPathApp` 仅存放键位设置，而 RGSS1/2/3 游戏脚本默认通过 Ruby `File.open("Save01.rvdata2", "wb")` 直接写往游戏本体目录！
   - 因此我们专门设计了预加载脚本 [00_nxrpg_save_redirect.rb](../../cores/mkxp-z/preload/00_nxrpg_save_redirect.rb)，通过 `ENV['NXRPG_SAVE_DIR']` 将存档读写透明重定向到 `sdmc:/switch/nx.rpg/saves/<game_id>/`（含原子写与旧档 Copy-on-Write 回退）。

---

## 2. 静态 Ruby MRI 3.1 与依赖构建方案

详见 [cores/mkxp-z/ruby-switch/README.md](../../cores/mkxp-z/ruby-switch/README.md) 与 [ext-static.list](../../cores/mkxp-z/ruby-switch/ext-static.list)：

- **Ruby 交叉配置**：采用 `--host=aarch64-none-elf --with-coroutine=arm64 --disable-shared --with-static-linked-ext --disable-dln --disable-jit-support --disable-yjit`，将 `zlib`（解压 `Scripts.rxdata/rvdata/rvdata2` 必需）、`stringio`、`fiber` 以及完整 CJK 编码表（`shift_jis` / `windows_31j` / `gbk` / `gb18030` / `big5` / `cp949`）静态编译进 `libruby-static.a`。
- **图形与音频**：使用 `devkitPro/portlibs/switch` 中的 `switch-sdl2`、`switch-sdl2_image`、`switch-sdl2_ttf`、`switch-mesa`（EGL + OpenGL ES 2，`gfx_backend=gles`）、`switch-openal-soft`、`switch-physfs`、`switch-pixman`、`switch-libogg`、`switch-libvorbis`；轻量辅助库（`uchardet`、`xxhash`、`SDL_sound`）通过 Meson subprojects 静态编译。

---

## 3. 分步攻关里程碑（Phase 3a ~ 3d）

- **Phase 3a**：在 `devkitpro/devkita64` 容器内跑通静态 `libruby-static.a` 编译，写最小 Switch NRO 验证 Ruby 3.1 VM 初始化、`Marshal.load`、`Zlib::Inflate`、ARM64 `Fiber` 与 CJK `String#encode`。
- **Phase 3b**：应用 `0001` 补丁与 [switch-aarch64.ini](../../cores/mkxp-z/cross/switch-aarch64.ini)，成功链接产出 `mkxp-z.nro` 并点亮 SDL2 + GLES2 渲染上下文与 OpenAL 音频输出。
- **Phase 3c**：应用 `0002` / `0003` 补丁及两枚预加载 Ruby 脚本，分别跑通 RMXP（`.rgssad`）、RMVX（`.rgss2a`）、RMVXAce（`.rgss3a`）工程，验证存档隔离与回跳 Launcher。
- **Phase 3d**：针对主流同人游戏扩充 `01_nxrpg_win32_ext.rb`、调优大图集纹理回收与 `clkrst` CPU 超频档位。
