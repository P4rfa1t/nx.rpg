# nx.js 核心集成规划（RPG Maker MV / MZ）

- **上游锁定提交**：`TooTallNate/nx.js` @ `462203af39179fb630ae7fe981f3361b7147cdfb`（`main`）
- **底层强制依赖**：**`switch-v8 >= 15.0.243-9`**（修复 Horizon W^X JIT GC 写只读代码页导致的 Data Abort，并在 Application 模式开放 `512 MiB` V8 对象堆 + 数 GiB `newlib` `ArrayBuffer` 堆）
- **输出产物**：`sdmc:/switch/nx.rpg/cores/nxjs-mvz.nro`（强制 `NXJS_FAT=1` `--fat` 自包含打包）
- **许可证**：MIT
- **集成难度**：★★★☆☆（Phase 2 核心任务）

---

## 1. 上游源码核实的关键架构约束（必读）

通过对 `TooTallNate/nx.js` 源码（`source/applet.cc`、`packages/runtime/src/screen.ts`、`storage.ts`、`switch/index.ts`、`packages/nro/src/main.ts`）的逐项核查，确立以下 5 条关键设计准则：

1. **必须启用 `--fat` 自包含 NRO 打包**：
   - `@nx.js/nro` 新版默认采用 `--slim` 模式（只生成微型加载器并依赖 `sdmc:/nx.js/nxjs-v*.nro`）。`cores/nxjs-mvz/package.json` 必须指定 `NXJS_FAT=1 nxjs-nro --fat`，确保 `cores/nxjs-mvz.nro` 单文件完整闭环。
2. **禁止向 `nxjs-mvz.nro` 传递 `argv[1]`**：
   - `nx.js` 将 `Switch.argv[1]` 视为外部 `.js` 或 `.nro` 入口路径（见 `packages/runtime/src/switch/index.ts` 中 `entrypoint` 逻辑）。因此前端跳转 `nxjs-mvz.nro` 时不附带位置参数，由内置 `romfs:/main.js` 直接读取 `sdmc:/switch/nx.rpg/runtime/launch.json`。
3. **主屏幕 `screen` 单一上下文锁（`screen.ts`）**：
   - `nx.js` 全局 `screen`（`1280×720`）只允许获取 `'2d'` **或** `'webgl2'` 之一，且在获取 WebGL2 前若应用代码主动获取过 `'2d'` 会导致 `'webgl2'` 返回 `null`；同时 `screen.getContext('webgl')` 默认返回 `null`。
   - **对策**：`shim/src/index.ts` 在游戏脚本启动前避免触发应用态 2D 占用；`shim/src/dom/` 将 PixiJS v4 请求的 `'webgl'` / `'experimental-webgl'` 桥接至 `screen.getContext('webgl2')`（或按配置回退 `'2d'`），而离屏 `Bitmap` 画布统一使用 `new OffscreenCanvas(w, h)` / `Canvas`。
4. **关闭 NACP 系统账户存档（`userAccountSaveDataSize: 0`）**：
   - `nx.js` 内置 `localStorage`（`storage.ts`）在 `userAccountSaveDataSize > 0` 时会强制拉起 Switch 系统选人弹窗并把数据写入主机 NAND。我们将 `userAccountSaveDataSize` 设为 `0`，由 `shim/src/storage/` 提供基于 `sdmc:/switch/nx.rpg/saves/<game_id>/` 的原子写 `localStorage` 与 `NW.js` `fs` 后端。
5. **MZ 的 `effekseer.wasm` 与 Applet 模式限制**：
   - 在相册 Applet 模式下，`nx.js` 以 `jitless` 运行，V8 对象堆仅约 `89 MiB` 且 WASM 编译受限；因此在启动 MZ 游戏或大型 MV 游戏时，Launcher 默认提示使用 Title Takeover 全内存模式。

---

## 2. 模块化实现蓝图（`cores/nxjs-mvz/`）

接口骨架已就位于 [cores/nxjs-mvz/shim/src/](../../cores/nxjs-mvz/shim/src/)：

- **原生补丁（`patches/0001-applet-bind-env-set-next-load.patch`）**：在 `source/applet.cc` 中暴露 `Switch.setNextLoad(nroPath, argv)` 封装 `libnx` `envSetNextLoad`。
- **`vfs/index.ts`**：一次性构建 `contentRoot` 小写路径索引表，拦截 `XMLHttpRequest` 与 `fetch()`，彻底消除大小写 404。
- **`crypto/rpgm-decryptor.ts`**：解析 `System.json -> encryptionKey`，流式解密 MV `.rpgmvp`/`.rpgmvo`/`.rpgmvm` 与 MZ `.png_`/`.ogg_`/`.m4a_` 的 16 字节伪头与前 16 字节异或载荷。
- **`dom/index.ts`**：提供 `window` / `document` / `HTMLCanvasElement` / `HTMLImageElement` / `HTMLVideoElement` / `FontFace` 垫片与脚本顺序加载器。
- **`nwjs/index.ts` & `storage/index.ts`**：兼容 `Utils.isNwjs()`，为 `StorageManager` 与常见插件（YEP / VisuStella）提供沙箱化 `require('fs')`、`require('path')` 与原子写存档隔离。
- **`input/index.ts` & `perf/index.ts`**：桥接手柄、掌机触屏（含双指取消）、系统软键盘与 `@nx.js/clkrst` CPU 调频。

---

## 3. 验收标准（Phase 2）

- [ ] Node 端 `vitest` 单测覆盖 `.rpgmvp`/`.png_` 解密器、大小写无关 VFS 与存档沙箱重定向。
- [ ] **MV 实机闭环**：标准 RPG Maker MV 工程（含 `www/` 目录结构与加密资源）60 FPS 渲染、WebAudio 播放、存读档并回跳 `nx.rpg.nro`。
- [ ] **MZ 实机闭环**：标准 RPG Maker MZ 工程在全内存模式下正常启用 WebGL2 与 `effekseer.wasm` 粒子动画、存读档并回跳 `nx.rpg.nro`。
