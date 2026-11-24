# nx.rpg — 跨模型上下文交接摘要（LLM Context Handoff）

> **用途**：本文件是 `nx.rpg` 项目建仓与 Phase 0 深度规划会话的高密度压缩摘要，供后续任意大模型 / Coding Agent 直接读取并无缝接手开发，无需重复调研上游源码或重做架构决策。

---

## 1. 项目目标与当前状态快照

- **项目名称**：`nx.rpg`
- **远端仓库**：`https://github.com/P4rfa1t/nx.rpg.git`（当前为 **Private** 仓库，默认分支 `main`，最新提交 `bd06117`）
- **核心目标**：在 **Nintendo Switch**（`Horizon OS` / `libnx` / `devkitA64`，`aarch64-none-elf`）上构建一款统一前端运行器，整合三大开源核心以支持 **7 代 RPG Maker 游戏**：
  - `rm2k` (2000)、`rm2k3` (2003) → **EasyRPG Player** (`cores/easyrpg.nro`)
  - `rmmv` (MV)、`rmmz` (MZ) → **nx.js + MV/MZ Shim** (`cores/nxjs-mvz.nro`)
  - `rmxp` (XP / RGSS1)、`rmvx` (VX / RGSS2)、`rmvxa` (VX Ace / RGSS3) → **mkxp-z** (`cores/mkxp-z.nro`)
- **当前进度**：**Phase 1–4 代码全部完成**（`common/` 公共库 + 7 项宿主机 ctest、Borealis 前端 MVP 含封面缓存/收藏/游玩时长/诊断中心、三大核心补丁队列 easyrpg×3 / nxjs-mvz×1 / mkxp-z×4 全部打通，四枚 NRO 已在 Docker `nxrpg-devkita64` 交叉编译产出）。Phase 5 部分：`.github/workflows/build.yml` 四目标并行构建已上线。**剩余：实机（Switch 真机/模拟器）闭环验收、插件兼容性迭代、发布打包流水线**。
- **当前宿主机环境情况**（`scripts/check-env.sh` 实测）：Windows 11 + Git Bash；已安装 `git`、`cmake`、`ninja`、`node`、`python`；尚未安装 `devkitPro`（`DEVKITPRO`、`aarch64-none-elf-gcc`、`elf2nro`）、`meson`、`pnpm`。

---

## 2. 用户已拍板的不可变更架构决策（Confirmed Decisions）

1. **多 NRO 进程隔离链式加载（`envSetNextLoad`）**：
   - 严禁将 GPL-3.0（`EasyRPG`）、GPL-2.0（`mkxp-z`）与 MIT（`nx.js` / 自有代码）静态或动态链接进同一可执行文件。
   - 统一前端 `nx.rpg.nro` 与三个核心 `cores/*.nro` 作为独立进程运行，通过 `sdmc:/switch/nx.rpg/runtime/launch.json` 传递参数，退出时通过 `libnx` `envSetNextLoad("sdmc:/switch/nx.rpg/nx.rpg.nro", "... --from-core")` 自动返回前端，同时让当前运行核心独享全部可用内存。
2. **统一前端技术栈锁定为 `C++20 + Borealis`**：
   - 位于 `launcher/`，兼顾 Switch 原生系统级手柄/触屏 UI、`< 0.5s` 链式返回冷启动、极低内存占用，并支持在桌面端通过 GLFW/SDL2 直接编译预览 UI（`NXRPG_DESKTOP_PREVIEW`）。
3. **上游源码管理采用 `Lockfile 锁 SHA + 补丁队列`（非 Git Submodule）**：
   - `third_party/` 下的三大上游源码目录被 `.gitignore` 忽略，由 `third_party/upstreams.lock.json` 精确锁定 Commit SHA，通过 `scripts/bootstrap-cores.sh` 检出，所有改动以补丁形式按序维护在 `cores/<core>/patches/SERIES`。
4. **文档与沟通语言**：仓库设计文档与用户沟通统一采用**简体中文**（代码标识符、路径、枚举值保持标准英文）；AI 协作总规范以 `AGENTS.md` 为单一事实来源（`CLAUDE.md` 通过 `@AGENTS.md` 引用）。

---

## 3. 三大上游核心源码实测结论与关键避坑清单（Critical Ground Truth）

