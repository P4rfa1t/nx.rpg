# nx.rpg 系统架构设计

本文档定义 `nx.rpg` 在 Nintendo Switch（Horizon OS / `libnx`）上的整体系统架构、进程模型、内存管理策略与 SD 卡目录规范。

---

## 1. 设计目标

1. **一站式体验**：用户只需将任意世代的 RPG Maker 游戏目录放入 `sdmc:/switch/nx.rpg/games/`，前端即可自动识别引擎版本并调度对应核心运行。
2. **高兼容性优先**：复用三个社区最活跃、兼容性最好的成熟核心，尽量少魔改上游核心源码，通过补丁队列（`patches/`）与适配层维持长期可同步性。
3. **Switch 原生友好**：全面支持 Joy-Con / Pro 手柄、触屏（尤其是 MV/MZ 触控交互）、休眠恢复、系统软键盘（输入主角姓名等场景）与 CJK 中日韩字体回退。
4. **严格内存与许可隔离**：针对 Switch 有限内存与不同开源协议，采用多 NRO 链式加载架构。

---

## 2. 总体拓扑：Launcher + 多核心 NRO 链式加载

Switch Homebrew 环境（Atmosphère `hbmenu` / `nro` loader）提供 `libnx` 标准接口 `envSetNextLoad(const char* path, const char* argv)`，允许当前 `.nro` 在退出后由 loader 立即加载另一个 `.nro`。

### 运行时控制流

```text
[用户从 Title Takeover 启动 nx.rpg.nro]
        │
        ▼
┌─────────────────────────────────────────┐
│  Launcher (nx.rpg.nro)                  │
│  1. 检测 Applet Mode vs Full RAM Mode   │
│  2. 扫描 sdmc:/switch/nx.rpg/games/     │
│  3. 自动识别引擎（2k/2k3/XP/VX/VXA/MV/MZ）│
│  4. 读取/编辑单游戏配置与全局设置         │
└──────────────────┬──────────────────────┘
                   │ 用户选择游戏并按下启动
                   ▼
   写入 sdmc:/switch/nx.rpg/runtime/launch.json
   调用 envSetNextLoad("sdmc:/switch/nx.rpg/cores/<core>.nro", ...)
   正常退出 Launcher 释放全部显存与内存
                   │
                   ▼
┌─────────────────────────────────────────┐
│  Core NRO (easyrpg / nxjs-mvz / mkxp-z) │
│  1. 启动后读取 launch.json 或 argv       │
│  2. 挂载游戏目录、独立存档目录、RTP与字体 │
│  3. 运行游戏主循环                       │
│  4. 用户退出游戏（或快捷键唤出退出）      │
└──────────────────┬──────────────────────┘
                   │
                   ▼
   调用 envSetNextLoad("sdmc:/switch/nx.rpg/nx.rpg.nro", "--from-core")
   核心退出并自动返回 Launcher 界面
```

### 前端（Launcher）技术选型建议

| 方案 | 优点 | 缺点 | 结论 |
| :--- | :--- | :--- | :--- |
| **方案 A：C++ + Borealis（Switch 原生风格 UI）** | 极低内存占用、启动极快（<1 秒）、天然契合 Switch 手柄导航与主题风格、C++ 直接复用游戏扫描逻辑 | 需维护独立 C++ UI 代码 | **推荐（首选）** |
| **方案 B：基于 nx.js (Canvas/TS) 编写前端** | UI 开发效率高、与 MV/MZ 核心共享工具链 | 每次返回前端需重新冷启动 V8 + Skia，切换延迟略高 | 备选（适合原型验证） |

> 在 Phase 1 初期可先完成极简 C++ 启动器验证链式加载与引擎识别；待核心跑通后再迭代 Borealis 完整封面墙 UI。

---

## 3. Switch 内存模型与 Applet Mode 处理

Switch Homebrew 存在两种启动上下文：

- **Applet Mode（相册启动）**：可用内存约 **440 MB**。
  - `EasyRPG Player`（RM2000/2003）通常可以运行。
  - `nx.js`（V8 引擎 + Skia GPU + WebGL2 + MV/MZ 贴图）及 `mkxp-z`（Ruby MRI + 大量 RPG Maker XP/VXA 高清立绘缓存）在 Applet Mode 下极易触发 OOM 崩溃。
