#!/usr/bin/env bash
set -euo pipefail

# Fetches upstream core repositories into third_party/ at the exact commit SHAs
# pinned in third_party/upstreams.lock.json (or branch HEAD if --latest is passed).
#
# Usage:
#   ./scripts/bootstrap-cores.sh                 # Checkout pinned commits for all cores
#   ./scripts/bootstrap-cores.sh easyrpg         # Checkout pinned commit for one core
#   ./scripts/bootstrap-cores.sh --latest all    # Fetch latest branch tip (for lockfile bumps)

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOCKFILE="$ROOT_DIR/third_party/upstreams.lock.json"

MODE="locked"
TARGET_CORE="all"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --latest)
      MODE="latest"
      shift
      ;;
    all|easyrpg|nxjs-mvz|mkxp-z)
      TARGET_CORE="$1"
      shift
      ;;
    *)
      echo "Usage: $0 [--latest] [all|easyrpg|nxjs-mvz|mkxp-z]" >&2
      exit 1
      ;;
  esac
done

if [[ ! -f "$LOCKFILE" ]]; then
  echo "Error: lockfile not found at $LOCKFILE" >&2
  exit 1
fi

PYTHON_BIN=""
if command -v python3 >/dev/null 2>&1; then
  PYTHON_BIN="python3"
elif command -v python >/dev/null 2>&1; then
  PYTHON_BIN="python"
else
  echo "Error: python3 or python is required to parse $LOCKFILE" >&2
  exit 1
fi

mapfile -t ENTRIES < <("$PYTHON_BIN" - "$LOCKFILE" "$TARGET_CORE" <<'PY'
import json, sys
sys.stdout.reconfigure(newline="\n")
lock_path, target = sys.argv[1], sys.argv[2]
with open(lock_path, "r", encoding="utf-8") as f:
    data = json.load(f)
for key, info in data["cores"].items():
    if target in ("all", key):
        print(f"{key}|{info['repoUrl']}|{info['branch']}|{info['commit']}|{info['checkoutDir']}")
PY
)

for entry in "${ENTRIES[@]}"; do
  entry="${entry%$'\r'}"
  IFS='|' read -r key repo_url branch commit checkout_rel <<< "$entry"
  target_dir="$ROOT_DIR/$checkout_rel"
  mkdir -p "$(dirname "$target_dir")"

  if [[ ! -d "$target_dir/.git" ]]; then
    echo ">>> [$key] Initializing git checkout at $checkout_rel..."
    git init "$target_dir"
    git -C "$target_dir" remote add origin "$repo_url"
  fi

  if [[ "$MODE" == "latest" ]]; then
    echo ">>> [$key] Fetching latest branch '$branch' from $repo_url..."
    git -C "$target_dir" fetch --depth 1 origin "$branch"
    git -C "$target_dir" checkout -B "$branch" FETCH_HEAD
  else
    echo ">>> [$key] Fetching pinned commit $commit ($branch)..."
    git -C "$target_dir" fetch --depth 1 origin "$commit"
    git -C "$target_dir" checkout --detach FETCH_HEAD
  fi

  actual_sha="$(git -C "$target_dir" rev-parse HEAD)"
  echo "    Checked out: $actual_sha"
done

echo ""
echo "Done. Upstream checkouts are ready."
