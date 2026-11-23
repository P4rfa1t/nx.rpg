#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
THIRD_PARTY_DIR="$ROOT_DIR/third_party"

mkdir -p "$THIRD_PARTY_DIR"

clone_or_update() {
  local name="$1"
  local url="$2"
  local branch="$3"
  local target="$THIRD_PARTY_DIR/$name"

  if [[ -d "$target/.git" ]]; then
    echo ">>> Updating $name ($branch)..."
    git -C "$target" fetch --depth 1 origin "$branch"
    git -C "$target" checkout "$branch"
    git -C "$target" pull --ff-only origin "$branch"
  else
    echo ">>> Cloning $name ($url @ $branch)..."
    git clone --depth 1 --branch "$branch" "$url" "$target"
  fi
}

clone_or_update "EasyRPG-Player" "https://github.com/EasyRPG/Player.git" "master"
clone_or_update "nx.js"          "https://github.com/TooTallNate/nx.js.git" "main"
clone_or_update "mkxp-z"         "https://github.com/mkxp-z/mkxp-z.git" "dev"

echo ""
echo "Done. Upstream repositories are ready under: $THIRD_PARTY_DIR"