以下事实均经过对锁定 Commit 真实源码的逐文件核查，后续编码时**必须直接遵循，切勿凭旧印象猜测**：

### 3.1 EasyRPG Player（锁定 `master` @ `a99ca834b8560f6a39a4df5de2d3972523ba2f30`，GPL-3.0）
- **真实 Switch 后端**（`src/platform/switch/`）：**不使用 SDL2 或 `audren`**！图形层 `ui.cpp` 直接使用 **EGL / OpenGL + `glad`**（内嵌 `resources/switch/touch_ui.png` 掌机侧边栏）；音频层 `audio.cpp` 直接使用 **`libnx` `audout`**（48 kHz 立体声 S16 双缓冲线程）。
- **构建方式**：上游 `CMakePresets.json` 原生提供 `switch-release`（调用 `$DEVKITPRO/cmake/Switch.cmake` + `$EASYRPG_BUILDSCRIPTS/switch`，要求 `liblcf >= 0.8.1`，默认产出 `easyrpg-player.nro` → 安装重命名为 `cores/easyrpg.nro`）。
- **原生 CLI 能力**：支持 `--project-path`、`--save-path`、`--encoding`、`--font1`、`--font2`、`--log-file`（上游默认名为 `EasyRPG-Player.log`）、`--vsync`、`--show-fps`、`--stretch`、`--scaling`。
- **【致命坑位 1 — 自定义 RTP 路径失效】**：在 `src/filefinder_rtp.cpp` 中，`#ifdef PLAYER_NINTENDO` 分支在添加 `./rtp/<ver>` 与 `/switch/easyrpg-player/rtp/<ver>` 后**直接提前 `return`**，导致 `--rtp-path` 与 `RPG2K_RTP_PATH` / `RPG2K3_RTP_PATH` 在 Switch 上完全无效！必须通过补丁 `0002-switch-enable-custom-rtp-path.patch` 移除该提前返回，共享池 `sdmc:/switch/nx.rpg/rtp/{rm2000,rm2003}` 才能生效。
- **【补丁 0001】**：上游 `src/platform/switch/main.cpp` 无 `--nxrpg-manifest` 解析且退出时不调 `envSetNextLoad`，需在 `0001-switch-nxrpg-manifest-and-chainload.patch` 中补齐并接入 `L+R+PLUS` 退出组合键。

### 3.2 TooTallNate/nx.js（锁定 `main` @ `462203af39179fb630ae7fe981f3361b7147cdfb`，MIT）
- **运行时底座**：使用 **V8 JavaScript 引擎 + Skia GPU (Canvas2D) + 真 GLES3 (WebGL2) + Web Audio / FFmpeg (`audren`) + 原生 WebAssembly**（非 QuickJS！）。
- **【强制依赖版本】**：必须链接 **`switch-v8 >= 15.0.243-9`**（修复了 Horizon W^X 全内存 JIT GC 写只读代码页触发的 Data Abort；Application 模式下 V8 对象堆上限已提升至 `512 MiB`，且 `ArrayBuffer`/`Uint8Array` 独立分配自数 GiB 的 `newlib` 堆）。在相册 **Applet 模式**（~440 MB）下强制退化为 `jitless`（堆约 `89 MiB`，MZ 的 `effekseer.wasm` 受限），故 Launcher 必须对 Applet 模式启动 MV/MZ 弹出强警告。
- **【致命坑位 2 — 严禁传 `argv[1]`】**：`packages/runtime/src/switch/index.ts` 会将 `Switch.argv[1]` 解析为**外部入口 `.js`/`.nro` 路径**覆盖内置 `romfs:/main.js`！因此 `Chainloader::format_core_argv` 对 `nxjs-mvz.nro` **绝对不能附带 `--nxrpg-manifest` 位置参数**；必须由内置 `romfs:/main.js` 直接读取固定路径 `sdmc:/switch/nx.rpg/runtime/launch.json`。
- **【致命坑位 3 — 默认 `--slim` 打包】**：`@nx.js/nro` 新版默认打 `--slim` 包（依赖外部 `sdmc:/nx.js/nxjs-v*.nro`）。`cores/nxjs-mvz/package.json` 必须固定 `NXJS_FAT=1 nxjs-nro --fat` 生成自包含单文件 `cores/nxjs-mvz.nro`。
- **【致命坑位 4 — 系统选人弹窗与 NAND 存档】**：`nx.js` 内置 `localStorage`（`storage.ts`）在 NACP `userAccountSaveDataSize > 0` 时会强制弹出 Switch 系统选人框并写入主机 NAND。必须在 `package.json` 保持 `"userAccountSaveDataSize": 0`，由 `shim/src/storage/` 将 `localStorage` 和 NW.js `require('fs')` 存档统一原子写入 `sdmc:/switch/nx.rpg/saves/<game_id>/`。
- **【致命坑位 5 — `screen` 单一上下文锁】**：`packages/runtime/src/screen.ts` 的全局 `screen`（`1280×720`）只允许获取 `'2d'` **或** `'webgl2'` 之一，且不认识 `'webgl'`。`shim/src/dom/` 必须：① 启动前禁止用户态代码先占 `'2d'`；② 将 PixiJS v4 请求的 `'webgl'`/`'experimental-webgl'` 桥接至 `screen.getContext('webgl2')`；③ 离屏 `Bitmap` 统一走 `OffscreenCanvas` / `Canvas`。
- **【补丁 0001】**：上游 `source/applet.cc` 未绑定 `envSetNextLoad`，需通过 `0001-applet-bind-env-set-next-load.patch` 导出 `Switch.setNextLoad(path, argv)`。CPU 调频直接复用官方 `@nx.js/clkrst` 包。

