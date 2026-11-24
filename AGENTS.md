# AGENTS.md

Repository guidance for AI coding agents and human contributors working on **nx.rpg** — a Nintendo Switch multi-engine RPG Maker runner supporting RPG Maker 2000, 2003, XP, VX, VX Ace, MV, and MZ.

---

## 1. Project Overview & Current Phase

- **Current Phase**: `Phase 0 Complete` (architecture specs, 5 JSON Schemas, upstream SHA lockfile, C++20/Borealis/TS/Ruby skeletons, and contract validator). Ready to begin `Phase 1` implementation.
- **Target Platform**: Nintendo Switch (`Horizon OS` / `libnx` / `devkitA64`, `aarch64-none-elf`).
- **Unified Frontend (`nx.rpg.nro`)**: **C++20 + Borealis** (`launcher/`), TitleID `016e787270670000`, backed by incremental `cache/library.json` metadata caching.
- **Three Pinned Upstream Cores** (locked in `third_party/upstreams.lock.json`, checkouts gitignored):
  1. **EasyRPG Player** (`EasyRPG/Player` @ `a99ca834...`, C++ / CMake `switch-release` / EGL+`glad` + `libnx` `audout` / GPL-3.0) → RM 2000 (`rm2k`) & RM 2003 (`rm2k3`) → `cores/easyrpg.nro`
  2. **nx.js** (`TooTallNate/nx.js` @ `462203af...`, `switch-v8 >= 15.0.243-9` + Skia GPU + GLES3 WebGL2 + FFmpeg `audren` + WASM / MIT) → RM MV (`rmmv`) & RM MZ (`rmmz`) → `cores/nxjs-mvz.nro`
  3. **mkxp-z** (`mkxp-z/mkxp-z` @ `a8f9d9ee...`, Meson `>= 1.4.0` + static Ruby MRI 3.1 + SDL2 + Mesa GLES2 + OpenAL Soft + PhysFS / GPL-2.0) → RM XP (`rmxp`), VX (`rmvx`), VX Ace (`rmvxa`) → `cores/mkxp-z.nro`

Read the engineering specs before implementing features:
- `docs/ARCHITECTURE.md` — Multi-NRO chainloading, Applet vs Application V8/WASM memory regimes, `clkrst` CPU profiles, SD layout
- `docs/ENGINE_DETECTION.md` — 7-engine detection precedence, `www/` unwrapping, renamed `execName` stem inference, encoding & RTP detection
- `docs/LAUNCH_PROTOCOL.md` — 3-layer Config Merger, per-core `argv` rules, `launch.json` contract & return flow
- `docs/INPUT_AND_UI_SPEC.md` — 7-engine button matrix, two-finger touch cancel, `swkbd` software keyboard, Borealis UI flows
- `docs/FILESYSTEM_AND_SAVES.md` — FAT32 vs exFAT atomic writes, case-insensitive VFS, 7-engine decryption, Copy-on-Write save isolation
- `docs/BUILD_AND_CI.md` — Docker `devkitpro/devkita64` matrix, NRO/NACP/256×256 baseline JPEG icon rules, GPL source bundling
- `docs/ROADMAP.md` & `docs/cores/{easyrpg,nxjs-mvz,mkxp-z}-plan.md`

---

## 2. Non-Negotiable Architectural & Upstream Invariants

