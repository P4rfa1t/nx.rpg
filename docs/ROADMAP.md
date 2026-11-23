# nx.rpg 分阶段开发路线图（Roadmap）

为了尽早在 Switch 实机/模拟器上跑通端到端链路，开发严格按照 **“由易到难、先通闭环再攻硬骨头”** 的顺序推进：
**基础设施 → EasyRPG（原生支持） → nx.js + MV/MZ（补齐 Web/DOM Shim） → mkxp-z（交叉编译 + Ruby MRI 移植） → 统一体验打磨**。

---

## Phase 0：项目布局、架构与规范（当前阶段）

- [x] 确立多 NRO 链式加载架构与开源许可证隔离原则
- [x] 建立仓库目录骨架、SD 卡目录模板与顶层 `Makefile`
- [x] 定义 7 代 RPG Maker 自动识别规则（`ENGINE_DETECTION.md`）
- [x] 定义 Launcher ↔ Core 启动协议与 JSON Schema（`LAUNCH_PROTOCOL.md`、`common/schemas/`）
- [x] 完成三大核心的专项移植与集成方案文档
- [ ] 搭建 GitHub Actions devkitPro 容器化基础构建 CI

---

## Phase 1：最小前端（Launcher MVP）与 EasyRPG 核心闭环

**目标**：验证 `nx.rpg.nro` → 扫描识别游戏 → 写入 `launch.json` → `envSetNextLoad()` → 启动 `easyrpg.nro` → 游玩 RM2000/2003 → 退出自动返回 `nx.rpg.nro` 的完整链路。

- [ ] 实现 `common` 模块：7 代 RPG Maker 自动检测器 + `launch.json` 读写库
- [ ] 实现 Launcher MVP（列表展示、引擎标识、Applet Mode 检测、链式加载触发）
- [ ] 接入 `EasyRPG/Player`（使用其官方 `switch-release` CMake Preset 构建）
- [ ] 编写 EasyRPG 轻量补丁：支持读取 `launch.json`（游戏路径、独立存档路径、RTP 路径、指定字符编码）及退出时 `envSetNextLoad` 返回 `nx.rpg.nro`
- [ ] 实机/模拟器验收：至少跑通 1 款 RM2000 与 1 款 RM2003（含中文/日文文本与 RTP 依赖）游戏

---

## Phase 2：nx.js 核心与 RPG Maker MV / MZ 运行时适配

**目标**：基于 `TooTallNate/nx.js`（V8 + Skia GPU + WebGL2 + Web Audio + WASM）实现可运行的 MV 与 MZ 兼容层。

- [ ] 接入 `TooTallNate/nx.js` 构建流程，输出 `cores/nxjs-mvz.nro`
- [ ] 构建 `cores/nxjs-mvz/shim/` 核心浏览环境垫片：
  - `window` / `document` / DOM 节点最小实现（含 `<canvas>`, `<img>`, `<audio>`, `<video>`, `<script>` 动态加载）
  - `XMLHttpRequest` / `fetch` 重定向至 `sdmc:/` 本地游戏目录（支持大小写不敏感路径回退）
  - `localStorage` 与 `NW.js` (`require('fs')`, `require('path')`, `process.mainModule`) 兼容层，对接 `saves/<game_id>/`
  - `@font-face` / `FontFace` / `document.fonts` 加载游戏内 `fonts/` 与全局 CJK 回退字体
  - Joy-Con / Pro 手柄 → RPG Maker 键盘按键映射 + 触屏 → `TouchEvent` / `MouseEvent` 映射
- [ ] **MV 里程碑**：跑通标准 RPG Maker MV 工程（PixiJS v4 WebGL/Canvas、BGM/SE 播放、存读档）
- [ ] **MZ 里程碑**：跑通标准 RPG Maker MZ 工程（PixiJS v5 WebGL2、Effekseer WASM 特效、存读档）
- [ ] 针对常见主流插件（YEP / VisuStella / Galv / 菜单/文字插件）建立兼容测试用例与缺失 API 补齐清单

---

## Phase 3：mkxp-z 移植至 Switch（XP / VX / VX Ace）

**目标**：攻克最复杂的交叉编译与 Ruby 运行时移植，产出可运行的 `cores/mkxp-z.nro`。

