# nx.rpg Handoff — 项目交接文档

> 更新日期：2026-11-26。写给接手本项目的下一个模型/开发者。
> 仓库：`P4rfa1t/nx.rpg`（main，D:\nx.rpg）。通信语言：简体中文。

---

## 1. 项目一句话

Nintendo Switch 多引擎 RPG Maker 运行器：Borealis C++20 启动器 `nx.rpg.nro` 通过
`envSetNextLoad` 链式加载三个独立 NRO 核心，彼此 GPL 隔离、绝不静态链接：

| 核心 NRO | 引擎 | 上游（锁定于 `third_party/upstreams.lock.json`） | 构建容器 |
|---|---|---|---|
| `cores/easyrpg.nro` | RM 2000/2003 | `EasyRPG/Player` @ `a99ca834` | `nxrpg-devkita64:latest` |
| `cores/nxjs-mvz.nro` | RM MV/MZ | `TooTallNate/nx.js` @ `462203af` | `nxrpg-nxjs-build:latest` |
| `cores/mkxp-z.nro` | RM XP/VX/VXAce | `mkxp-z/mkxp-z` @ `a8f9d9ee` | `nxrpg-devkita64:latest` |

对上游的一切修改都在 `cores/<core>/patches/` 里按 `SERIES` 顺序排队；
`third_party/` 的 checkout 已 gitignore，靠 `scripts/bootstrap-cores.sh` 恢复。

## 2. 当前状态（2026-11-26）

**Phase 0 完成，Phase 1 进行中。真机（Switch 硬件）调试是当前主战场。**

### 已在真机验证成功

- 启动器 → 链式加载三个核心全部打通；
- **easyrpg**：多款 RM2k/2k3 游戏可跑（Akumu Oni、Final Fantasy Blackmoon
  Prophecy 标题画面正常）；
- **nxjs-mvz**：MV 游戏可进入（The Witch's House MV 还有崩溃，见 §5）；
- **mkxp-z**：VXAce 游戏（hc / hc_ep1）此前在 `ruby_init` 内 canary smash，
  **v6 已修复待真机确认**（见 §3.3）。

### 本轮（round-5）修复与产物

| 产物 | 版本 | sha256 前 8 位 | 内容 |
|---|---|---|---|
| easyrpg.nro | v3 | `b8f1b8fb` | patch 0005：音频缓冲 4096→8192 samples（85→170ms），解决流式 OGG/MP3 因 SD 读卡停顿导致的**爆音**；音频线程栈 64K→128K |
| mkxp-z.nro | v6 | `cc5979f2` | patch 0001 重生成：向 ruby 3.1 configure 注入 `stack_protector=no`，**消除 Ruby 自带的 -fstack-protector-strong canary**（v4 NRO 含 1 处 "stack smashing" 消息串，v6 为 0，已二进制验证） |
| nxjs-mvz.nro | v4 | （构建中/刚完成，见 §3.2） | shim 错误可见性：动态 script 错误带 `e.target._url`、脚本先于 onload 执行、`SceneManager.catchException`/`unhandledrejection` 镜像原始异常进 per-game 日志 |

