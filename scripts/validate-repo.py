#!/usr/bin/env python3
"""
Zero-dependency repository layout, JSON Schema subset, lockfile, and patch-series validator.
Runs identically on Windows, macOS, Linux, and GitHub Actions CI.
"""

from __future__ import annotations

import glob
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def fail(msg: str) -> None:
    print(f"[FAIL] {msg}", file=sys.stderr)
    sys.exit(1)


def load_json(path: Path) -> object:
    try:
        with path.open("r", encoding="utf-8") as f:
            return json.load(f)
    except Exception as exc:
        fail(f"Invalid JSON in {path.relative_to(ROOT)}: {exc}")


def validate_against_schema(instance: object, schema: dict, root_schema: dict, path: str) -> None:
    if "$ref" in schema:
        ref = schema["$ref"]
        assert ref.startswith("#/$defs/"), f"Unsupported $ref: {ref}"
        def_name = ref.split("/")[-1]
        schema = root_schema["$defs"][def_name]

    expected_type = schema.get("type")
    if expected_type == "object":
        if not isinstance(instance, dict):
            fail(f"{path}: expected object, got {type(instance).__name__}")
        for req in schema.get("required", []):
            if req not in instance:
                fail(f"{path}: missing required key '{req}'")
        props = schema.get("properties", {})
        if schema.get("additionalProperties") is False:
            unknown = set(instance.keys()) - set(props.keys())
            if unknown:
                fail(f"{path}: unknown properties {sorted(unknown)}")
        for k, v in instance.items():
            if k in props:
                validate_against_schema(v, props[k], root_schema, f"{path}.{k}")
    elif expected_type == "array":
        if not isinstance(instance, list):
            fail(f"{path}: expected array, got {type(instance).__name__}")
        min_items = schema.get("minItems")
        if min_items is not None and len(instance) < min_items:
            fail(f"{path}: array length {len(instance)} < minItems {min_items}")
        if "items" in schema:
            for idx, item in enumerate(instance):
                validate_against_schema(item, schema["items"], root_schema, f"{path}[{idx}]")
    elif expected_type == "string":
        if not isinstance(instance, str):
            fail(f"{path}: expected string, got {type(instance).__name__}")
        if "minLength" in schema and len(instance) < schema["minLength"]:
            fail(f"{path}: string shorter than minLength {schema['minLength']}")
        if "pattern" in schema and not re.match(schema["pattern"], instance):
            fail(f"{path}: '{instance}' does not match pattern {schema['pattern']}")
    elif expected_type == "integer":
        if isinstance(instance, bool) or not isinstance(instance, int):
            fail(f"{path}: expected integer, got {type(instance).__name__}")
        if "minimum" in schema and instance < schema["minimum"]:
            fail(f"{path}: {instance} < minimum {schema['minimum']}")
        if "maximum" in schema and instance > schema["maximum"]:
            fail(f"{path}: {instance} > maximum {schema['maximum']}")
    elif expected_type == "boolean":
        if not isinstance(instance, bool):
            fail(f"{path}: expected boolean, got {type(instance).__name__}")

    if "const" in schema and instance != schema["const"]:
        fail(f"{path}: expected const {schema['const']!r}, got {instance!r}")
    if "enum" in schema and instance not in schema["enum"]:
        fail(f"{path}: {instance!r} not in enum {schema['enum']}")


