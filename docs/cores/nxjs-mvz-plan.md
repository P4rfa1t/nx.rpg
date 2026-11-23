# nx.js 核心集成规划（RPG Maker MV / MZ）

- **上游仓库**：[TooTallNate/nx.js](https://github.com/TooTallNate/nx.js)（`main` 分支）
- **核心定位**：支持 RPG Maker MV（`rmmv`）与 RPG Maker MZ（`rmmz`）
- **输出产物**：`sdmc:/switch/nx.rpg/cores/nxjs-mvz.nro`
- **许可证**：MIT
- **集成难度**：★★★☆☆（中等 — 底层引擎原生支持 Switch，工作重心在 Browser/DOM/NW.js 兼容层）

---

## 1. 上游能力与 MV / MZ 需求的精准映射

`TooTallNate/nx.js` 已原生运行于 Nintendo Switch，其内置的底层组件与 RPG Maker MV/MZ 的运行需求高度契合：

| 能力维度 | `nx.js` 原生能力 | RPG Maker MV 需求 | RPG Maker MZ 需求 | 缺口与适配策略 |
| :--- | :--- | :--- | :--- | :--- |
| **JS 引擎** | **V8** | ES5 / ES6 插件脚本 | ES6+ 现代脚本 | 几乎零缺口；需按 Switch 内存模式调优 V8 Heap 上限 |
| **2D 渲染** | **Skia GPU Canvas2D** | `Bitmap` 离屏绘制、文字渲染、滤镜 | `Bitmap`、窗口与 UI 绘制 | 需补齐 `HTMLCanvasElement` DOM 包装与样式属性 |
| **3D / GPU 渲染** | **WebGL2**（基于真 GLES3） | PixiJS v4（WebGL1 / Canvas） | PixiJS v5（WebGL2） | 验证 PixiJS v4/v5 着色器编译与纹理上传边界 |
| **WASM** | **V8 原生 WebAssembly** | 部分第三方插件 | **Effekseer 粒子特效 (`effekseer.wasm`)** | 原生支持，显著提升 MZ 特效兼容性 |
| **音频 / 视频** | **Web Audio API + FFmpeg + `audren`** | WebAudio (`ogg`/`m4a`) + HTML5 Audio/Video | WebAudio (`ogg`) + HTML5 Video | 需补齐 `Audio`/`Video` DOM 事件与解密流加载（`.rpgmvo`/`.ogg_`） |
| **输入与软键盘** | **Gamepad API + 触屏 + `software-keyboard`** | 键盘 / 手柄 / 鼠标 / 触摸 | 键盘 / 手柄 / 鼠标 / 触摸 | 将 Switch 手柄/触屏桥接为标准 `KeyboardEvent`/`Gamepad`/`TouchEvent` |

---

## 2. 核心工作：MV / MZ 浏览环境兼容层（`cores/nxjs-mvz/shim/`）

RPG Maker MV 和 MZ 本质上是运行在浏览器或 NW.js（Chromium + Node.js）中的 HTML5 应用。`nxjs-mvz` 的核心任务是构建一层轻量、高兼容的 **Bootstrap + DOM/NW.js Shim**，将 `index.html` 中声明的脚本顺序加载并在 `nx.js` 中执行：

### 2.1 分层模块规划

1. **`bootstrap.ts`（引导器）**
   - 读取 `launch.json`，解析目标游戏的 `index.html`（或按 MV `js/main.js` / MZ `js/main.js` 约定加载依赖脚本队列）。
   - 设置基准路径 `contentRoot`（自动适配 MV 的 `www/` 子目录）。
   - 捕获全局 `uncaughtException` / `unhandledRejection`，写入 `manifest.logPath` 并优雅返回 Launcher。

2. **`dom-shim.ts`（最小 DOM 树与事件循环）**
   - 实现 `window`、`document`、`document.head`、`document.body`、`HTMLElement` 子集。
   - 支持 `<canvas>`（委托 `nx.js` `OffscreenCanvas`/主屏 Canvas）、`<img>`（解码 `png` 及 RPG Maker 加密图 `.rpgmvp`/`.png_`）、`<script>`（从 SD 卡同步/异步求值）、`<style>`/`<link>`（提取 `@font-face` 声明）。
   - 维护 `document.addEventListener` / `window.addEventListener` 事件派发（`keydown`, `keyup`, `mousedown`, `mousemove`, `mouseup`, `touchstart`, `touchmove`, `touchend`, `resize`, `load`, `blur`, `focus`）。

3. **`fs-network-shim.ts`（本地文件与网络重定向）**
   - 拦截 `XMLHttpRequest` 与相对路径 `fetch()`，直接映射至 `sdmc:/.../contentRoot/<path>`。
   - **大小写不敏感路径索引**：在游戏启动时扫描目录构建小写路径映射表，彻底解决 Windows 制作的游戏在大小写敏感或路径拼写不一致时导致的 404 崩溃。
   - 拦截 `NW.js` 常用调用：`window.Utils.isNwjs()` 开关策略，按需提供 `require('fs')`、`require('path')`、`process.mainModule.filename` 垫片，使依赖本地文件读写的插件与存档管理器（`StorageManager`）无缝重定向到 `manifest.game.savePath`。

4. **`storage-shim.ts`（存档持久化）**
   - 实现 `localStorage` 与 `indexedDB`（轻量文件后端），将数据持久化到 `sdmc:/switch/nx.rpg/saves/<game_id>/`，避免污染游戏目录。

5. **`font-shim.ts`（字体加载与 CJK 回退）**
   - 解析 MV 的 `fonts/gamefont.css` 与 MZ 的 `data/System.json` 字体配置，注册游戏内置 `.ttf`/`.otf`/`.woff` 字体，并挂载 `assets.fallbackFontPath` 作为 CJK 缺字回退。

6. **`input-bridge.ts`（手柄 / 触屏 / 软键盘桥接）**
   - 默认手柄映射：左摇杆/十字键 → 方向键、`A` → 确认（`Z`/`Enter`）、`B` → 取消/菜单（`X`/`Esc`）、`Y` → 奔跑（`Shift`）、`L`/`R` → 翻页（`PageUp`/`PageDown`）、`exitCombo` → 退出回 Launcher。
   - 触屏直接映射为 MV/MZ 熟悉的单指点按/双指取消。

---

## 3. 关键性能与内存注意事项

- **强制提示 Title Takeover 模式**：V8 + Skia + PixiJS 贴图在 Applet Mode（~440 MB）下极易 OOM，仅推荐全内存模式运行。
- **纹理与音频内存管控**：针对部分大型 MV/MZ 同人游戏包含超大 4K 立绘或未压缩音频，需监控 Switch 可用内存并在切图时触发 `PIXI.utils.clearTextureCache()` 与 V8 GC 提示。
- **动态 `eval` 与插件兼容**：部分加密/混淆插件依赖 `Function` / `eval` / `document.createElement('script')`，引导器需完整保留 V8 动态代码执行能力。

---

## 4. 验收标准（Phase 2）

- [ ] 基于 `TooTallNate/nx.js` 成功产出 `nxjs-mvz.nro` 并接入 `launch.json`。
- [ ] **MV 测试通过**：新建/示例 RPG Maker MV 工程正常渲染地图、播放 BGM/SE、进入战斗、存读档并退出返回 Launcher。
- [ ] **MZ 测试通过**：新建/示例 RPG Maker MZ 工程正常启用 WebGL2 渲染、运行 Effekseer WASM 动画、存读档并退出返回 Launcher。
- [ ] 实测 3~5 款含常见插件（如 YEP / VisuStella / 文字增强类插件）的 MV/MZ 游戏并形成兼容报告。
