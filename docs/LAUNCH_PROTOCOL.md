# Launcher ↔ Core 启动协议与配置合并规范（Launch Protocol）

本文档定义 `nx.rpg.nro`（Borealis 前端）如何将三层用户配置合并为 `runtime/launch.json`，以及各核心 `.nro` 的精确启动与链式返回契约。

---

## 1. 三层配置合并优先级（Config Merger）

每次启动游戏前，`nxrpg::ConfigMerger` 按从低到高的优先级解析最终参数：

```text
[1. 引擎内置推荐默认值]  (如 rmmv/rmmz 默认 utf-8；rmxp rgssVersion=1)
          │
          ▼ 覆写
[2. 全局配置 config/global.json]  (Schema: common/schemas/global-config.schema.json)
          │
          ▼ 覆写
[3. 单游戏配置 config/games/<id>.json]  (Schema: common/schemas/game-config.schema.json)
          │
          ▼ 输出（经原子写 .tmp + fsync + rename）
[4. 瞬时启动清单 runtime/launch.json]  (Schema: common/schemas/launch-manifest.schema.json)
```

---

## 2. 按核心区分的 `envSetNextLoad` 参数传递规则（关键！）

经核实三个上游仓库的 `main` 入口实现，**不同核心对 `argv[1]` 的语义假设不同**，因此 `nxrpg::Chainloader::format_core_argv()` 严格执行下表规则：

| 目标核心 | `envSetNextLoad(path, argv)` 中的 `argv` 字符串 | 原因说明 |
| :--- | :--- | :--- |
| **`easyrpg`** | `"sdmc:/switch/nx.rpg/cores/easyrpg.nro --nxrpg-manifest sdmc:/switch/nx.rpg/runtime/launch.json"` | C++ `main(argc, argv)` 标准解析 `--nxrpg-manifest` |
| **`mkxp-z`** | `"sdmc:/switch/nx.rpg/cores/mkxp-z.nro --nxrpg-manifest sdmc:/switch/nx.rpg/runtime/launch.json"` | C++ `main(argc, argv)` 标准解析 `--nxrpg-manifest` |
| **`nxjs-mvz`** | `"sdmc:/switch/nx.rpg/cores/nxjs-mvz.nro"`（**严禁附带 `argv[1]`！**） | `nx.js` 运行时会将 `Switch.argv[1]` 当作外部入口 `.js`/`.nro` 文件路径覆盖内置 `romfs:/main.js`！不传 `argv[1]` 可确保稳定启动内置 `romfs:/main.js` 并直接读取固定路径 `sdmc:/switch/nx.rpg/runtime/launch.json` |

---

## 3. `launch.json` 完整标准示例（Schema v1）

校验规则以 [common/schemas/launch-manifest.schema.json](../common/schemas/launch-manifest.schema.json) 为准：

```json
{
  "schemaVersion": 1,
  "sessionId": "20261124-120000-f4a9c2",
  "returnTo": "sdmc:/switch/nx.rpg/nx.rpg.nro",
  "logPath": "sdmc:/switch/nx.rpg/runtime/last_run.log",
  "game": {
    "id": "sample-mv-7c8d1a",
    "title": "Sample RPG MV",
    "engine": "rmmv",
    "core": "nxjs-mvz",
    "gamePath": "sdmc:/switch/nx.rpg/games/SampleMV",
    "contentRoot": "sdmc:/switch/nx.rpg/games/SampleMV/www",
    "savePath": "sdmc:/switch/nx.rpg/saves/sample-mv-7c8d1a",
    "execName": "Game"
  },
  "assets": {
    "rtpPath": "",
    "fallbackFontPath": "sdmc:/switch/nx.rpg/fonts/NotoSansCJK-Regular.ttc"
  },
  "runtime": {
    "encoding": "utf-8",
    "rgssVersion": 0,
    "cpuProfile": "boost_1581",
    "enableTouch": true,
    "enableSoftwareKeyboard": true
  },
  "video": {
    "scalingMode": "fit",
    "integerScale": false,
    "vsync": true,
    "showFps": false
  },
  "input": {
    "profile": "default",
    "swapConfirmCancel": false,
    "exitCombo": ["L", "R", "PLUS"]
  },
  "coreOptions": {
    "mvz": {
      "renderer": "auto",
      "pretendNwjs": true,
      "maxTextureSize": 4096
    }
  }
}
```

---

## 4. 核心退出与返回协议（Core → Launcher）

当玩家通过游戏内“退出”选项或按下 `input.exitCombo`（默认 `L + R + PLUS`）退出游戏时，核心必须按严格顺序执行清理：

1. **落盘屏障**：完成所有待写入存档及 `logPath` 日志的原子写入与 `fflush` / `fsync`。
2. **频率复位**：若启动时通过 `clkrst` 调整过 CPU 频率，必须恢复为系统默认 `1020 MHz`。
3. **释放外设与句柄**：关闭音频流（`audout` / `audren` / `OpenAL`）、图形上下文（EGL / GLES / SDL / Skia）及 `romfsExit()`。
4. **注册回跳**：若 `manifest.returnTo` 非空且目标文件存在，调用：
   ```c
   envSetNextLoad(manifest.returnTo, "sdmc:/switch/nx.rpg/nx.rpg.nro --from-core");
   ```
5. **退出进程**：正常退出 `main`（或调用 `Switch.exit()`）。

### 标准致命异常日志头

若核心因未捕获异常、缺失核心数据或内存耗尽退出，须在 `logPath` 末尾写入统一格式的诊断行，供前端 `DiagnosticsTab` 自动识别并弹窗提示：

```text
[nx.rpg][FATAL][core=mkxp-z][game=touhou-xp-91b2][code=SCRIPT_ERR] NoMethodError: undefined method `foo' for nil:NilClass
```

### 完整运行日志（Per-Core Run Logs）

`logPath`（默认 `runtime/last_run.log`）同时承载各核心的完整运行日志，均为**追加写**：

- **EasyRPG**：上游 `--log-file` 全量 Player 日志（时间戳 + Error/Warning/Info/Debug 各级行）。
- **mkxp-z**：补丁将 `stderr`（含 `Debug()`、RGSS 脚本错误、SDL 消息）`freopen` 镜像至 `logPath`，并在启动时写入 `---- mkxp-z session start (game=<id>) ----` 横幅。
- **nxjs-mvz**：shim 在 session start / VFS 索引完成 / 干净退出 / FATAL 时追加带 ISO 时间戳的行。
- **Launcher**：自身日志独立写在 `runtime/launcher.log`，Diagnostics 页同时显示两者的尾部。
