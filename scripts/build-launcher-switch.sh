#!/usr/bin/env bash
set -euo pipefail

# Cross-compiles `nx.rpg.nro` (C++20 + Borealis frontend) for Nintendo Switch
# and installs it into `sdcard/switch/nx.rpg/nx.rpg.nro`.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/switch-launcher"
OUT_NRO="$ROOT_DIR/sdcard/switch/nx.rpg/nx.rpg.nro"
BOREALIS_DIR="${NXRPG_BOREALIS_DIR:-/opt/nxrpg/borealis}"

if [[ -z "${DEVKITPRO:-}" ]]; then
  echo "Error: DEVKITPRO is not set. Run inside nxrpg-devkita64 / devkitpro/devkita64." >&2
  exit 1
fi

if [[ ! -f "$BOREALIS_DIR/library/CMakeLists.txt" ]]; then
  if [[ -f "$ROOT_DIR/third_party/borealis/library/CMakeLists.txt" ]]; then
    BOREALIS_DIR="$ROOT_DIR/third_party/borealis"
  else
    echo "Error: Borealis checkout not found at $BOREALIS_DIR or third_party/borealis." >&2
    exit 1
  fi
fi

cmake -S "$ROOT_DIR/launcher" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DPLATFORM_SWITCH=ON \
  -DUSE_DEKO3D=ON \
  -DNXRPG_BOREALIS_DIR="$BOREALIS_DIR"

cmake --build "$BUILD_DIR" --target nx.rpg.nro --parallel

if [[ ! -f "$BUILD_DIR/nx.rpg.nro" ]]; then
  echo "Error: nx.rpg.nro not found in $BUILD_DIR" >&2
  exit 1
fi

mkdir -p "$(dirname "$OUT_NRO")"
cp -f "$BUILD_DIR/nx.rpg.nro" "$OUT_NRO"
echo "Installed: $OUT_NRO ($(stat -c%s "$OUT_NRO" 2>/dev/null || wc -c < "$OUT_NRO") bytes)"