### 3.3 mkxp-z（锁定 `dev` @ `a8f9d9ee331034b7778d0f6c3a5603de8b87388f`，v2.4.2，GPL-2.0）
- **构建与许可证约束**：要求 `meson >= 1.4.0`；交叉配置见 `cores/mkxp-z/cross/switch-aarch64.ini`（`gfx_backend=gles`, `aarch64_neon=enabled`, `use_miniffi=false`, `openssl=disabled`, `extra_shaders=disabled` 以维持纯 GPL-2.0）。
- **直接复用的三大上游内置特性（勿重复造轮子）**：
  1. `"pathCache": true`：上游默认建立全量小写资源路径索引，已原生解决大小写敏感问题。
  2. `"execName"`：当同人游戏把 `Game.ini`/`Game.rgss3a` 改名为 `<Stem>.ini`/`<Stem>.rgss3a` 时，直接把检测到的 `game.execName` 注入 `mkxp-z` `Config::execName`。
  3. `scripts/preload/win32_wrap.rb`（CC0）：上游已实现跨平台 `User32` 键盘状态、鼠标坐标、窗口尺寸与容错（`TOLERATE_ERRORS = true`）；我们的 `cores/mkxp-z/preload/01_nxrpg_win32_ext.rb` 仅在其之上补充 `Kernel32` INI 读写等扩展。
- **【关键补充 — RGSS 存档目录重定向】**：RGSS1/2/3 游戏默认通过 `File.open("Save01.rvdata2", "wb")` 直接写游戏根目录。必须注入 `cores/mkxp-z/preload/00_nxrpg_save_redirect.rb`，读取 `ENV['NXRPG_SAVE_DIR']` 将存档读写拦截至 `saves/<game_id>/`（含 Copy-on-Write 旧档回退）。
- **静态 Ruby MRI 3.1 方案**：见 `cores/mkxp-z/ruby-switch/`（`--host=aarch64-none-elf --with-coroutine=arm64 --disable-shared --with-static-linked-ext --disable-dln --disable-jit-support --disable-yjit`），静态链接 `zlib`（解压 `Scripts.rxdata/rvdata(2)` 必需）、`stringio`、`fiber` 及全套 CJK 编码表（`shift_jis`/`windows_31j`/`gbk`/`gb18030`/`big5`/`cp949`）。

---

## 4. Switch 平台级全局不变量

1. **防 exFAT 掉电损坏铁律**：
   - `sdmc:/switch/nx.rpg/games/<folder>/` 运行期严格视为**只读**。
   - 所有配置、缓存（`library.json`）、启动清单（`launch.json`）与隔离存档落盘，必须执行 **`<file>.tmp` 写入 → `fflush` + `fsync` → `rename` 原子替换**。