- [ ] 完成 `cores/mkxp-z/cross/switch-aarch64.ini` Meson 交叉编译配置
- [ ] 梳理并验证 `devkitPro/portlibs/switch` 依赖矩阵（SDL2、SDL2_image、SDL2_ttf、FreeType、OpenAL Soft、PhysFS、Pixman、Ogg/Vorbis、Mesa EGL/GLES2）
- [ ] 交叉编译静态 **Ruby MRI 3.1**（面向 `aarch64-none-elf` / newlib，裁剪不支持的 POSIX 调用，验证 Fiber/Coroutine 与 RGSS GC 稳定性）
- [ ] 适配 `mkxp-z` Switch 平台层：
  - 默认启用 `gfx_backend=gles`、`use_miniffi=false`
  - 为常见 RGSS Win32API 脚本提供安全 Stub（如按键检测、窗口标题、路径查询）
  - 集成 `launch.json` 读取（自动指定 `rgssVersion` 1/2/3、RTP 路径、存档重定向、CJK 回退字体）
  - 适配手柄映射与退出返回 `nx.rpg.nro`
- [ ] 实机/模拟器验收：分别跑通 RPG Maker XP（RGSS1）、VX（RGSS2）、VX Ace（RGSS3）测试工程（含 `.rgssad`/`.rgss2a`/`.rgss3a` 加密包解包）

---

## Phase 4：前端完善、统一配置与体验打磨

**目标**：将 `nx.rpg` 升级为面向普通 Switch 玩家的完整桌面级前端。

- [ ] 升级 Launcher UI（游戏封面墙/列表视图、引擎徽章、搜索与筛选、收藏夹、最近游玩）
- [ ] 单游戏配置编辑器：字符编码覆写、自定义键位映射、画面缩放（整数倍/满屏/原比例）、RTP 选择
- [ ] Switch 系统软键盘集成（用于角色起名、密码输入等场景，打通三大核心）
- [ ] 崩溃与异常日志查看器（前端直接阅读 `runtime/last_run.log`）
- [ ] 内置 RTP 状态检测器（扫描缺失的 RTP 并提示用户放置路径）

---

## Phase 5：发布工程与持续集成

- [ ] 完善 GitHub Actions 自动化构建（基于 `devkitpro/devkita64` Docker 镜像分 Job 构建 Launcher 与三个核心）
- [ ] 自动打包标准 SD 卡发布压缩包（`nx.rpg-vX.Y.Z.zip`）与上游源码/补丁归档（满足 GPL 合规）
- [ ] 建立公开的《Switch 游戏兼容性列表》模板与 Issue 反馈模板

---

## 核心技术风险与缓解策略

| 风险项 | 影响范围 | 风险等级 | 缓解策略 |
| :--- | :--- | :---: | :--- |
| **Ruby MRI 交叉编译至 newlib/devkitA64** | mkxp-z (XP/VX/VXAce) | **高** | 参考社区已知 `mruby`/CRuby Switch 移植补丁，优先关闭非必需标准库（`socket`/`openssl`/`fiddle`）与 `fork`，保证 RGSS 所需核心子集稳定 |
| **MV/MZ 第三方插件依赖浏览器/NW.js 冷门 API** | nx.js (MV/MZ) | **中高** | 采用按需演进的 Shim 架构 + 详细缺失 API 日志上报，优先覆盖官方核心脚本与 Top 50 热门插件 |
| **Switch 内存限制（特别是 Applet Mode 与大图集）** | nx.js / mkxp-z | **中** | 强提醒 Title Takeover 全内存模式；为 V8 设定合理堆上限，为 Bitmap/Texture 缓存配置积极回收策略 |
| **Switch SD 卡（FAT32/exFAT）路径大小写与非法文件名** | 全部核心 | **中** | 在适配层实现大小写不敏感路径解析缓存（RPG Maker 游戏常见脚本路径与真实文件名大小写不一致） |
| **Windows 专用 Win32API / DLL 扩展** | mkxp-z (XP/VX/VXAce) | **中** | 提供可控的 RGSS Win32API Ruby Stub 层，拦截常见非关键调用避免崩溃，对依赖原生 x86 DLL 的游戏明确标注不兼容 |
