#!/usr/bin/env bash
set -euo pipefail

# Builds the patched TooTallNate/nx.js fat base NRO (with Switch.setNextLoad)
# when run inside `ghcr.io/tootallnate/pacman-packages` (or any env with switch-v8/skia),
# bundles `cores/nxjs-mvz/shim/src/index.ts` -> `romfs/main.js`, and packages
# `sdcard/switch/nx.rpg/cores/nxjs-mvz.nro` using `--fat` and `userAccountSaveDataSize=0`.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NXJS_DIR="$ROOT_DIR/third_party/nx.js"
MVZ_DIR="$ROOT_DIR/cores/nxjs-mvz"
OUT_NRO="$ROOT_DIR/sdcard/switch/nx.rpg/cores/nxjs-mvz.nro"

if [[ ! -d "$NXJS_DIR/.git" ]]; then
  bash "$ROOT_DIR/scripts/bootstrap-cores.sh" nxjs-mvz
fi

git config --global --add safe.directory "$ROOT_DIR" >/dev/null 2>&1 || true
git config --global --add safe.directory "$NXJS_DIR" >/dev/null 2>&1 || true

# Apply 0001-applet-bind-env-set-next-load.patch if not already applied
if git -C "$NXJS_DIR" apply --check "$MVZ_DIR/patches/0001-applet-bind-env-set-next-load.patch" 2>/dev/null; then
  bash "$ROOT_DIR/scripts/apply-patches.sh" nxjs-mvz
fi

# Step 1: Build patched nx.js runtime + nxjs.nro if switch-v8 is present
if [[ -n "${DEVKITPRO:-}" ]] && [[ -f "$DEVKITPRO/portlibs/switch/lib/libv8_monolith.a" ]]; then
  echo ">>> Building patched nxjs.nro from third_party/nx.js..."
  (
    cd "$NXJS_DIR"
    pnpm install --frozen-lockfile || pnpm install --no-frozen-lockfile
    pnpm build --filter @nx.js/runtime
    pnpm bundle
    make -j"$(nproc)"
  )
fi

# Step 2: Typecheck, test, and bundle MV/MZ shim
(
  cd "$MVZ_DIR"
  pnpm install --no-frozen-lockfile
  pnpm typecheck
  pnpm test
  pnpm build:romfs

  # If a freshly patched nxjs.nro was built, splice it into @nx.js/nro's dist/nxjs.nro
  if [[ -f "$NXJS_DIR/nxjs.nro" ]]; then
    for dist_nro in "$MVZ_DIR"/node_modules/.pnpm/@nx.js+nro@*/node_modules/@nx.js/nro/dist/nxjs.nro "$MVZ_DIR/node_modules/@nx.js/nro/dist/nxjs.nro"; do
      if [[ -f "$dist_nro" ]]; then
        cp -f "$NXJS_DIR/nxjs.nro" "$dist_nro"
        echo ">>> Using patched nxjs.nro at $dist_nro"
      fi
    done
  fi

  cp -f "$ROOT_DIR/assets/icon.jpg" "$MVZ_DIR/icon.jpg"
  NXJS_FAT=1 pnpm package:nro
  rm -f "$MVZ_DIR/icon.jpg"
)

mkdir -p "$(dirname "$OUT_NRO")"
mv -f "$MVZ_DIR/nxjs-mvz.nro" "$OUT_NRO"
echo "Installed: $OUT_NRO ($(stat -c%s "$OUT_NRO" 2>/dev/null || wc -c < "$OUT_NRO") bytes)"
