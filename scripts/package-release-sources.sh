#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="${1:-}"
if [[ ! "$VERSION" =~ ^v[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z]+([.-][0-9A-Za-z]+)*)?$ ]]; then
  printf 'Usage: %s vX.Y.Z\n' "$0" >&2
  exit 1
fi
cd "$ROOT_DIR"

STAGING="$(mktemp -d)"
trap 'rm -rf "$STAGING"' EXIT
NAME="nx.rpg-${VERSION}-gpl-sources"
SOURCE="$STAGING/$NAME"
mkdir -p "$SOURCE/project" "$SOURCE/upstreams" "$SOURCE/dependencies" dist

# Record the exact project revision with its build recipes and patch queues.
git archive HEAD | tar -xf - -C "$SOURCE/project"
git rev-parse HEAD > "$SOURCE/project-commit.txt"

# Archive committed upstream sources, excluding local patches and build outputs.
python3 -I - <<'PY' > "$STAGING/locked-cores"
import json
from pathlib import Path

lock = json.loads(Path('third_party/upstreams.lock.json').read_text(encoding='utf-8'))
for core in lock['cores'].values():
    print(core['checkoutDir'], core['commit'], sep='|')
PY
while IFS='|' read -r checkout commit; do
  destination="$SOURCE/upstreams/${checkout##*/}"
  mkdir -p "$destination"
  git -C "$checkout" archive "$commit" | tar -xf - -C "$destination"
done < "$STAGING/locked-cores"

RUBY_TARBALL=third_party/mkxp-z/subprojects/packagecache/ruby-3_1_3.tar.gz
printf '%s  %s\n' c87fc4789233ad90cb1b865e547a257d3c15b3e8e6918b153197e1ed4e47226b "$RUBY_TARBALL" | sha256sum -c -
cp "$RUBY_TARBALL" "$SOURCE/dependencies/"

tar -czf "dist/$NAME.tar.gz" -C "$STAGING" "$NAME"
tar -tzf "dist/$NAME.tar.gz" >/dev/null
sha256sum "dist/$NAME.tar.gz"
