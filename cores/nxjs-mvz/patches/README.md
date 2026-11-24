# nx.js 补丁队列精确规格

基于上游 `TooTallNate/nx.js` 提交 `462203af39179fb630ae7fe981f3361b7147cdfb` 源码审计，上游已完成 Application 模式下 512 MiB V8 Heap Sizing 修复（见上游 `source/V8-HEAP-SIZING-HANDOFF.md`），仅需 1 个极小原生绑定补丁：

---

## `0001-applet-bind-env-set-next-load.patch`

- **修改文件**：
  - `source/applet.cc`
  - `packages/runtime/src/$.ts`
  - `packages/runtime/src/switch/index.ts`
- **原因**：
  - 上游 `Switch.exit()` 仅调用 `$.exit()` 结束事件循环，尚未在 `source/applet.cc` 中将 `libnx` 的 `envSetNextLoad(const char* path, const char* argv)` 暴露给 JS 层。
- **修改内容**：
  1. 在 `source/applet.cc` 新增 `nx_env_set_next_load` V8 回调，内部校验 `envHasNextLoad()` 并调用 `envSetNextLoad(nro_path, argv_str)`，失败时通过 `nx_throw_libnx_error` 抛出异常。
  2. 在 `packages/runtime/src/switch/index.ts` 导出 `Switch.setNextLoad(path: string, argv?: string): void`。
  3. 当玩家触发游戏内退出或 `exitCombo` 时，`shim` 先冲刷 `storage` 与 `last_run.log`，再执行：
     ```ts
     Switch.setNextLoad(manifest.returnTo, `${manifest.returnTo} --from-core`);
     Switch.exit();
     ```
