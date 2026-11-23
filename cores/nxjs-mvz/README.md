# cores/nxjs-mvz — nx.js + RPG Maker MV / MZ 适配层

- **上游源码路径**：`third_party/nx.js`
- **构建系统**：`devkitA64` Makefile + Node.js / pnpm（TypeScript Runtime & Shim）
- **目标引擎**：RPG Maker MV / RPG Maker MZ
- **规划文档**：[docs/cores/nxjs-mvz-plan.md](../../docs/cores/nxjs-mvz-plan.md)

## 目录用途

- `shim/`：RPG Maker MV / MZ 运行时兼容垫片（`bootstrap`、`dom-shim`、`fs-network-shim`、`storage-shim`、`font-shim`、`input-bridge`、`nwjs-shim`）。
- `patches/`：针对 `TooTallNate/nx.js` 上游源码的必要补丁（`--nxrpg-manifest` 读取、`envSetNextLoad` 返回、V8 堆参数调优等）。
