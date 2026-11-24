#!/usr/bin/env bash
set -euo pipefail

# Cross-compiles mkxp-z + static Ruby MRI 3.1.3 for Nintendo Switch inside
# `nxrpg-devkita64:latest` and packages `sdcard/switch/nx.rpg/cores/mkxp-z.nro`.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MKXP_DIR="$ROOT_DIR/third_party/mkxp-z"
SRC_STAGE="/tmp/nxrpg-mkxp-z-src"
BUILD_DIR="/tmp/nxrpg-mkxp-z-build"
ROMFS_STAGE="/tmp/nxrpg-mkxp-z-romfs"
OUT_NRO="$ROOT_DIR/sdcard/switch/nx.rpg/cores/mkxp-z.nro"

if [[ -z "${DEVKITPRO:-}" ]]; then
  echo "Error: DEVKITPRO is not set. Run inside nxrpg-devkita64:latest." >&2
  exit 1
fi

if [[ ! -d "$MKXP_DIR/.git" ]]; then
  bash "$ROOT_DIR/scripts/bootstrap-cores.sh" mkxp-z
fi

git config --global --add safe.directory "$ROOT_DIR" >/dev/null 2>&1 || true
git config --global --add safe.directory "$MKXP_DIR" >/dev/null 2>&1 || true

# Stage into container-local tmpfs for fast Meson subproject extraction/patching
rm -rf "$SRC_STAGE" "$BUILD_DIR" "$ROMFS_STAGE"
mkdir -p "$SRC_STAGE"
tar -C "$MKXP_DIR" --exclude=.git -cf - . | tar -C "$SRC_STAGE" -xf -

# Normalize CRLF in staged copy if checked out on Windows, then apply patch queue if needed
if ! grep -q "host_machine_is_horizon" "$SRC_STAGE/subprojects/common/sonametool.py"; then
  git -C "$SRC_STAGE" init -q
  git -C "$SRC_STAGE" config core.autocrlf false
  for p in 0001-meson-horizon-host-and-nro-target.patch \
           0002-switch-nxrpg-manifest-config-loader.patch \
           0003-switch-sdl2-gamepad-exitcombo-and-chainload.patch; do
    git -C "$SRC_STAGE" apply --ignore-whitespace "$ROOT_DIR/cores/mkxp-z/patches/$p"
  done
fi

find "$SRC_STAGE/subprojects/packagefiles" -type f -exec sed -i 's/\r$//' {} +

meson setup "$BUILD_DIR" "$SRC_STAGE" \
  --cross-file "$ROOT_DIR/cores/mkxp-z/cross/switch-aarch64.ini" \
  --buildtype=release \
  -Ddefault_system=auto \
  -Dgfx_backend=gles \
  -Daarch64_neon=enabled \
  -Duse_miniffi=false \
  -Dworkdir_current=true \
  -Dcjk_fallback_font=false \
  -Dopenssl=disabled \
  -Dangle=disabled \
  -Dextra_shaders=disabled \
  -Dalsa=disabled -Dpulseaudio=disabled -Dpipewire=disabled -Djack=disabled -Doss=disabled

meson compile -C "$BUILD_DIR" -j"$(nproc)"

mkdir -p "$ROMFS_STAGE/preload" "$(dirname "$OUT_NRO")"
cp -f "$SRC_STAGE/scripts/preload/win32_wrap.rb" "$ROMFS_STAGE/preload/"
cp -f "$ROOT_DIR/cores/mkxp-z/preload/00_nxrpg_save_redirect.rb" "$ROMFS_STAGE/preload/"
cp -f "$ROOT_DIR/cores/mkxp-z/preload/01_nxrpg_win32_ext.rb" "$ROMFS_STAGE/preload/"

"$DEVKITPRO/tools/bin/nacptool" --create "mkxp-z (nx.rpg)" "nx.rpg contributors" "2.4.2" "$BUILD_DIR/mkxp-z.nacp" --titleid=016e787270670003
"$DEVKITPRO/tools/bin/elf2nro" "$BUILD_DIR/mkxp-z.elf" "$OUT_NRO" \
  --icon="$ROOT_DIR/assets/icon.jpg" \
  --nacp="$BUILD_DIR/mkxp-z.nacp" \
  --romfsdir="$ROMFS_STAGE"

echo "Installed: $OUT_NRO ($(stat -c%s "$OUT_NRO" 2>/dev/null || wc -c < "$OUT_NRO") bytes)"
