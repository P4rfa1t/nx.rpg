# AGENTS.md

Repository guidance for AI coding agents and human contributors working on **nx.rpg** — a Nintendo Switch multi-engine RPG Maker runner supporting RPG Maker 2000, 2003, XP, VX, VX Ace, MV, and MZ.

---

## 1. Project Overview & Current Phase

- **Current Phase**: `Phase 0` (project layout, architecture specs, JSON schemas, and build scaffolding). No playable `.nro` binaries exist yet.
- **Target Platform**: Nintendo Switch (`Horizon OS` / `libnx` / `devkitA64`, `aarch64-none-elf`).
- **Three Upstream Cores** (managed under `third_party/`, ignored by git, fetched via `scripts/bootstrap-cores.sh`):
  1. **EasyRPG Player** (`EasyRPG/Player`, C++ / CMake / GPL-3.0) → RM 2000 (`rm2k`) & RM 2003 (`rm2k3`) → `cores/easyrpg.nro`
  2. **nx.js** (`TooTallNate/nx.js`, C++ + TS / V8 + Skia GPU + WebGL2 + Web Audio/FFmpeg + WASM / MIT) → RM MV (`rmmv`) & RM MZ (`rmmz`) → `cores/nxjs-mvz.nro`
  3. **mkxp-z** (`mkxp-z/mkxp-z`, C++ + Ruby MRI 3.1 / Meson + SDL2 + GLES2 + OpenAL Soft / GPL-2.0) → RM XP (`rmxp`), VX (`rmvx`), VX Ace (`rmvxa`) → `cores/mkxp-z.nro`

Read the specs before implementing features:
- `docs/ARCHITECTURE.md` — Multi-NRO chainloading, Switch memory model, SD layout
- `docs/ROADMAP.md` — Phased execution order (`Phase 0` → `Phase 5`)
- `docs/ENGINE_DETECTION.md` — 7-engine detection precedence, metadata, encoding, shared RTP
- `docs/LAUNCH_PROTOCOL.md` — Launcher ↔ Core `launch.json` contract & return flow
- `docs/cores/{easyrpg,nxjs-mvz,mkxp-z}-plan.md` — Per-core porting and integration plans

---

## 2. Non-Negotiable Architectural Invariants

1. **Strict Multi-NRO Isolation (`envSetNextLoad`)**
   - **Never** statically or dynamically link `EasyRPG/Player` (GPL-3.0) and `mkxp-z` (GPL-2.0) into one binary, nor merge the three runtimes into the Launcher binary.
   - Launcher (`nx.rpg.nro`) and each core (`cores/*.nro`) MUST remain independent `.nro` executables communicating solely via `sdmc:/switch/nx.rpg/runtime/launch.json` and `--nxrpg-manifest <path>`.
   - On exit, every core must flush saves/logs, tear down graphics/audio/fs handles, and chainload `returnTo` (`nx.rpg.nro --from-core`) via `envSetNextLoad()`.

2. **Keep Upstream Sources Out of Git & Use Patch Queues**
   - Upstream repositories belong in `third_party/` (gitignored). Do **not** commit upstream source trees into this repository.
   - Store all core-specific modifications as minimal patches under `cores/<core>/patches/` and integration/shim code under `cores/<core>/`.
   - Preserve each core's native upstream build system (CMake for EasyRPG, Makefile + pnpm for nx.js, Meson for mkxp-z).

3. **Respect Switch Platform Constraints**
   - **Applet Mode vs Title Takeover**: Applet Mode only exposes ~440 MB RAM. Launcher must check `appletGetAppletType()` and warn before launching memory-heavy cores (`nxjs-mvz`, `mkxp-z`).
   - **Filesystem & Case-Insensitivity**: Switch SD cards (FAT32/exFAT) and RPG Maker games authored on Windows frequently have filename casing mismatches and non-UTF-8 legacy paths/metadata (`Shift_JIS`, `GBK`, `Big5`, `CP949`). Always handle case-insensitive asset lookups and configurable encodings.
   - **No Copyrighted Game Assets**: Never commit commercial RPG Maker games, RTP packs, or proprietary fonts. Keep `sdcard/switch/nx.rpg/{games,saves,rtp,fonts,cores}` empty except `.gitkeep` and example configs.

4. **Schema-First Contracts**
   - Any change to Launcher ↔ Core handoff fields or per-game settings MUST update both the JSON Schema in `common/schemas/` and `docs/LAUNCH_PROTOCOL.md` together.
   - Valid `engine` enum values: `rm2k`, `rm2k3`, `rmxp`, `rmvx`, `rmvxa`, `rmmv`, `rmmz`.
   - Valid `core` enum values: `easyrpg`, `nxjs-mvz`, `mkxp-z`.

---

## 3. Repository Layout

```text
nx.rpg/
├── common/schemas/          # Shared JSON Schemas (launch-manifest, game-config)
├── launcher/                # Unified Switch frontend (nx.rpg.nro; recommended: C++ + Borealis)
├── cores/
│   ├── easyrpg/             # RM2000/2003 patches & build wiring
│   ├── nxjs-mvz/            # RM MV/MZ browser/DOM/NW.js shim & nx.js patches
│   └── mkxp-z/              # RM XP/VX/VXAce devkitA64 Meson cross-file & patches
├── sdcard/switch/nx.rpg/    # Target Switch SD card layout template
├── scripts/                 # Toolchain check & upstream bootstrap scripts
├── third_party/             # Local-only checkouts of upstream cores (gitignored)
├── assets/                  # Switch icon.jpg, default engine covers, NACP templates
└── docs/                    # Architecture, roadmap, protocols, per-core plans
```

---

## 4. Common Commands & Validation

Run from repository root:

```bash
# Show available top-level targets
make help

# Check local devkitPro / devkitA64 / CMake / Meson / Node / pnpm toolchain
bash scripts/check-env.sh

# Fetch or update the three upstream repos into third_party/
bash scripts/bootstrap-cores.sh

# Validate shell scripts (matches CI)
bash -n scripts/check-env.sh && bash -n scripts/bootstrap-cores.sh

# Validate all JSON files and schemas (matches CI)
python3 -c 'import glob, json; [json.load(open(p, encoding="utf-8")) for p in glob.glob("common/schemas/*.json") + glob.glob("sdcard/**/*.json", recursive=True)]'
```

---

## 5. Development Order & Conventions

- **Follow the phased roadmap**: prioritize `Phase 1` (common detector/manifest + Launcher MVP + `EasyRPG/Player` round-trip) → `Phase 2` (`nx.js` + MV/MZ shim) → `Phase 3` (`mkxp-z` + static Ruby MRI 3.1 on `devkitA64`) → `Phase 4` (UI/UX & soft keyboard polish).
- **Formatting**: follow `.editorconfig` and `.gitattributes` (UTF-8, LF line endings everywhere — especially `.sh` and `Makefile`, 2-space indentation for C/C++/TS/JSON/YAML/Shell, tabs for `Makefile`).
- **License Hygiene**: `nx.rpg`'s own code is MIT. Whenever adding patches against GPL cores (`easyrpg`, `mkxp-z`), document the upstream commit SHA and keep patches cleanly reviewable in `cores/<core>/patches/` per `THIRD_PARTY_LICENSES.md`.
- **Verification**: before finishing any change, validate modified schemas/configs/scripts (and target builds once implemented) and report any skipped checks explicitly.
