# cores/easyrpg — EasyRPG Player 核心适配层

- **上游锁定基线**：`EasyRPG/Player` @ `a99ca834b8560f6a39a4df5de2d3972523ba2f30`（见 `third_party/upstreams.lock.json`）
- **上游 Switch 原生后端**：
  - 图形：`src/platform/switch/ui.cpp`（原生 EGL / OpenGL + `glad`，内嵌 `resources/switch/touch_ui.png`）
  - 音频：`src/platform/switch/audio.cpp`（原生 `libnx` `audout`，48 kHz 立体声 16-bit PCM 双缓冲工作线程）
  - 构建：CMake Preset `switch-release`（`$DEVKITPRO/cmake/Switch.cmake` + `$EASYRPG_BUILDSCRIPTS/switch`，依赖 `liblcf >= 0.8.1`）
- **产物重命名映射**：上游构建产物 `easyrpg-player.nro` → 安装至 `sdmc:/switch/nx.rpg/cores/easyrpg.nro`

## 目录结构

```text
cores/easyrpg/
├── README.md
├── cmake/
│   └── nxrpg-easyrpg-flags.cmake   # 叠加至 switch-release 的编译定义与输出约定
└── patches/
    ├── SERIES                      # 补丁顺序清单（由 scripts/apply-patches.sh 读取）
    └── README.md                   # 3 个补丁的精确触点说明（含 filefinder_rtp.cpp 缺陷修复）
```
