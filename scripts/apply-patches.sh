#!/usr/bin/env bash
set -euo pipefail

# Applies (or dry-run checks) patch queues listed in cores/<core>/patches/SERIES
# against checked-out upstream trees in third_party/.
#
# Usage:
#   ./scripts/apply-patches.sh --check [all|easyrpg|nxjs-mvz|mkxp-z]
#   ./scripts/apply-patches.sh [all|easyrpg|nxjs-mvz|mkxp-z]

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOCKFILE="$ROOT_DIR/third_party/upstreams.lock.json"

CHECK_ONLY=0
TARGET_CORE="all"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --check)
      CHECK_ONLY=1
      shift
      ;;
    all|easyrpg|nxjs-mvz|mkxp-z)
      TARGET_CORE="$1"
      shift
      ;;
    *)
      echo "Usage: $0 [--check] [all|easyrpg|nxjs-mvz|mkxp-z]" >&2
      exit 1
      ;;
  esac
done

PYTHON_BIN=""
if command -v python3 >/dev/null 2>&1; then
  PYTHON_BIN="python3"
elif command -v python >/dev/null 2>&1; then
  PYTHON_BIN="python"
else
  echo "Error: python3 or python is required" >&2
  exit 1
fi

mapfile -t ENTRIES < <("$PYTHON_BIN" - "$LOCKFILE" "$TARGET_CORE" <<'PY'
import json, sys
sys.stdout.reconfigure(newline="\n")
with open(sys.argv[1], "r", encoding="utf-8") as f:
    data = json.load(f)
for key, info in data["cores"].items():
    if sys.argv[2] in ("all", key):
        print(f"{key}|{info['checkoutDir']}|{info['patchSeries']}")
PY
)

for entry in "${ENTRIES[@]}"; do
  entry="${entry%$'\r'}"
  IFS='|' read -r key checkout_rel series_rel <<< "$entry"
  checkout_dir="$ROOT_DIR/$checkout_rel"
  series_file="$ROOT_DIR/$series_rel"
  patch_dir="$(dirname "$series_file")"

  if [[ ! -f "$series_file" ]]; then
    echo "Error: missing SERIES file at $series_rel" >&2
    exit 1
  fi

  mapfile -t PATCHES < <(grep -vE '^\s*(#|$)' "$series_file" || true)
  if [[ ${#PATCHES[@]} -eq 0 ]]; then
    echo ">>> [$key] SERIES has 0 active patches (queue clean)."
    continue
  fi

  if [[ ! -d "$checkout_dir/.git" ]]; then
    echo ">>> [$key] Skipping git apply: $checkout_rel not cloned yet (run scripts/bootstrap-cores.sh first)."
    continue
  fi

  for patch_name in "${PATCHES[@]}"; do
    patch_path="$patch_dir/$patch_name"
    if [[ ! -f "$patch_path" ]]; then
      echo "Error: patch listed in $series_rel does not exist: $patch_path" >&2
      exit 1
    fi
    if [[ $CHECK_ONLY -eq 1 ]]; then
      echo ">>> [$key] Checking $patch_name..."
      git -C "$checkout_dir" apply --check "$patch_path"
    else
      echo ">>> [$key] Applying $patch_name..."
      git -C "$checkout_dir" apply "$patch_path"
    fi
  done
done
