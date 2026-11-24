# cores/nxjs-mvz — nx.js + RPG Maker MV / MZ 核心适配层

- **上游锁定基线**：`TooTallNate/nx.js` @ `462203af39179fb630ae7fe981f3361b7147cdfb`（需配合 `switch-v8 >= 15.0.243-9`）
- **打包模式约束**：必须使用 **`NXJS_FAT=1` (`--fat`)** 自包含打包为 `sdmc:/switch/nx.rpg/cores/nxjs-mvz.nro`（禁用默认的 `--slim` 外部 `sdmc:/nx.js/` 依赖）。
- **NACP 关键配置**：在 `package.json` 中显式声明 `"userAccountSaveDataSize": 0`，禁用 `nx.js` 默认挂载 Switch 系统账户存档及弹出用户头像选择框的行为，改由 `shim/src/storage/` 将 `localStorage` 与文件存档统一持久化到 `sdmc:/switch/nx.rpg/saves/<game_id>/`。
- **启动参数陷阱规避**：`nx.js` 会将 `Switch.argv[1]` 解释为入口 JS 脚本路径而非普通 CLI 标志！因此 Launcher 启动 `nxjs-mvz.nro` 时**不传 `argv[1]`**，由内嵌于 RomFS 的 `romfs:/main.js`（由 `shim/src/index.ts` 打包生成）直接固定读取 `sdmc:/switch/nx.rpg/runtime/launch.json`。

## 目录结构

```text
cores/nxjs-mvz/
├── package.json                        # NACP 配置 (userAccountSaveDataSize=0) 与 --fat 打包脚本
├── tsconfig.json                       # TypeScript 编译配置
├── patches/
│   ├── SERIES                          # nx.js C++ 补丁清单
│   └── README.md                       # source/applet.cc 暴露 envSetNextLoad 绑定说明
└── shim/src/                           # MV / MZ 浏览环境与 NW.js 兼容垫片接口骨架
    ├── index.ts                        # RomFS 引导入口 (romfs:/main.js)
    ├── types.ts                        # LaunchManifest TS 契约与运行时上下文类型
    ├── dom/index.ts                    # window/document 最小树 + PixiJS v4 WebGL1->WebGL2 适配桥
    ├── vfs/index.ts                    # 大小写无关 SD 路径索引 + XHR/fetch 拦截器
    ├── crypto/rpgm-decryptor.ts        # .rpgmvp/.rpgmvo/.png_/.ogg_ 16 字节头 XOR 解密器接口
    ├── nwjs/index.ts                   # NW.js (require('fs'/'path'), process.mainModule) 沙箱垫片
    ├── storage/index.ts                # SD 卡隔离目录 localStorage & StorageManager 原子写入后端
    ├── input/index.ts                  # 手柄->键盘事件、触屏->Touch/Mouse、swkbd 起名与组合键退出
    └── perf/index.ts                   # @nx.js/clkrst CPU 超频管理与纹理/V8 内存压力回收
```
