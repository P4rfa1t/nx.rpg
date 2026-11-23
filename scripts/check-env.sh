#!/usr/bin/env bash
set -euo pipefail

echo "=== nx.rpg Toolchain Environment Check ==="

check_cmd() {
  local name="$1"
  local usage="$2"
  if command -v "$name" >/dev/null 2>&1; then
    printf "  [OK]      %-24s (%s)\n" "$name" "$usage"
  else
    printf "  [MISSING] %-24s (%s)\n" "$name" "$usage"
  fi
}

if [[ -n "${DEVKITPRO:-}" ]]; then
  printf "  [OK]      DEVKITPRO=%s\n" "$DEVKITPRO"
else
  printf "  [MISSING] DEVKITPRO env var is not set\n"
fi

check_cmd "aarch64-none-elf-gcc" "devkitA64 C/C++ compiler (All cores & Launcher)"
check_cmd "elf2nro"              "Switch NRO packager (devkitPro switch-tools)"
check_cmd "cmake"                "EasyRPG Player & Launcher build"
check_cmd "ninja"                "Fast CMake/Meson backend"
check_cmd "meson"                "mkxp-z cross-compilation"
check_cmd "node"                 "nx.js & MV/MZ shim build"
check_cmd "pnpm"                 "nx.js workspace package manager"
check_cmd "git"                  "Upstream core management"