三份 NRO 均已复制到 **`C:\Users\P4rfa1t\Desktop\nx.rpg\cores\`**（用户与 Switch SD
同步的目录）。**mkxp-z v6 与 nxjs v4 尚未经真机测试**——下次用户测完要收回日志。

### Git 状态

- `main` @ `76138ec`（含 round-4 `e31a3df` + round-5 `76138ec`），**已推送 origin**。
- 工作树干净（`.tmp-probe/`、`build/` 已 gitignore）。

## 3. 关键技术事实（踩坑实录，勿重复踩）

### 3.1 Horizon fsdev / libnx

- **第二次 fopen 已打开的文件会失败**：任何"长驻 FILE* + 其它线程再 fopen 日志"
  的写法都会让后者静默失败。所有 boot 插桩用 **open-append-close 每次一调**。
- **`rename()` 不覆盖已存在目标**（非 POSIX）：`write_file_atomic` 必须
  write `.tmp` → fsync → `remove(旧)` → `rename`。
- SD 上游戏目录运行期**只读**；所有 JSON 用原子写。

### 3.2 mkxp-z（XP/VX/VXAce 核心）——ruby_init 崩溃全链

真机日志三段式演进，最终根因有两层：

1. **RGSS 线程栈太小**：`SDL_CreateThread` 走 libnx 默认栈，Ruby VM 直接
   canary smash。修复 = `SDL_CreateThreadWithStackSize(fn, "rgss", 8*1024*1024,
   &rtData)`（patch 0006）。
2. **Ruby 自己的 canary**：8MiB 栈后仍 smash。证据链：
   - 给 cross ini 的 meson `c_args` 加 `-fno-stack-protector` 后 NRO **逐字节相同**
     （sha256 db71d867 == db71d867）→ devkitA64 默认不给 mkxp-z C++ 代码上 canary；
   - Ruby 3.1.3 `configure.ac` 765-783 行：探测 `-fstack-protector-strong` 并写入
     XCFLAGS/XLDFLAGS/LDFLAGS；
   - Ruby 是 **autoconf 外部项目，meson c_args 传不进去**；
   - **唯一入口**是 `subprojects/packagefiles/ruby-3.1/meson.build` 里
     `configure_options` 的 `env:` 字典：注入 `'stack_protector': 'no'` ——
     configure.ac 770 行 `AS_IF([test -z "${stack_protector+set}"])` 看到变量已设
     就整段跳过探测。已并入 patch 0001（2026-11-26 重生成，718 行）。
   - 二进制验证：v4 NRO 内含 1 处 `"stack smashing detected"` 消息串，v6 为 0。

**mkxp-z 的 boot 插桩**（patch 0006）：`nxrpg_boot_v()` 三层
（boot=main 流程、boot2=binding-mri.cpp 的 ruby_sysinit→ruby_init→ruby_options→
ruby_exec_node→mriBindingInit、boot3=sharedstate.cpp initFontSets/Font::initDefaults），
每条消息 open-append-close 写 `logs/mkxp-boot.log`（多游戏共享，带进程内
`--nxrpg-manifest` 参数定位是哪一次启动）。

### 3.3 easyrpg（2k/2k3 核心）

- Switch 音频栈：`audout` 2×4096 samples@48kHz S16。**爆音根因**：流式解码
  OGG/MP3 时 SD 读卡停顿 >85ms → underrun。v3 改 8192 samples + 线程栈 0x20000。
- **日志镜像**（patch 0004）：原 LogCallback 只镜像 Error 级 → per-game 日志 0 字节；
  现在**所有级别**都镜像到 `logs/games/<id>.log`。boot 追踪写 `logs/easyrpg-boot.log`。
- **RTP**：上游 `filefinder_rtp.cpp` 在 `#ifdef PLAYER_NINTENDO` 提前 return，令
  `--rtp-path` 失效 → patch 0002 保留。共享 RTP 在 `sdmc:/switch/nx.rpg/rtp/{rm2000,rm2003}`。
