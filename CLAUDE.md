# CLAUDE.md

@AGENTS.md

---

## Claude Code Working Notes

- **Language**: Primary documentation and user communication in this repository are in **Simplified Chinese** (with standard English technical identifiers, paths, and code comments). Match the language of existing docs when editing `docs/` or `README.md`.
- **Single Source of Truth**: Keep shared architecture rules, directory layout, and commands in `AGENTS.md`. Only place Claude-specific workflow preferences here so the two files never drift.
- **Scope Discipline**:
  - Do not clone or check in upstream core repositories (`EasyRPG/Player`, `TooTallNate/nx.js`, `mkxp-z/mkxp-z`) unless the user explicitly asks to bootstrap/build them — `third_party/` is intentionally gitignored.
  - When updating `common/schemas/*.json`, always verify JSON syntax and keep `docs/LAUNCH_PROTOCOL.md` and `sdcard/switch/nx.rpg/config/global.example.json` in sync.
  - On Windows hosts, ensure newly created `.sh` scripts keep LF line endings (`.gitattributes` enforces `eol=lf`) and executable mode (`git update-index --chmod=+x`).
- **Switch / Upstream Fact Checking**: Before writing code against upstream internals of `EasyRPG/Player`, `TooTallNate/nx.js` (note: uses **V8 + Skia GPU + WebGL2 + FFmpeg**, not QuickJS), or `mkxp-z/mkxp-z` (uses **Meson + Ruby MRI 3.1 + SDL2 + GLES2**), inspect the actual checked-out files in `third_party/` or query the upstream repo rather than relying on outdated assumptions.
- **Git & GitHub**: Remote is `origin` (`P4rfa1t/nx.rpg`, default branch `main`). Confirm before destructive git operations or changing repository visibility.
