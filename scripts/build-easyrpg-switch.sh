#!/usr/bin/env bash
set -euo pipefail

# Cross-compiles EasyRPG/Player for Nintendo Switch inside a devkitA64 environment
# (or Docker `devkitpro/devkita64`) and installs `cores/easyrpg.nro` into the SD layout.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EASYRPG_DIR="$ROOT_DIR/third_party/EasyRPG-Player"
BUILD_DIR="$ROOT_DIR/build/switch-easyrpg"
OUT_NRO="$ROOT_DIR/sdcard/switch/nx.rpg/cores/easyrpg.nro"

if [[ -z "${DEVKITPRO:-}" ]]; then
  echo "Error: DEVKITPRO is not set. Run inside devkitpro/devkita64 or source devkitPro env." >&2
  exit 1
fi

if [[ ! -d "$EASYRPG_DIR/.git" ]]; then
  bash "$ROOT_DIR/scripts/bootstrap-cores.sh" easyrpg
fi

git config --global --add safe.directory "$ROOT_DIR" >/dev/null 2>&1 || true
git config --global --add safe.directory "$EASYRPG_DIR" >/dev/null 2>&1 || true

# Re-apply the full SERIES patch queue on a clean upstream tree so updated
# patches always take effect (build-from-clean; upstream HEAD is the base).
echo ">>> Resetting EasyRPG tree to upstream HEAD for clean patch application..."
git -C "$EASYRPG_DIR" reset --hard -q HEAD
bash "$ROOT_DIR/scripts/apply-patches.sh" easyrpg

cmake -S "$EASYRPG_DIR" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/Switch.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -C "$ROOT_DIR/cores/easyrpg/cmake/nxrpg-easyrpg-flags.cmake"

cmake --build "$BUILD_DIR" --parallel

NRO_SRC=""
for candidate in "$BUILD_DIR/easyrpg-player.nro" "$BUILD_DIR/EasyRPG-Player.nro"; do
  if [[ -f "$candidate" ]]; then
    NRO_SRC="$candidate"
    break
  fi
done

if [[ -z "$NRO_SRC" ]]; then
  echo "Error: built EasyRPG NRO not found in $BUILD_DIR" >&2
  exit 1
fi

mkdir -p "$(dirname "$OUT_NRO")"
cp -f "$NRO_SRC" "$OUT_NRO"
echo "Installed: $OUT_NRO ($(stat -c%s "$OUT_NRO" 2>/dev/null || wc -c < "$OUT_NRO") bytes)"