- **Application / Title Takeover Mode（按住 R 键启动任意正版/安装游戏，或通过独立 Forwarder NSP）**：可用内存约 **3.2 GB ~ 3.5 GB**。

### 架构约束

1. Launcher 启动时调用 `appletGetAppletType()` 检测运行模式：
   - 若为 `AppletType_LibraryApplet` / `AppletType_SystemApplet`，在界面顶部常驻醒目提示条，并在启动 `nxjs-mvz` 或 `mkxp-z` 游戏前弹出明确警告（允许用户知情继续或退出改用 Title Takeover）。
2. 各核心在启动时依据当前可用堆空间动态配置缓存上限（例如 `nx.js` 的 V8 Heap Sizing 与纹理缓存上限、`mkxp-z` 的 Bitmap 回收阈值）。

---

## 4. Switch 图形、音频与输入栈对比

三个核心各自拥有独立的底层后端，由独立 NRO 进程隔离，避免上下文冲突：

| 子系统 | EasyRPG Player | nx.js (MV / MZ) | mkxp-z (XP / VX / VXAce) |
| :--- | :--- | :--- | :--- |
| **图形渲染** | SDL2 / Software Renderer | Skia GPU（Canvas2D）+ GLES3（WebGL2） | SDL2 + OpenGL ES 2 (`gfx_backend=gles`) / Mesa EGL |
| **音频输出** | SDL2 / libnx audren + WildMIDI / FluidSynth / FMMidi | Web Audio API + FFmpeg + `audren` | OpenAL Soft + SDL_sound + Vorbis / Ogg / FluidSynth |
| **脚本虚拟机** | 内置 LCF 事件解释器（C++） | V8 JavaScript Engine + WASM | Ruby MRI 3.1（静态链接 `libruby`） |
| **输入方式** | Joy-Con / Pro 手柄 + 触屏 | Gamepad API + 触屏（映射鼠标/触摸）+ 软键盘 | SDL2 GameController + 虚拟键盘映射 + 触屏辅助 |

---

## 5. SD 卡目录布局规范

所有运行时文件统一收敛于 `sdmc:/switch/nx.rpg/` 下，方便用户备份、迁移与清理：

```text
sdmc:/switch/nx.rpg/
├── nx.rpg.nro                  # 主前端 Launcher
├── cores/
│   ├── easyrpg.nro             # RM 2000 / 2003 核心
│   ├── nxjs-mvz.nro            # RM MV / MZ 核心（内置 MV/MZ Shim 包）
│   └── mkxp-z.nro              # RM XP / VX / VX Ace 核心
├── games/                      # 用户放置游戏目录（支持一层或多层子目录扫描）
│   ├── MyRM2003Game/
│   ├── MyXPGame/
│   └── MyMZGame/
├── saves/                      # 按 gameId 隔离的存档目录（防止污染原游戏目录，便于云/本地备份）
│   └── <game_id>/
├── rtp/                        # 共享 RTP 资产池（大幅节省 SD 卡空间）
│   ├── rm2000/
│   ├── rm2003/
│   ├── rmxp/
│   ├── rmvx/
│   └── rmvxa/
├── fonts/                      # 全局 CJK 回退字体（如 NotoSansCJK / WenQuanYi）
├── config/
│   ├── global.json             # 全局默认设置（语言、显示比例、快捷键、默认编码）
│   └── games/
│       └── <game_id>.json      # 单游戏覆写配置（按键映射、编码、插件开关、缩放模式）
└── runtime/
    ├── launch.json             # Launcher -> Core 瞬时启动清单
    └── last_run.log            # 最近一次核心运行日志（用于排错与前端展示错误原因）
```

---

## 6. 公共能力规划

- **全局退出与菜单手势**：统一约定退出/唤出核心菜单的组合键（例如 `L + R + Plus` 或 `ZL + ZR + Minus`），保证从任意核心都能平滑返回 `nx.rpg.nro`。
- **编码与 CJK 字体治理**：大量中文/日文 RPG Maker 2000/2003/XP/VX/VXAce 游戏依赖 `Shift_JIS`、`GBK`、`Big5`、`CP949` 编码及 Windows 系统字体（如 MS Gothic / 宋体 / 黑体）。系统将提供统一的字符集检测与 `fonts/` 回退字体注入。
- **日志回传**：核心发生脚本异常或资源缺失时写入 `runtime/last_run.log`，返回 Launcher 后可一键查看最近崩溃日志，极大降低 Switch 实机调试成本。
