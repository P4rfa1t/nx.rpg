# common — 跨模块共享协议、接口头文件与公共库

`common/` 是 `launcher/`、原生核心补丁层及宿主机单元测试（`tests/`）共同复用的 C++20 契约与基础库。设计上严格做到**平台无关逻辑与 `libnx` 系统调用解耦**，使得引擎识别、配置合并与 `launch.json` 序列化均可在普通桌面环境直接跑单元测试。

## 目录结构

```text
common/
├── CMakeLists.txt
├── schemas/                            # 权威 JSON Schema 定义（CI 强制校验）
│   ├── global-config.schema.json       # sdmc:/switch/nx.rpg/config/global.json
│   ├── game-config.schema.json         # sdmc:/switch/nx.rpg/config/games/<id>.json
│   ├── launch-manifest.schema.json     # sdmc:/switch/nx.rpg/runtime/launch.json
│   ├── library-cache.schema.json       # sdmc:/switch/nx.rpg/cache/library.json
│   └── upstreams-lock.schema.json      # third_party/upstreams.lock.json
├── include/nxrpg/                      # C++20 公共接口声明骨架
│   ├── types.hpp                       # 7 代引擎、3 大核心、CPU 档位、缩放与编码枚举
│   ├── paths.hpp                       # Switch SD 卡标准路径常量与大小写无关路径解析接口
│   ├── engine_detector.hpp             # 7 代 RPG Maker 自动检测、重命名茎推断与元数据提取接口
│   ├── launch_manifest.hpp             # LaunchManifest 数据结构、JSON 编解码与 CLI 参数提取
│   ├── config_merger.hpp               # 默认值 -> global.json -> games/<id>.json 合并规则接口
│   └── chainload.hpp                   # libnx envSetNextLoad 链式跳转与落盘屏障抽象
└── src/                                # Phase 1 实现源码目录
```
