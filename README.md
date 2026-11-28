# nx.rpg

**nx.rpg** 是一款面向 **Nintendo Switch** 的全世代 RPG Maker 统一运行器（All-in-One Runner）：基于 **C++20 + Borealis** 的原生风格统一前端，通过 libnx `envSetNextLoad` 链式调度三个独立 `.nro` 核心，一台机器通吃 **RPG Maker 2000 / 2003 / XP / VX / VX Ace / MV / MZ** 共 7 代引擎作品。

> **当前状态**：Phase 1–4 代码全部完成并已通过实机初验（游戏启动、存档隔离、封面缓存、游玩时长、诊断中心、软键盘输入均可用）。持续迭代中：插件兼容性、发布打包流水线。

## ✨ 特性

- **7 代引擎全覆盖** —— 2k/2k3 走 LCF 事件解释器，XP/VX/VX Ace 跑真实 Ruby MRI 3.1 (RGSS 1/2/3)，MV/MZ 跑真 V8 + PixiJS，均为原生 Switch 交叉编译，非模拟器。
- **统一前端** —— Borealis 原生 UI：全部游戏 / 最近游玩 / 环境与日志自检 / 全局设置四个页签；封面缩略图缓存、收藏、累计游玩时长、按引擎筛选（ZL/ZR）。
- **进程级隔离** —— 每代引擎一个独立 `.nro` 核心，GPL（EasyRPG / mkxp-z）与 MIT（nx.js / 自有代码）代码永不同进程链接；核心独占全部内存，退出经 `envSetNextLoad` 秒回启动器并恢复上次焦点。
- **存档完全隔离** —— 所有代际的存档统一重定向到 `saves/<game_id>/`（RGSS `Save*.rxdata` / MV/MZ `localStorage` + `save/` 均含 Copy-on-Write 旧档回退），游戏目录运行期严格只读。
- **exFAT/FAT32 防损** —— 全部配置、缓存、清单与存档走 `<file>.tmp` → fflush → fsync → rename 原子写入；启动时自动清理崩溃残留的 `.tmp`。
- **按需 CPU 调频** —— 默认"跟随系统"（尊重系统 powerd 与自装超频 KIP），也可手动钳制 1020/1581/1785 MHz。
- **完整可观测性** —— 启动器日志 `logs/launcher.log` + **每游戏独立运行日志** `logs/games/<game_id>.log`（EasyRPG 全量 / mkxp-z stderr 镜像 / nxjs 带时间戳事件行），诊断页按游戏分组查看尾部，崩溃时前端自动弹窗定位。
- **中文化输入** —— 全部核心接入系统软键盘（swkbd），角色起名输入 CJK 无压力；界面内置 zh-CN / en-US 等五语言。

## 🎮 支持矩阵