2. **TitleID 与图标硬指标**：
   - TitleID 分配：`nx.rpg.nro` = `016e787270670000`，`nxjs-mvz.nro` = `016e787270670001`（`userAccountSaveDataSize=0`），`easyrpg.nro` = `016e787270670002`，`mkxp-z.nro` = `016e787270670003`。
   - NRO 图标必须为 **`256 × 256` 标准 Baseline DCT JPEG**（严禁 Progressive JPEG 与 EXIF 缩略图）。
3. **三层配置合并与增量缓存**：
   - 优先级：`内置默认` → `config/global.json` → `config/games/<id>.json` → 生成 `runtime/launch.json`。
   - 前端扫描结果缓存于 `cache/library.json`（比对目录 `dirMtime` 增量刷新），封面缩略图缓存在 `cache/covers/<id>.jpg`，确保 `--from-core` 返回 `< 50ms` 恢复原焦点。

---

## 5. 仓库关键文件地图

| 模块路径 | 内容与职责 |
| :--- | :--- |
| `AGENTS.md` / `CLAUDE.md` | 仓库最高优先级工程守则与工作流约束 |
| `third_party/upstreams.lock.json` | 三大上游仓库 URL、分支、40 位 Commit SHA 与补丁队列绑定 |
| `common/schemas/*.schema.json` | 5 份权威契约：`global-config`、`game-config`、`launch-manifest`、`library-cache`、`upstreams-lock` |
| `common/include/nxrpg/*.hpp` | C++20 公共库头文件骨架：`types`、`paths`、`engine_detector`、`launch_manifest`、`config_merger`、`chainload` |
| `launcher/` | C++20 + Borealis 前端骨架：`CMakeLists.txt`、`npdm.json`、`include/launcher/{app,services/*,views/*}`、`resources/i18n/` |
| `cores/easyrpg/` | `cmake/nxrpg-easyrpg-flags.cmake`、`patches/{SERIES,README.md}` |
| `cores/nxjs-mvz/` | `package.json` (`--fat`)、`tsconfig.json`、`shim/src/{index,types,dom,vfs,crypto,nwjs,storage,input,perf}`、`patches/` |
| `cores/mkxp-z/` | `cross/switch-aarch64.ini`、`ruby-switch/{README.md,ext-static.list}`、`preload/*.rb`、`patches/` |
| `docs/` | `ARCHITECTURE.md`、`ENGINE_DETECTION.md`、`LAUNCH_PROTOCOL.md`、`INPUT_AND_UI_SPEC.md`、`FILESYSTEM_AND_SAVES.md`、`BUILD_AND_CI.md`、`ROADMAP.md`、`cores/*-plan.md` |
| `tests/` | 宿主机无硬件 CMake 单测骨架与 7 代零版权合成测试夹具规范 |
| `scripts/` | `validate-repo.py`（全仓契约校验）、`bootstrap-cores.sh`（锁 SHA 检出）、`apply-patches.sh`、`check-env.sh` |

---

## 6. 接手开发的第一步指令（Next Agent Entrypoint）

接手本仓库的任意模型应首先运行契约校验确认基线完好：

```bash
python scripts/validate-repo.py
bash -n scripts/check-env.sh scripts/bootstrap-cores.sh scripts/apply-patches.sh
```

随后按 `docs/ROADMAP.md` 进入 **Phase 1**，推荐严格按以下顺序推进：
1. **Phase 1a（纯宿主机可编译验证，不依赖 devkitPro）**：在 `common/src/` 实现 `types.cpp`、`paths.cpp`、`engine_detector.cpp`、`launch_manifest.cpp`、`config_merger.cpp`、`chainload.cpp`，并在 `tests/` 编写覆盖 7 代合成夹具、`www/` 剥离、重命名 `execName` 与 `nxjs-mvz` 无 `argv[1]` 规则的 `ctest` 单元测试。
2. **Phase 1b（Borealis 前端 MVP）**：在 `launcher/src/` 实现游戏扫描列表、Applet 内存告警、单游戏编码/缩放配置与 `launch.json` 调度。
3. **Phase 1c（EasyRPG 闭环）**：执行 `scripts/bootstrap-cores.sh easyrpg`，编写并挂载 `cores/easyrpg/patches/` 下的 `0001` 与 `0002` 补丁，在 `devkitpro/devkita64` 环境下编出 `nx.rpg.nro` 与 `cores/easyrpg.nro` 完成实机/模拟器往返验证。