def main() -> None:
    print("=== nx.rpg Repository Contract Validation ===")

    # 1. Parse all JSON files across the repo (excluding node_modules / third_party checkouts)
    json_patterns = [
        "common/schemas/*.json",
        "third_party/upstreams.lock.json",
        "launcher/npdm.json",
        "launcher/resources/i18n/*.json",
        "cores/nxjs-mvz/package.json",
        "cores/nxjs-mvz/tsconfig.json",
        "sdcard/**/*.json",
    ]
    all_json_files: list[Path] = []
    for pat in json_patterns:
        all_json_files.extend(sorted(ROOT.glob(pat)))
    if not all_json_files:
        fail("No JSON files matched!")

    for jf in all_json_files:
        load_json(jf)
    print(f"  [OK] Parsed {len(all_json_files)} JSON files cleanly")

    # 2. Validate concrete config & lockfile instances against their formal JSON Schemas
    pairs = [
        ("sdcard/switch/nx.rpg/config/global.example.json", "common/schemas/global-config.schema.json"),
        ("sdcard/switch/nx.rpg/config/games/example-game.json", "common/schemas/game-config.schema.json"),
        ("third_party/upstreams.lock.json", "common/schemas/upstreams-lock.schema.json"),
    ]
    for inst_rel, schema_rel in pairs:
        inst = load_json(ROOT / inst_rel)
        schema = load_json(ROOT / schema_rel)
        assert isinstance(schema, dict)
        validate_against_schema(inst, schema, schema, inst_rel)
        print(f"  [OK] Schema validated: {inst_rel} -> {schema_rel}")

    # 3. Validate nxjs-mvz NACP / packaging invariants
    nxjs_pkg = load_json(ROOT / "cores/nxjs-mvz/package.json")
    assert isinstance(nxjs_pkg, dict)
    nacp = nxjs_pkg.get("nacp", {})
    if nacp.get("userAccountSaveDataSize") != 0:
        fail("cores/nxjs-mvz/package.json must keep nacp.userAccountSaveDataSize == 0")
    if "--fat" not in nxjs_pkg.get("scripts", {}).get("package:nro", ""):
        fail("cores/nxjs-mvz/package.json package:nro script must pass --fat")
    print("  [OK] Verified cores/nxjs-mvz --fat and userAccountSaveDataSize == 0 invariants")

    # 4. Validate upstream lockfile patchSeries references
    lock = load_json(ROOT / "third_party/upstreams.lock.json")
    assert isinstance(lock, dict)
    for core_key, info in lock["cores"].items():
        series_path = ROOT / info["patchSeries"]
        if not series_path.is_file():
            fail(f"Missing patchSeries file for {core_key}: {info['patchSeries']}")
        for line in series_path.read_text(encoding="utf-8").splitlines():
            stripped = line.strip()
            if stripped and not stripped.startswith("#"):
                patch_file = series_path.parent / stripped
                if not patch_file.is_file():
                    fail(f"Active patch listed in {info['patchSeries']} missing: {stripped}")
    print("  [OK] Verified third_party/upstreams.lock.json and SERIES patch queues")

    # 5. Verify required architecture docs and C++20 / TS skeleton headers exist
    required_paths = [
        "docs/ARCHITECTURE.md",
        "docs/ROADMAP.md",
        "docs/ENGINE_DETECTION.md",
        "docs/LAUNCH_PROTOCOL.md",
        "docs/INPUT_AND_UI_SPEC.md",
        "docs/FILESYSTEM_AND_SAVES.md",
        "docs/BUILD_AND_CI.md",
        "common/CMakeLists.txt",
        "common/include/nxrpg/types.hpp",
        "common/include/nxrpg/engine_detector.hpp",
        "common/include/nxrpg/launch_manifest.hpp",
        "common/include/nxrpg/chainload.hpp",
        "launcher/CMakeLists.txt",
        "launcher/include/launcher/app.hpp",
        "cores/nxjs-mvz/shim/src/index.ts",
        "cores/mkxp-z/cross/switch-aarch64.ini",
        "cores/mkxp-z/preload/00_nxrpg_save_redirect.rb",
        "tests/CMakeLists.txt",
    ]
    for rel in required_paths:
        if not (ROOT / rel).is_file():
            fail(f"Missing required project file: {rel}")
    print(f"  [OK] Verified {len(required_paths)} core docs and module skeleton files")
    print("All repository contract checks passed.")


if __name__ == "__main__":
    main()
