# tests — 宿主机无硬件单元测试与合成夹具架构

Switch 实机/模拟器往返调试成本较高，因此 `nx.rpg` 将所有**不依赖 `libnx` 硬件句柄**的核心逻辑集中在宿主机测试套件中验证：

## 测试分层

1. **仓库契约静态校验（已上线 CI）**
   - 脚本：[scripts/validate-repo.py](../scripts/validate-repo.py)
   - 内容：校验所有 JSON Schema 合法性、实例配置与 Schema 字段/枚举一致性、`third_party/upstreams.lock.json` 格式、`cores/*/patches/SERIES` 完整性及 Shell 脚本语法。

2. **C++20 公共库宿主机单测（Phase 1，`tests/CMakeLists.txt`）**
   - 7 代引擎识别优先级与边界场景（如目录内同时残留多版本文件、MV `www/` 嵌套、`<CustomName>.ini` + `<CustomName>.rgss3a` 重命名识别、Shift_JIS / GBK `Game.ini` 标题解码）。
   - 三层配置合并（内置默认 → `global.json` → `config/games/<id>.json` → `LaunchManifest`）。
   - 链式加载 `argv` 构造验证（重点验证 `nxjs-mvz` 不注入 `argv[1]`）。

3. **TypeScript MV/MZ Shim 单元测试（Phase 2，`cores/nxjs-mvz/`）**
   - 使用 `vitest` 在 Node.js 下测试 `.rpgmvp`/`.png_` 16 字节头 XOR 解密器、大小写无关 VFS 映射表及 `StorageManager` 路径沙箱。
