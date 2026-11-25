# nx.rpg 系统架构设计

本文档定义 `nx.rpg` 在 Nintendo Switch（Horizon OS / `libnx`）上的整体系统架构、进程模型、内存与 CPU 调频策略以及 SD 卡目录规范。

---

## 1. 设计目标

1. **一站式即拷即玩**：用户只需将任意世代的 RPG Maker 游戏目录放入 `sdmc:/switch/nx.rpg/games/`，前端即可自动识别引擎版本并调度对应核心运行。
2. **锁定基线 + 补丁队列低侵入集成**：通过 [third_party/upstreams.lock.json](../third_party/upstreams.lock.json) 精确锁定三大成熟开源核心的 Commit SHA，所有 Switch 专属改动收敛在 `cores/<core>/patches/` 与独立垫片层中。
3. **Switch 原生交互体验**：统一前端确定采用 **C++20 + Borealis** 构建，全面支持 Joy-Con / Pro 手柄、掌机触屏（含双指取消）、系统软键盘（`swkbd` 角色起名）、CPU 按需超频（`clkrst`）与 CJK 中日韩字体回退。
4. **严格内存、存档与许可证隔离**：采用独立可执行 `.nro` 链式跳转架构，游戏本体目录只读、存档独立原子落盘。

---

## 2. 总体拓扑：Borealis 前端 + 三核心 NRO 链式加载

Switch Homebrew 环境（Atmosphère `hbmenu` / `nro` loader）提供 `libnx` 标准接口 `envSetNextLoad(const char* path, const char* argv)`，允许当前 `.nro` 在退出后由 loader 立即加载下一个 `.nro`，且完全释放前一个进程的堆内存与 GPU 上下文。

### 运行时控制流

```text
[用户从 Title Takeover 启动 nx.rpg.nro (C++20 + Borealis)]
        │
        ▼
┌────────────────────────────────────────────────────────┐
│  Launcher (nx.rpg.nro)                                 │
│  1. 检测 Application 全内存模式 vs Applet 低内存模式   │
│  2. 读取 cache/library.json 并按目录 mtime 增量扫描    │
│  3. 合并默认 -> global.json -> config/games/<id>.json  │
│  4. 原子写入 runtime/launch.json                       │
└──────────────────────────┬─────────────────────────────┘
                           │ 调用 envSetNextLoad() 并退出释放全部 RAM/VRAM
                           ▼
┌────────────────────────────────────────────────────────┐
│  Core NRO (easyrpg.nro / nxjs-mvz.nro / mkxp-z.nro)    │
│  1. 读取 runtime/launch.json，按需应用 clkrst CPU 频率 │
│  2. 只读访问游戏目录，挂载共享 RTP、字体与隔离存档目录 │
│  3. 运行游戏主循环；监听游戏内退出或 L+R+PLUS 组合键   │
│  4. 原子冲刷存档与 runtime/last_run.log，恢复默认频率  │
└──────────────────────────┬─────────────────────────────┘
                           │ envSetNextLoad(".../nx.rpg.nro", "... --from-core")
                           ▼
             [0.5s 内返回 Borealis 前端并恢复原卡片焦点]
```

> **重要细节（`nx.js` 参数特殊性）**：`TooTallNate/nx.js` 会将 `Switch.argv[1]` 解析为入口 JS 脚本覆盖路径（而非普通参数标志）。因此 `Chainloader` 在跳转 `easyrpg.nro` 与 `mkxp-z.nro` 时传入 `--nxrpg-manifest <path>`，而在跳转自包含打包（`--fat`）的 `nxjs-mvz.nro` 时**不传 `argv[1]`**，由内置的 `romfs:/main.js` 直接读取固定路径 `sdmc:/switch/nx.rpg/runtime/launch.json`。

---

## 3. Switch 内存制度、V8 JIT/WASM 与 CPU 调频策略

### 3.1 Applet Mode vs Application (Title Takeover) Mode

| 运行模式 | 可用物理内存 | V8 JIT / WebAssembly 状态 | 各核心运行策略 |
| :--- | :--- | :--- | :--- |
| **Application Mode**<br>（按住 `R` 启动任意游戏 / NSP Forwarder） | **~3.2 GiB – 3.5 GiB** | **允许 W^X JIT**（`switch-v8 >= 15.0.243-9` 下对象堆上限 `512 MiB`，`ArrayBuffer` 走数 GiB `newlib` 堆；**MZ 的 `effekseer.wasm` 正常编译执行**） | **全部三大核心满血推荐模式** |
| **Applet Mode**<br>（从系统相册进入 `hbmenu`） | **~440 MiB** | **强制 `jitless` 无 JIT 模式**（V8 对象堆上限约 `89 MiB`，WebAssembly 受限） | `easyrpg` 可正常运行；启动 `nxjs-mvz` 或 `mkxp-z` 前前端弹出强告警拦截框（允许知情继续或返回重进） |

