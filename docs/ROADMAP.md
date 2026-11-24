# nx.rpg 分阶段开发路线图（Roadmap）

开发遵循 **“契约先行、由易到难、宿主机可测优先”** 的工程演进路线：
**Phase 0（架构与契约骨架） → Phase 1（公共库 + Borealis 前端 MVP + EasyRPG 闭环） → Phase 2（nx.js + MV/MZ Shim） → Phase 3（mkxp-z + 静态 Ruby MRI 移植） → Phase 4/5（全功能体验与合规发布流水线）**。

---

## Phase 0：项目架构、契约 Schema 与子模块骨架（已完成）

- [x] 确立 Borealis 统一前端 + 三大独立 `.nro` 核心链式加载隔离架构
- [x] 建立全部 5 份 JSON Schema（`global-config`、`game-config`、`launch-manifest`、`library-cache`、`upstreams-lock`）及自动化校验脚本 `scripts/validate-repo.py`
- [x] 建立 `third_party/upstreams.lock.json`，精确锁定三大上游仓库 Commit SHA 与补丁队列目录 `cores/*/patches/SERIES`
- [x] 完成 `common/`（C++20 公共接口声明）、`launcher/`（C++20 + Borealis 视图与服务骨架）、`cores/*`（CMake/Meson/TS/Ruby 预加载骨架）与 `tests/` 目录布局
- [x] 完成架构、7 代引擎检测、启动协议、输入/触控/软键盘、文件系统/加密/存档隔离、构建与 GPL 合规 CI 全套规范文档

---

## Phase 1：C++20 公共库、Borealis 前端 MVP 与 EasyRPG 闭环

**目标**：在宿主机跑通引擎识别单测，并在 Switch 实机/模拟器打通 `nx.rpg.nro` ↔ `cores/easyrpg.nro` 完整往返闭环。

- [ ] 实现 `common/src/`：
  - 7 代引擎识别器（含 MV `www/` 剥离、重命名 `execName` 推断、INI/LDB/JSON 标题解码）
  - 三层配置合并器 `ConfigMerger`、原子文件写入 `write_file_atomic` 与 `Chainloader`
- [ ] 实现 `tests/` 宿主机 CMake 单测套件并接入 `.github/workflows/validate.yml`
- [ ] 实现 `launcher/src/` MVP（Borealis 游戏卡片列表、`cache/library.json` 增量缓存、Applet 模式检测横幅、单游戏编码/缩放快速设置、`--from-core` 焦点恢复）
- [ ] 完成 `cores/easyrpg/patches/` 的 `0001`（`--nxrpg-manifest` + `envSetNextLoad`）与 `0002`（修复 `src/filefinder_rtp.cpp` Switch 自定义 RTP 路径失效）补丁
- [ ] 实机/模拟器验收：流畅运行 RM2000 与 RM2003（含共享 RTP、GBK/Shift_JIS 编码、独立存档与 `L+R+PLUS` 返回前端）

---

## Phase 2：nx.js 核心与 RPG Maker MV / MZ 兼容层

**目标**：基于 `TooTallNate/nx.js`（`switch-v8 >= 15.0.243-9`，`NXJS_FAT=1`）跑通 RPG Maker MV 与 MZ。

- [ ] 完成 `cores/nxjs-mvz/patches/0001-applet-bind-env-set-next-load.patch`，为 `nx.js` 导出 `Switch.setNextLoad()`
- [ ] 实现 `cores/nxjs-mvz/shim/src/` 各子模块并补齐 `vitest` 单测：
  - `vfs/`：大小写无关路径缓存 + `fetch` / `XMLHttpRequest` 本地重定向
  - `crypto/`：`.rpgmvp` / `.rpgmvo` / `.png_` / `.ogg_` 16 字节伪头与 XOR 解密
  - `dom/`：最小 DOM + PixiJS v4 `'webgl'` → `screen.getContext('webgl2')` 兼容桥 + 离屏 `Canvas` + 字体加载
  - `storage/` & `nwjs/`：关闭 NACP 账户存档弹窗，实现基于 `saves/<id>/` 的原子写 `localStorage` 与 `require('fs')` 沙箱
  - `input/` & `perf/`：手柄、掌机触屏（含双指取消）、`@nx.js/clkrst` CPU 升频与组合键返回
- [ ] 分别完成标准 **RPG Maker MV**（PixiJS v4）与 **RPG Maker MZ**（PixiJS v5 + `effekseer.wasm`）工程实机闭环验收
- [ ] 针对主流插件（YEP / VisuStella / Galv）迭代补齐高频 DOM / NW.js 边角接口

---

## Phase 3：mkxp-z 移植与 RGSS1 / RGSS2 / RGSS3 攻坚

**目标**：在 `devkitA64` 下交叉构建静态 Ruby MRI 3.1 与 `mkxp-z.nro`，打通 XP / VX / VX Ace。

- [ ] **Phase 3a**：按 `cores/mkxp-z/ruby-switch/` 规范编出含 `zlib` + CJK 编码表的静态 `libruby-static.a` 并在 Switch 验证 Ruby VM 与 ARM64 Fiber
- [ ] **Phase 3b**：基于 `switch-aarch64.ini` 与 `0001` 补丁打通 SDL2 + Mesa EGL/GLES2 + OpenAL Soft + PhysFS 静态链接，产出 `mkxp-z.nro`
- [ ] **Phase 3c**：完成 `0002` / `0003` 补丁及 `00_nxrpg_save_redirect.rb`、`01_nxrpg_win32_ext.rb`，实机跑通 RMXP / VX / VX Ace（含重命名 `.rgssad/.rgss2a/.rgss3a` 解包与存档隔离）
- [ ] **Phase 3d**：面向社区热门同人作品完善 `Win32API` 兼容垫片与大图集内存回收调优

---

## Phase 4：全平台体验打磨与诊断中心

- [ ] 三大核心打通 Switch 原生 `swkbd` 系统软键盘（主角起名、密码输入直接支持中文拼音/日文假名）
- [ ] 完善 Borealis 前端 `DiagnosticsTab`：一键查看 `runtime/last_run.log` 崩溃摘要、5 代共享 RTP 完整性检测、CJK 字体巡检与孤立 `.tmp` 清理
- [ ] 支持游戏封面缩略图异步解码缓存（含从加密包提取标题图缩略图）、收藏夹与游玩时长统计

---

## Phase 5：容器化矩阵 CI 与合规发布工程

- [ ] 上线基于 `devkitpro/devkita64` 的 GitHub Actions 四目标并行构建缓存流水线
- [ ] 实现一键打包脚本：同步产出 `nx.rpg-vX.Y.Z-switch.zip` 与满足 GPLv2/GPLv3 义务的 `nx.rpg-vX.Y.Z-gpl-sources.tar.zst`
- [ ] 建立公开游戏兼容性数据库与标准反馈 Issue 模板
