# 构建工具链、容器化编译与发布合规工程规范（Build & CI）

`nx.rpg` 同时协调四个独立构建目标（1 个前端 + 3 个异构引擎核心）。本文档规范统一的容器化交叉编译环境、TitleID/NACP 约定、CI 流水线以及满足 GPLv2/GPLv3 要求的自动化发布产物打包流程。

---

## 1. 四大构建目标与工具链总览

| 构建目标 | 输出文件 | 构建系统 | 关键依赖要求 |
| :--- | :--- | :--- | :--- |
| **Launcher** | `nx.rpg.nro` | CMake + Ninja (`Switch.cmake`) | `devkitA64`, `libnx`, `Borealis` (`switch-glfw`/`deko3d`/`glad`), `nlohmann_json` |
| **Core: EasyRPG** | `cores/easyrpg.nro` | CMake + Ninja (`switch-release`) | `devkitA64`, `libnx`, `liblcf >= 0.8.1`, `switch-mesa` (EGL/`glad`), `freetype`, `harfbuzz`, `pixman`, `mpg123`, `ogg`/`vorbis`, `wildmidi`/`fluidsynth` |
| **Core: nxjs-mvz** | `cores/nxjs-mvz.nro` | GNU Make + Node 22 + `pnpm` | `devkitA64`, `libnx`, **`switch-v8 >= 15.0.243-9`**（修复全内存 JIT W^X GC 崩溃）, `skia`, `ffmpeg`, `NXJS_FAT=1` |
| **Core: mkxp-z** | `cores/mkxp-z.nro` | Meson `>= 1.4.0` + Ninja (`switch-aarch64.ini`) | `devkitA64`, `libnx`, Host `ruby 3.1` + 交叉静态 `libruby-static.a`, `switch-sdl2`, `sdl2_image`, `sdl2_ttf`, `openal-soft`, `physfs`, `pixman`, `uchardet`, `xxhash` |

---

## 2. 上游核心锁定与补丁队列工作流

所有上游核心版本统一由 [third_party/upstreams.lock.json](../third_party/upstreams.lock.json) 精确锁定到 40 位 Commit SHA，杜绝“上游分支漂移导致本地补丁失配”：

```text
third_party/upstreams.lock.json
        │
        ├── 1. bash scripts/bootstrap-cores.sh
        │      检出精确锁定的 Commit SHA 到 third_party/<Core>/
        │
        └── 2. bash scripts/apply-patches.sh [all|easyrpg|nxjs-mvz|mkxp-z]
               按 cores/<core>/patches/SERIES 顺序执行 git am / git apply --check
```

### 升级上游核心基线的标准步骤

1. 运行 `bash scripts/bootstrap-cores.sh --latest <core>` 拉取目标分支最新提交。
2. 在 `third_party/<Core>/` 中变基验证 `cores/<core>/patches/` 下的所有补丁，必要时重新导出 `git format-patch`。
3. 更新 `third_party/upstreams.lock.json` 中的 `commit` 与 `updatedAt`，运行 `python3 scripts/validate-repo.py` 校验通过后提交。

---

## 3. Switch NRO / NACP / 图标打包硬性规范

Switch `hbmenu` 与 Horizon OS 对 `.nro` 包装元数据有严格格式要求：

1. **图标规格（`assets/icon.jpg`）**：
   - 分辨率**必须严格为 `256 × 256` 像素**。
   - 编码**必须为标准基线 JPEG（Baseline DCT，严禁 Progressive JPEG 渐进式扫描与 EXIF 缩略图块）**，否则 `elf2nro` 或 `hbmenu` 会拒绝显示图标甚至崩溃。
2. **TitleID 规划（避免与系统或社区自制软件冲突）**：
   - `nx.rpg.nro`（前端）：`016e787270670000`（ASCII `"nxrpg"` 十六进制前缀）
   - `cores/nxjs-mvz.nro`：`016e787270670001`（且强制 `userAccountSaveDataSize = 0`）
   - `cores/easyrpg.nro`：`016e787270670002`
   - `cores/mkxp-z.nro`：`016e787270670003`

---

## 4. GitHub Actions CI 分阶段流水线设计

- **Stage A — 仓库契约与宿主机测试（每次 Push / PR 必跑，秒级完成）**：
  - 工作流：[.github/workflows/validate.yml](../.github/workflows/validate.yml)
  - 执行 `python3 scripts/validate-repo.py`（全量 JSON Schema 结构与枚举校验、锁文件 SHA 校验、补丁队列引用校验）与 `bash -n scripts/*.sh`。
  - Phase 1 起追加宿主机 CMake `ctest`（跑 7 代引擎识别器与配置合并器单元测试）。
- **Stage B — 容器化 Switch 交叉编译矩阵（Phase 1~3 分核心点亮）**：
  - 基于官方 `devkitpro/devkita64` 容器镜像，拆分为 4 个并行且带 ccache / portlibs 缓存的独立 Job：`build-launcher`、`build-core-easyrpg`、`build-core-nxjs-mvz`、`build-core-mkxp-z`。
- **Stage C — 发布打包与 GPL 合规源码归档（Tag `v*` 触发）**：
  1. `nx.rpg-vX.Y.Z-switch.zip`：解压即用的 `switch/nx.rpg/` 目录结构（含 `nx.rpg.nro`、`cores/*.nro`、`config/global.example.json`、`LICENSE`、`THIRD_PARTY_LICENSES.md`）。
  2. `nx.rpg-vX.Y.Z-gpl-sources.tar.zst`：自动归档本次构建实际使用的 `upstreams.lock.json`、对应版本的 `EasyRPG/Player`（GPL-3.0）与 `mkxp-z` + `Ruby`（GPL-2.0）源码快照及 `cores/*/patches/` 完整补丁集，100% 履行 GPL 二进制分发附带完整对应源码（Corresponding Source）的义务。
