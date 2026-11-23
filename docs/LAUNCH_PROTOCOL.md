# Launcher ↔ Core 启动协议规范（Launch Protocol）

为了在 `nx.rpg.nro`（主前端）与三个独立核心 `.nro` 之间建立稳定、松耦合、可扩展的通信契约，所有核心统一遵循本启动协议。

---

## 1. 通信机制

Switch 的 `envSetNextLoad(path, argv)` 对 `argv` 长度存在限制，且包含中文/空格的游戏路径在命令行转义上容易出错。因此采用 **“极简 `argv` + JSON 启动清单文件”** 的双层机制：

1. Launcher 将完整启动参数写入固定路径：
   ```text
   sdmc:/switch/nx.rpg/runtime/launch.json
   ```
2. Launcher 调用：
   ```c
   envSetNextLoad(
     "sdmc:/switch/nx.rpg/cores/<core>.nro",
     "sdmc:/switch/nx.rpg/cores/<core>.nro --nxrpg-manifest sdmc:/switch/nx.rpg/runtime/launch.json"
   );
   ```
3. 目标核心启动时检查 `--nxrpg-manifest <path>` 参数：
   - **若存在**：解析该 JSON 清单，直接以清单指定配置启动游戏，并在游戏退出时自动通过 `envSetNextLoad(returnTo, ...)` 返回 Launcher。
   - **若不存在**（例如用户直接从 hbmenu 单独启动某个核心）：核心仍可按自身默认行为或提示单独运行，不强制报错崩溃。

---

## 2. `launch.json` 示例

完整字段校验规则见 [common/schemas/launch-manifest.schema.json](../common/schemas/launch-manifest.schema.json)。

```json
{
  "schemaVersion": 1,
  "sessionId": "20261123-214600-a1b2c3",
  "returnTo": "sdmc:/switch/nx.rpg/nx.rpg.nro",
  "logPath": "sdmc:/switch/nx.rpg/runtime/last_run.log",
  "game": {
    "id": "demo-mv-01",
    "title": "Sample RPG MV",
    "engine": "rmmv",
    "core": "nxjs-mvz",
    "gamePath": "sdmc:/switch/nx.rpg/games/SampleMV",
    "contentRoot": "sdmc:/switch/nx.rpg/games/SampleMV/www",
    "savePath": "sdmc:/switch/nx.rpg/saves/demo-mv-01"
  },
  "assets": {
    "rtpPath": "",
    "fallbackFontPath": "sdmc:/switch/nx.rpg/fonts/NotoSansCJK-Regular.ttc"
  },
  "runtime": {
    "encoding": "utf-8",
    "rgssVersion": 0,
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
    "exitCombo": ["L", "R", "PLUS"]
  }
}
```

---

## 3. 核心返回协议（Core → Launcher）

当用户通过游戏内退出菜单或全局组合键（`exitCombo`）退出游戏时，核心必须按以下顺序收尾：

1. 确保存档缓冲区与 `logPath` 日志已 `fflush` / `fsync` 落盘。
2. 释放音频、图形（EGL/GLES/SDL/Skia）与文件系统句柄。
3. 若 `returnTo` 非空且文件存在，调用：
   ```c
   envSetNextLoad(manifest.returnTo, "sdmc:/switch/nx.rpg/nx.rpg.nro --from-core");
   ```
4. 正常 `return 0` 退出主函数，由 Homebrew Loader 接管并重新启动 `nx.rpg.nro`。

### 异常日志约定

若核心因致命错误（如缺失关键数据文件、脚本未捕获异常、内存不足）中止运行，应在退出前向 `logPath` 写入简明错误摘要：

```text
[nx.rpg][FATAL][core=nxjs-mvz][game=demo-mv-01] Uncaught ReferenceError: SomePluginAPI is not defined
```

Launcher 下次启动检测到 `--from-core` 或非空异常标记时，可在前端弹出友好的错误摘要卡片。