| RPG Maker 世代 | 脚本 / 运行时 | 核心进程 | 上游基线（锁定 SHA） | Switch 底层栈 |
| :--- | :--- | :--- | :--- | :--- |
| **RM 2000 / 2003** | LCF 事件解释器 | `cores/easyrpg.nro` | [EasyRPG/Player](https://github.com/EasyRPG/Player) `a99ca834` (GPL-3.0) | EGL/OpenGL + `libnx` `audout` |
| **RM XP / VX / VX Ace** | Ruby MRI 3.1 (RGSS 1/2/3) | `cores/mkxp-z.nro` | [mkxp-z/mkxp-z](https://github.com/mkxp-z/mkxp-z) `a8f9d9ee` (GPL-2.0) | SDL2 + GLES2 + OpenAL Soft + PhysFS |
| **RM MV** | JS / PixiJS v4 | `cores/nxjs-mvz.nro` | [TooTallNate/nx.js](https://github.com/TooTallNate/nx.js) `462203af` (MIT) | V8 + Skia / GLES3 WebGL2 + FFmpeg `audren` |
| **RM MZ** | JS / PixiJS v5 + Effekseer WASM | `cores/nxjs-mvz.nro` | 同上 | 同上 + Application 模式 V8 WASM JIT |

上游源码经 `third_party/upstreams.lock.json` 锁定精确 Commit，全部修改以补丁队列（`cores/*/patches/SERIES`，当前 easyrpg×3 / nxjs-mvz×1 / mkxp-z×5）形式维护，构建从干净树按序重放，可复现。

## 📦 安装（玩家）

1. 下载 `nx.rpg-vX.Y.Z.zip`，其中包含启动器和三个核心。[GitHub Releases](https://github.com/P4rfa1t/nx.rpg/releases)
2. 将压缩包内的 `switch` 文件夹解压到 SD 卡根目录。首次运行会创建存档、日志等运行目录。完整目录结构如下。

   ```text
   sdcard:/
   └── switch/nx.rpg/
       ├── nx.rpg.nro              # 统一前端
       ├── cores/
       │   ├── easyrpg.nro         # RM2k / RM2k3 核心
       │   ├── nxjs-mvz.nro        # RM MV / MZ 核心
       │   └── mkxp-z.nro          # RM XP / VX / VXAce 核心
       ├── games/                  # 游戏放这里（每游戏一个文件夹）
       ├── rtp/{rm2000,rm2003,rmxp,rmvx,rmvxa}/   # 可选：共享 RTP 池
       ├── fonts/                  # 可选：NotoSansCJK-Regular.ttc CJK 回退字体
       ├── config/global.json      # 可选：全局配置（省略则用默认值）
       ├── cache/                  # 自动生成：库缓存 + 封面缩略图
       ├── saves/                  # 自动生成：按游戏隔离的存档
       ├── logs/                   # 自动生成：launcher.log + games/<game_id>.log 每游戏运行日志
       └── runtime/                # 自动生成：launch.json 启动清单
   ```

3. **务必用全内存模式启动**：从相册 hbmenu 启动会进入 Applet 模式（~440 MB，V8 JIT 与大型 RGSS 堆受限）；按住 **R** 键启动任意已安装游戏（Title Takeover）即获得 ~3.2 GB 全内存。前端标题栏会显示当前模式。
4. 放入游戏后启动 `nx.rpg.nro`，前端自动识别引擎世代并匹配核心，按 A 启动；游戏内按 **L + R + ＋** 返回前端。

### 不拔 SD 卡传输游戏

在 Launcher 的 **文件传输** 标签打开对应开关（可同时开启）：

- **USB MTP**：用支持数据传输的 USB 线连接可信电脑，在支持 MTP 的文件管理器里打开 `nx.rpg`。新游戏放到设备里的 `games/` 文件夹。
- **局域网 FTP**：在同一个可信局域网内，使用支持被动模式的 FTP 客户端连接页面显示的地址（端口 `5000`），匿名登录即可，无需用户名和密码。FTP 是明文协议，请勿转发端口或在公共网络使用。上传不会覆盖同名文件。

两个开关切换页签后仍会继续运行，直到手动关闭或退出 nx.rpg；启动游戏前会提示先关闭传输。两种方式看到的根目录都是 `switch/nx.rpg/`，可以访问其中的游戏、配置、存档、日志和核心，但不能访问这一级目录以外的 SD 卡内容。传输过程中开关下方会显示当前文件名与实时速率；传完后关闭开关，回到游戏库重新扫描。

<details>
<summary>📁 游戏目录形态与单文件归档（传输慢的解法）</summary>

- **2k/2k3**：`games/<名字>/RPG_RT.ldb`（+ `RPG_RT.lmt`）。EasyRPG 原生透明挂载 `.zip` / `.tar` / `.easyrpg`（`.lzh` 亦支持），可以把整个游戏压成一个 zip 放进 `games/`——zip 根目录直接是 `RPG_RT.ldb`（或 zip 内一层文件夹），几万个小文件变 1 个文件，拷卡时间缩短一个数量级。
- **XP/VX/VXAce**：`games/<名字>/Game.ini` + `Game.rgssad/.rgss2a/.rgss3a`（或解包后的 `Data/`/`Graphics/`）。带官方加密包的游戏资产天然就是单文件；解包版同样建议读卡器直拷。
- **MV/MZ**：`games/<名字>/www/index.html`（或包一层 `www`）。nx.js 运行时暂不支持 zip 挂载，必须散文件；强烈建议用 **USB 3.0 读卡器**直插电脑拷贝（避免 FTP/MTP 逐文件通道）。
- 游戏检测规则详见 [docs/ENGINE_DETECTION.md](docs/ENGINE_DETECTION.md)。

</details>

## 🛠 从源码构建（开发者）

宿主机需要 Docker（推荐，全部工具链已容器化）或本地 devkitPro 工具链（`scripts/check-env.sh` 自检）。

```bash
# 1. 校验仓库契约（JSON Schema、示例配置、锁文件、补丁队列、脚本语法）
make validate

# 2. 按锁定 SHA 检出三大上游核心到 third_party/（源码 gitignored）
make bootstrap-cores

# 3. 宿主机单元测试（无需 Switch 硬件；7 组 ctest）
make test-host

# 4a. 本地构建（Docker，一键产出全部 NRO）
docker run --rm -v "$(pwd):/work" -w /work --dns 223.5.5.5 nxrpg-devkita64:latest \
  bash -c "bash scripts/build-launcher-switch.sh && bash scripts/build-easyrpg-switch.sh && bash scripts/build-mkxp-z-switch.sh"
docker run --rm -v "$(pwd):/work" -w /work --dns 223.5.5.5 nxrpg-nxjs-build:latest \
  bash -c "bash scripts/build-nxjs-mvz-switch.sh"

# 4b. 或者直接推 main —— 每个构建目标有独立 workflow（按路径过滤，只重编译改动的目标）：
#      build-launcher.yml / build-easyrpg.yml / build-nxjs-mvz.yml / build-mkxp-z.yml
#      打 v* tag 会触发 release.yml：全量构建 + 单个安装 ZIP + GPL 源码归档 → GitHub Releases
```

构建脚本特性：每次从干净上游树按序重放补丁队列（`git reset --hard` + `git clean`），产物直接落到 `sdcard/switch/nx.rpg/` 对应位置。工具链镜像、Meson 交叉文件与已知坑位清单见 [docs/BUILD_AND_CI.md](docs/BUILD_AND_CI.md)。

## 📚 文档

| 文档 | 内容 |
| :--- | :--- |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | 总体拓扑、Applet vs 全内存 V8/WASM 机制、CPU 调频档位、SD 布局 |
| [docs/ENGINE_DETECTION.md](docs/ENGINE_DETECTION.md) | 7 代引擎识别优先级、`www/` 解包、重命名 `execName` 推断、编码与 RTP 探测 |
| [docs/LAUNCH_PROTOCOL.md](docs/LAUNCH_PROTOCOL.md) | 三层配置合并、每核心 argv 规则（含 nx.js 禁传 `argv[1]` 铁律）、回跳协议与运行日志 |
| [docs/INPUT_AND_UI_SPEC.md](docs/INPUT_AND_UI_SPEC.md) | 跨 7 代手柄按键矩阵、触屏双指取消、swkbd 软键盘、前端交互流 |
| [docs/FILESYSTEM_AND_SAVES.md](docs/FILESYSTEM_AND_SAVES.md) | FAT32/exFAT 原子写防损、大小写无关 VFS、7 代资产解密、存档 CoW 隔离 |
| [docs/BUILD_AND_CI.md](docs/BUILD_AND_CI.md) | 容器化工具链、TitleID/图标规范、GPL 合规源码归档流水线 |
| [docs/ROADMAP.md](docs/ROADMAP.md) | Phase 0–5 实施路线图与当前进度 |
| [docs/HANDOFF.md](docs/HANDOFF.md) | 跨会话交接摘要（实测结论与避坑清单） |

## 🗺 路线图

- [x] Phase 1 — EasyRPG 核心（RM2k/2k3）：manifest 解析、clkrst 调频、自定义 RTP、swkbd、回跳
- [x] Phase 2 — nx.js MV/MZ 核心：TypeScript shim（大小写无关 VFS、RPGMV 加密解密、NW.js 沙箱、存档隔离、软键盘）
- [x] Phase 3 — mkxp-z 核心（XP/VX/VXAce）：静态 Ruby 3.1、存档重定向、Win32API 垫片、swkbd 绑定
- [x] Phase 4 — 统一前端打磨：封面缓存、游玩时长、诊断中心、完整日志、单列布局
- [x] Phase 5 — CI/CD：四作业并行构建 + 全仓契约校验 workflow
- [ ] 实机全面验收（MV/MZ Effekseer、插件重度游戏）
- [ ] 插件兼容性迭代（YEP / VisuStella / Galv 常用插件适配）
- [x] 一键发布打包（单个安装 ZIP 与 GPL 源码包）
- [ ] 兼容性数据库

## ⚠️ 使用须知

- 请使用**自购正版**的 RPG Maker 制作或获得授权的游戏；本仓库不包含、不分发任何商业游戏、官方 RTP 素材或专有字体。
- SD 卡建议 FAT32（兼容性最好）；exFAT 受支持但务必保持系统与 Atmosphère 为新版本。
- 2k/2k3 游戏若无自带 RTP，需将官方 RTP 文件放入 `rtp/rm2000` / `rtp/rm2003`（带 `FullPackageFlag=1` 的游戏无需 RTP）。

## 📄 许可证

- 本仓库自有代码（`common/`、`launcher/`、`cores/nxjs-mvz/shim/`、`scripts/`、文档与 Schema）采用 **[MIT License](LICENSE)**。
- 三大上游核心作为**独立 `.nro` 进程**运行，永不与本仓库代码或彼此静态/动态链接，严格遵循各自许可证：EasyRPG Player **GPL-3.0**、mkxp-z **GPL-2.0**、nx.js **MIT**；详见 [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md)。