### 3.2 按需 CPU 频率档位（`clkrst` / `pcv`）

部分重度插件化的 RPG Maker MV/MZ 地图或高分辨率立绘 XP/VXAce 同人游戏在 Switch 默认 `1020 MHz` CPU 频率下会出现掉帧。`nx.rpg` 提供四档频率策略：

- `system_default`（**默认**）：核心**完全不调用 `clkrst`**，尊重系统 powerd 策略或用户自装超频 sysmodule / KIP（如已超频至 2000 MHz 则保持 2000 MHz）
- `stock_1020`：`1020 MHz`（显式钳制回官方默认，最省电，适合 RM2000/2003 与轻量作品）
- `boost_1581`：`1581 MHz`（均衡超频档，显著改善 MV/MZ 切图加载与复杂并行事件卡顿）
- `boost_1785`：`1785 MHz`（Switch 官方芯片最高安全频率，适合重度特效 MZ / 大型 RGSS 脚本作品）

选择后三档时核心会在退出前把频率复位至 `1020 MHz`；`system_default` 不做任何调整。

---

## 4. 三大核心真实底层技术栈矩阵（经源码核实）

| 子系统 | EasyRPG Player (`easyrpg.nro`) | nx.js + Shim (`nxjs-mvz.nro`) | mkxp-z (`mkxp-z.nro`) |
| :--- | :--- | :--- | :--- |
| **锁定上游提交** | `a99ca834...` (`master`) | `462203af...` (`main`) | `a8f9d9ee...` (`dev`, v2.4.2) |
| **图形渲染后端** | **原生 EGL / OpenGL + `glad`**（`src/platform/switch/ui.cpp`） | **Skia GPU（Canvas2D）+ 原生 GLES3（WebGL2）**（`screen` 单上下文切换） | **SDL2 + Mesa EGL / OpenGL ES 2**（`gfx_backend=gles`） |
| **音频输出后端** | **原生 `libnx` `audout`**（48 kHz 16-bit 双缓冲线程）+ WildMIDI/FluidSynth/FMMidi | **Web Audio API + FFmpeg + `audren`** | **OpenAL Soft** + `SDL_sound` + Ogg/Vorbis + FluidSynth |
| **脚本/逻辑虚拟机** | C++ LCF 事件解释器（`liblcf >= 0.8.1`） | **V8 JavaScript 引擎**（`>= 15.0.243-9`）+ WebAssembly | **Ruby MRI 3.1**（静态链接 `libruby-static.a` + `arm64` 协程） |
| **大小写兼容机制** | 上游 `DirectoryTree` 原生小写缓存 | `shim/src/vfs/` 启动时构建全量小写映射表 | 上游 `pathCache: true` 原生内置 |
| **存档隔离方式** | 原生 `--save-path` 参数 | 关闭 NACP 账户存档 + 自定义 SD `localStorage` 与 `nwjs` `fs` 重定向 | 预加载脚本 `00_nxrpg_save_redirect.rb` 拦截重定向 |

---

## 5. SD 卡标准目录树（`sdmc:/switch/nx.rpg/`）

```text
sdmc:/switch/nx.rpg/
├── nx.rpg.nro                  # 主前端 Launcher (C++20 + Borealis)
├── cores/
│   ├── easyrpg.nro             # RM 2000 / 2003 核心
│   ├── nxjs-mvz.nro            # RM MV / MZ 核心 (--fat 自包含打包)
│   └── mkxp-z.nro              # RM XP / VX / VX Ace 核心
├── games/                      # 用户只读游戏库目录（支持 1~2 层子目录分类存放）
├── saves/                      # 按 <game_id> 完全隔离的独立存档目录（原子写保护）
├── rtp/                        # 5 代共享 RTP 资产池（rm2000 / rm2003 / rmxp / rmvx / rmvxa）
├── fonts/                      # 全局 CJK 回退字体（如 NotoSansCJK-Regular.ttc）
├── cache/
│   ├── library.json            # 基于目录 mtime 的增量扫描元数据缓存
│   └── covers/                 # 预缩放的游戏封面缩略图缓存 (<game_id>.jpg)
├── config/
│   ├── global.json             # 全局配置（见 global-config.schema.json）
│   └── games/                  # 单游戏覆写配置（<game_id>.json，见 game-config.schema.json）
└── runtime/
    ├── launch.json             # Launcher -> Core 瞬时启动清单（见 launch-manifest.schema.json）
    └── last_run.log            # 最近一次核心运行与崩溃诊断日志
```
