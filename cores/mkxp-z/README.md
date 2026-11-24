# cores/mkxp-z — mkxp-z Switch / devkitA64 移植层

- **上游锁定基线**：`mkxp-z/mkxp-z` @ `a8f9d9ee331034b7778d0f6c3a5603de8b87388f`（v2.4.2-dev，要求 `meson >= 1.4.0`）
- **核心优势复用（上游已原生内置）**：
  1. **`pathCache: true`**：上游已默认构建全量小写路径索引缓存，天然兼容 Windows 游戏大小写不一致问题。
  2. **`execName`**：原生支持重命名后的 `<Stem>.ini` 与 `<Stem>.rgssad/.rgss2a/.rgss3a` 匹配，直接由 `launch.json -> game.execName` 注入。
  3. **`scripts/preload/win32_wrap.rb`（CC0）**：上游已内置 `User32` 键盘扫描码、鼠标坐标、窗口句柄与容错（`TOLERATE_ERRORS = true`）预加载脚本；本目录 `preload/` 在其之上扩展 `Kernel32` INI 读写与存档目录重定向。

## 目录结构

```text
cores/mkxp-z/
├── README.md
├── cross/
│   └── switch-aarch64.ini              # Meson >= 1.4.0 面向 devkitA64/libnx 的交叉编译文件
├── ruby-switch/
│   ├── README.md                       # 静态 libruby 3.1 (aarch64-none-elf + arm64 coroutine) 交叉构建说明
│   └── ext-static.list                 # 必须静态链接进 libruby.a 的 RGSS 与 CJK 编码扩展白名单
├── preload/
│   ├── 00_nxrpg_save_redirect.rb       # 将 RGSS1/2/3 默认写往游戏目录的 Save*.rxdata/rvdata(2) 重定向至 saves/<id>/
│   └── 01_nxrpg_win32_ext.rb           # 在上游 win32_wrap.rb 基础上补充 Kernel32 INI / 系统度量 Stub
└── patches/
    ├── SERIES                          # mkxp-z 补丁顺序清单
    └── README.md                       # Switch SDL2+EGL/GLES2、launch.json 转 Config 与链式退出补丁说明
```
