# frozen_string_literal: false
# cores/mkxp-z/preload/00_nxrpg_save_redirect.rb
#
# Purpose (Phase 3 implementation skeleton):
#   RPG Maker XP (`Save1.rxdata`), VX (`Save1.rvdata`), and VX Ace (`Save01.rvdata2`)
#   write save files, `save.ini`, and screenshot thumbnails directly into `.` (`gameFolder`)
#   using Ruby's `File.open`, `File.exist?`, `File.delete`, and `Dir.glob`.
#
#   In nx.rpg, `games/<folder>/` is treated as immutable user media, while per-game
#   writable storage lives at `ENV['NXRPG_SAVE_DIR']` (`sdmc:/switch/nx.rpg/saves/<game_id>`).
#
# Planned behavior:
#   1. Read `NXRPG_SAVE_DIR` injected by mkxp-z's `--nxrpg-manifest` loader.
#   2. Intercept relative write/delete/save-glob operations targeting save patterns
#      (`/^Save\d+\.r[xv]data2?$/i`, `/\.sav$/i`, `/^System\.rvdata2$/i` custom global data)
#      and transparently map them into `NXRPG_SAVE_DIR` with atomic `.tmp` rename.
#   3. Fall back to `gameFolder` for read-only lookups if a pre-bundled save exists in `gameFolder`
#      and has not yet been shadowed in `NXRPG_SAVE_DIR` (Copy-on-Write semantics).