1. **Strict Multi-NRO Isolation (`envSetNextLoad`)**
   - **Never** statically or dynamically link `EasyRPG/Player` (GPL-3.0) and `mkxp-z` (GPL-2.0) into one binary, nor merge the runtimes into `nx.rpg.nro`.
   - **Critical `nx.js` `argv` Rule**: `TooTallNate/nx.js` interprets `Switch.argv[1]` as the JS entrypoint path override! `Chainloader` MUST pass `--nxrpg-manifest <path>` ONLY to `easyrpg.nro` and `mkxp-z.nro`, and pass **no `argv[1]`** to `nxjs-mvz.nro` (whose embedded `romfs:/main.js` reads `sdmc:/switch/nx.rpg/runtime/launch.json` directly).
   - **`nxjs-mvz` Packaging & Storage Rules**: Always package `nxjs-mvz.nro` with `NXJS_FAT=1` (`--fat`) and `"userAccountSaveDataSize": 0` in NACP so `nx.js` never pops up the Horizon Account Selector or writes to Switch NAND instead of `sdmc:/switch/nx.rpg/saves/<id>/`.
   - **EasyRPG Custom RTP Caveat**: Upstream `EasyRPG/Player` `src/filefinder_rtp.cpp` returns early under `#ifdef PLAYER_NINTENDO` before applying `--rtp-path`; keep `0002-switch-enable-custom-rtp-path.patch` active so shared `rtp/rm2000` and `rtp/rm2003` work on Switch.
   - **`mkxp-z` Built-ins & Save Isolation**: Reuse `mkxp-z`'s built-in `pathCache: true`, `Config::execName`, and CC0 `scripts/preload/win32_wrap.rb`; isolate RGSS `Save*.rxdata/rvdata(2)` writes via `cores/mkxp-z/preload/00_nxrpg_save_redirect.rb`.

2. **Locked Upstream Commits & Patch Queues**
   - Upstream repositories belong in `third_party/` (gitignored) and are pinned in `third_party/upstreams.lock.json`.
   - Store all modifications to upstream cores as ordered patches listed in `cores/<core>/patches/SERIES`.

3. **Switch SD Card Safety & Asset Hygiene**
   - Treat `sdmc:/switch/nx.rpg/games/<folder>/` as strictly **read-only** at runtime.
   - All JSON configs, caches, manifests, and redirected saves MUST use **atomic write-temp-fsync-rename** (`<file>.tmp` → `fsync` → `rename`) to prevent Horizon OS exFAT corruption.
   - Never commit commercial games, official RTP assets, or proprietary fonts.

4. **Schema-First Contracts**
   - Any change to config, cache, lockfile, or manifest fields MUST update `common/schemas/*.schema.json`, matching C++/TS headers (`common/include/nxrpg/`, `cores/nxjs-mvz/shim/src/types.ts`), and `docs/LAUNCH_PROTOCOL.md`, and pass `python3 scripts/validate-repo.py`.

---

## 3. Repository Layout

```text
nx.rpg/
├── common/                  # CMakeLists.txt, schemas/*.schema.json, include/nxrpg/*.hpp
├── launcher/                # C++20 + Borealis frontend (CMakeLists.txt, npdm.json, include/, resources/)
├── cores/
│   ├── easyrpg/             # cmake/ flags & patches/SERIES
│   ├── nxjs-mvz/            # package.json (--fat), tsconfig.json, shim/src/**, patches/SERIES
│   └── mkxp-z/              # cross/switch-aarch64.ini, ruby-switch/, preload/*.rb, patches/SERIES
├── sdcard/switch/nx.rpg/    # Target Switch SD card layout & example configs
├── tests/                   # Host-side C++ unit test CMake & 7-engine synthetic fixture specs
├── scripts/                 # check-env.sh, bootstrap-cores.sh, apply-patches.sh, validate-repo.py
├── third_party/             # upstreams.lock.json & gitignored upstream checkouts
├── assets/                  # Switch 256x256 baseline JPEG icon, covers, NACP templates
└── docs/                    # Full architecture, protocol, input, filesystem, build & core specs
```

---

## 4. Common Commands & Validation

Run from repository root:

```bash
# Run full repository validation (JSON Schemas, example configs, lockfile, SERIES, shell syntax)
make validate
# (or directly: python3 scripts/validate-repo.py && bash -n scripts/*.sh)

# Check local devkitPro / devkitA64 / CMake / Meson / Node / pnpm toolchain
bash scripts/check-env.sh

# Checkout exact locked upstream commits from third_party/upstreams.lock.json
bash scripts/bootstrap-cores.sh

# Dry-run check patch queues against checked-out third_party/ repos
bash scripts/apply-patches.sh --check all
```