- **触屏数字键盘**：`touch_ui.png`（编译进 `touch_ui_png.h`）是 RM "输入数字"事件
  的屏上小键盘 —— 手持模式非 docked 且 `touch_ui` 开启时绘制：左条(x<160) 1-8、
  右条(x≥1120) 触屏/N9/N0/./+/-/*//；easyrpg 菜单 Video→touch UI 可关，
  开 stretch 时自动锁定隐藏。

### 3.4 nx.js shim（MV/MZ 核心）

- **V8 `console.error` 不保证进 nxjs-debug.log** —— shim 所有诊断走 `runLog`
  回调（`appendRunLog` 直写 `logs/games/<id>.log`）。这是 v4 的核心改动。
- Witch's House MV 崩溃链：`printError → _clearUpperCanvas → getContext undefined`
  —— **错误画面自身先炸**，原始异常被吞。v4 的 catchException 包装会先记
  原始异常再放行。
- MV 的 `PluginManager.onError` 读 `e.target._url` → error 事件必须带
  `{target, currentTarget, error}`。
- 动态 `<script>` 必须先于 `window.onload` 执行（`queueMicrotask` 让位）。
- 打包铁律：`NXJS_FAT=1`、NACP `"userAccountSaveDataSize": 0`（否则弹账号选择器/
  写 NAND）；`argv[1]` 绝不能传给 nxjs（它把 argv[1] 当 JS 入口覆盖），
  manifest 走 `sdmc:/switch/nx.rpg/runtime/launch.json`。

### 3.5 构建配方（Docker）

- 容器 FS **ephemeral**：`/tmp` 不持久，`docker run --rm` 每次全新。构建脚本
  已自包含（stage 到 /tmp、打全 SERIES、meson、elf2nro、产物写回 /repo）。
- Git Bash 里必须 `export MSYS_NO_PATHCONV=1` 再 `docker run -v D:/nx.rpg:/repo`。
- `nxrpg-devkita64:latest` 需 `--dns 223.5.5.5`（SDL_sound tarball 下载）。
- **nx.js 重建前必删** `third_party/nx.js/{build/,nxjs.elf,nxjs.nacp,nxjs.nro,
  nxjs.nso,nxjs.pfs0,nxjs.npdm}`：残留 .d/.o 里是旧容器的 `/work` 路径，make 报
  `No rule to make target '/work/...'`。
- **改多文件 patch 的重放配方**（勿手编 hunk header）：
  1. 容器里 clone `third_party/<repo>` → checkout 锁定 SHA；
  2. 全树 `sed -i 's/\r$//'`（跳过二进制）→ `git add -A; git commit -qm norm`；
  3. `git apply` 旧 patch → commit；
  4. 跑 `.tmp-probe/edit*.py`（exact-string assert + replace，文件里 tab 要跟原文件）→ commit；
  5. `git diff base..HEAD` = 新 patch；对 fresh clone 跑**全 SERIES `--check`** 验证。
  - 容器里 commit 前要设 `GIT_AUTHOR_NAME/EMAIL`。
- Ruby tarball 预下载进 `subprojects/packagecache/`（sha256 与 wrap 一致）。
- 全部命令记录在 `scripts/build-{mkxp-z,easyrpg,nxjs-mvz}-switch.sh`，
  外层调用模式见 `.tmp-probe/` 里的历史（已 gitignore）或 memory 文件。

### 3.6 仓库纪律

- 改任何 schema/manifest 字段 → 同步 `common/schemas/`、C++/TS 头、
  `docs/LAUNCH_PROTOCOL.md`，过 `python3 scripts/validate-repo.py`。
- 文档一律简体中文；`.sh` 保持 LF（`.gitattributes` 强制）。
- 破坏性 git 操作、改仓库可见性要先问用户。

## 4. 日志体系（真机 → 用户 → 我）

```
C:\Users\P4rfa1t\Desktop\nx.rpg\logs\   ← 用户的 Switch SD logs 目录（每轮测试后收回）
├── launcher.log          # Borealis 启动器
├── mkxp-boot.log         # mkxp-z 全局 boot 追踪（共享，靠参数区分启动次数）
├── easyrpg-boot.log      # easyrpg 全局 boot 追踪
├── nxjs-debug.log        # nx.js V8 console（不完整，别依赖）
└── games/<id>.log        # 每游戏运行日志（三核心都写这里）
```

调试循环：改 patch → 容器重建 NRO → 复制到 `Desktop/nx.rpg/cores/` →
用户拷到 SD 真机跑 → 收回 `logs/` → 我读日志定位。

## 5. 待办（按优先级）

1. **等用户真机测 v6**（mkxp-z ruby_init 能否过 "ruby_init ok"）——
   看 `logs/mkxp-boot.log` 尾行 + `logs/games/hc-*.log`。
2. **等用户真机测 nxjs v4** —— The Witch's House MV 的 `logs/games/
   the-witch-s-house-mv-*.log` 里 v4 会先记下**原始异常**再进错误画面
   （v3 里它被 printError 的二次崩溃吞掉）。
3. **easyrpg v3 爆音是否消失**（Blackmoon Prophecy）。
4. 三个都确认后：把临时的 `'stack_protector': 'no'` 从"诊断"转"定论"，可考虑
   更精确的方案（如只给 ruby 加 `-fno-stack-protector` 但保留 libnx 侧防护）——
   亦可接受现状（Switch 上无 guard page 假设的 SSP 本就无意义）。
5. 低优先级：rm2k3 最小测试夹具（需 liblcf）；easyrpg.nro 的 CI strip 步骤。
6. 若 nxjs v4 日志显示 MV 原始异常在 VFS/字体/纹理路径，按异常位置修 shim
   （`cores/nxjs-mvz/shim/src/`，改完 `pnpm test`（10 用例）+ 重打包）。

## 6. 长文件/长会话生存指南

- 改 patch 用 §3.5 的重放配方；**永远不要手编 hunk 行数**。
- heredoc 里嵌 python 时单引号会打碎 bash 引号——把编辑逻辑写成
  `.tmp-probe/*.py` 文件，容器里 `python3 /repo/.tmp-probe/x.py`。
- 背景构建用 `run_in_background` + 输出文件尾行 marker（如 `V6-BUILD-DONE`），
  完成会有 task-notification，**不要轮询**。
- 每轮收尾：commit（中文无关、英文正文）→ `git push origin main` →
  部署 NRO 到 Desktop cores → 关闭信息里给用户下一步。
- 更多细节见记忆文件：`memory/docker-ci-recipes.md`。
