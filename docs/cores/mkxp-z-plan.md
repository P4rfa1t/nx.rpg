# mkxp-z 核心移植与集成规划（RPG Maker XP / VX / VX Ace）

- **上游仓库**：[mkxp-z/mkxp-z](https://github.com/mkxp-z/mkxp-z)（`dev` 分支）
- **核心定位**：支持 RPG Maker XP（RGSS1）、RPG Maker VX（RGSS2）、RPG Maker VX Ace（RGSS3）
- **输出产物**：`sdmc:/switch/nx.rpg/cores/mkxp-z.nro`
- **许可证**：GPL-2.0（如启用 `extra_shaders` 选项则为 GPL-3.0）
- **集成难度**：★★★★★（最高 — 需完成 `devkitA64` Meson 交叉编译、静态 Ruby MRI 移植与平台适配）

---

## 1. 技术挑战分析

相比 `EasyRPG/Player` 与 `TooTallNate/nx.js`，`mkxp-z` 上游目前主要面向 Windows / macOS / Linux，**没有开箱即用的 Nintendo Switch 构建目标**。移植的主要技术难点集中在四个方面：

1. **静态 Ruby MRI 3.1 交叉编译至 `aarch64-none-elf`（devkitA64 / newlib）**
   - `mkxp-z` 通过 C/C++ 绑定嵌入 Ruby MRI（默认 `mri_version=3.1`）以完整兼容 RGSS1/2/3 的 Ruby 脚本语法与 Marshal 反序列化（`.rxdata` / `.rvdata` / `.rvdata2`）。
   - Switch 的 `newlib` 缺少 `fork()`、`exec()`、`dlopen()`（动态 `.so` 扩展）、完整信号处理等桌面 POSIX 特性。
   - **对策**：交叉编译静态 `libruby.a`，关闭非 RGSS 必需的原生扩展（保留 `zlib`、`stringio`、`enc` 等 RGSS 关键扩展），并确保 `aarch64` 协程（`coroutine/arm64`）与 `pthread`（libnx 支持）正常工作。

2. **图形渲染后端适配（OpenGL ES 2 + EGL）**
   - `mkxp-z` 已在 `meson_options.txt` 中支持 `gfx_backend=gles` 与 `aarch64_neon=enabled`！
   - Switch 通过 `switch-mesa` / `switch-glad` / `switch-sdl2` 提供硬件加速 EGL + OpenGL ES 2/3 上下文。
   - 需验证 `mkxp-z` 内部着色器（`shader/`）在 GLES2/Mesa Nouveau 驱动下的编译兼容性（如精度限定符 `precision mediump float;`、FBO 格式与 NPOT 纹理限制）。

3. **依赖库矩阵对齐（`devkitPro/portlibs/switch`）**

   | 依赖组件 | devkitPro Portlib / 解决方式 | 说明 |
   | :--- | :--- | :--- |
   | **SDL2** | `switch-sdl2` | 窗口、EGL 上下文、手柄与事件循环 |
   | **SDL2_image / SDL2_ttf** | `switch-sdl2_image`、`switch-sdl2_ttf` | 图像与字体渲染 |
   | **FreeType / Pixman / zlib** | `switch-freetype`、`switch-pixman`、`switch-zlib` | 位图缩放与字形渲染 |
   | **OpenAL Soft** | `switch-openal-soft` | BGM/BGS/ME/SE 音频混音 |
   | **Ogg / Vorbis / FluidSynth** | `switch-libogg`、`switch-libvorbis`、`switch-fluidsynth`（可选） | 音频解码与 MIDI 回放 |
   | **PhysFS** | `switch-physfs`（或 Meson subproject 静态编译） | 虚拟文件系统与 `.rgssad`/`.rgss2a`/`.rgss3a` 挂载 |
   | **SDL_sound / libsigc++ / uchardet / xxHash / fmt** | 通过 `meson` subprojects 或自建 Switch 静态库补齐 | 交叉编译难度低，纯 C/C++ 库 |
   | **OpenSSL / Steamworks / MiniFFI / X11 / Wayland** | **全部禁用**（`openssl=disabled`, `use_miniffi=false`） | Switch 平台不适用 |

4. **Win32API 脚本兼容性（`use_miniffi=false`）**
   - 大量社区 RPG Maker XP/VX/VXAce 游戏在 RGSS 脚本中调用 `Win32API.new('user32', ...)` 或 `kernel32`（例如检测键盘状态、获取屏幕分辨率、弹窗提示、INI 读写）。
   - Switch（ARM64 / Horizon OS）无法执行 x86 Windows DLL。
   - **对策**：在 Ruby 预加载脚本层（`preloadScript`）注入可控的 **Win32API 兼容垫片（Stub）**，对高频无害 API 返回合理模拟值（如将 `GetAsyncKeyState` 映射至 Switch 手柄输入、将 `GetPrivateProfileString` 映射至纯 Ruby INI 读取），从而救活大量非重度依赖原生 DLL 的同人游戏。

---

## 2. Meson 交叉编译策略

项目已在 [cores/mkxp-z/cross/switch-aarch64.ini](../../cores/mkxp-z/cross/switch-aarch64.ini) 预留 Meson 交叉编译配置文件骨架，核心编译选项规划如下：

```ini
# 关键 Meson 构建参数预览
-Ddefault_system=auto
-Dgfx_backend=gles
-Daarch64_neon=enabled
-Duse_miniffi=false
-Dopenssl=disabled
-Dalsa=disabled
-Dpulseaudio=disabled
-Dpipewire=disabled
-Djack=disabled
-Doss=disabled
-Dcjk_fallback_font=true
```

---

## 3. `nx.rpg` 专属适配清单（`cores/mkxp-z/patches/`）

1. **启动清单集成**：解析 `--nxrpg-manifest` 并将其动态转换为 `mkxp-z` 的内部 `Config` 对象（自动填充 `gameFolder`、`rgssVersion` 1/2/3、`RTP` 列表、`savePath`、`solidFonts` 与 `fallbackFont`）。
2. **Switch 路径与只读/读写分离**：通过 `PhysFS` 挂载游戏目录（含加密归档）与共享 `rtp/rmxp|rmvx|rmvxa`，并将存档写入重定向至 `saves/<game_id>/`。
3. **输入与退出流**：默认将 Switch Joy-Con / Pro 手柄映射至 RGSS 标准按键（`Input::C`/`B`/`A`/`X`/`Y`/`L`/`R`），支持 `exitCombo` 安全释放 Ruby VM + OpenAL + SDL2 并 `envSetNextLoad` 返回 `nx.rpg.nro`。

---

## 4. 分步攻关计划（Phase 3 子阶段）

- **Phase 3a（工具链与 Ruby 静态库）**：在 `devkitpro/devkita64` 容器内成功编出 `libruby.a` 并通过最小 Switch `.nro` 执行 `puts "Hello RGSS"` 验证。
- **Phase 3b（依赖补齐与链接出包）**：补全 Meson 依赖并成功链接产出 `mkxp-z.nro`（能初始化 SDL2 + EGL/GLES2 + OpenAL）。
- **Phase 3c（三代 RGSS 实机验证）**：分别跑通 RPG Maker XP / VX / VX Ace 测试工程，验证 CJK 字体、加密归档与存读档。
- **Phase 3d（Win32API Stub 与性能调优）**：补齐常用 Win32API Ruby 垫片并优化大图集内存占用。
