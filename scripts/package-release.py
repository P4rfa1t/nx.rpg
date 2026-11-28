#!/usr/bin/env python3
"""Package the Switch launcher and its three cores in one installable ZIP."""

import argparse
from pathlib import Path
import re
import struct
from zipfile import ZIP_DEFLATED, ZipFile

ROOT = Path(__file__).resolve().parents[1]
INSTALL_ROOT = Path("switch/nx.rpg")
BINARIES = (
    Path("nx.rpg.nro"),
    Path("cores/easyrpg.nro"),
    Path("cores/mkxp-z.nro"),
    Path("cores/nxjs-mvz.nro"),
)
NOTICES = (
    "LICENSE",
    "THIRD_PARTY_LICENSES.md",
    "cores/nxjs-mvz/LICENSE.nxjs",
    "launcher/src/transfer/LICENSE.rpynx",
)


def verify_nro(path: Path) -> None:
    with path.open("rb") as stream:
        header = stream.read(28)
    if len(header) != 28 or header[16:20] != b"NRO0":
        raise ValueError(f"Invalid NRO header in {path}")
    executable_size, = struct.unpack_from("<I", header, 24)
    if not 0x80 <= executable_size <= path.stat().st_size:
        raise ValueError(f"Invalid executable size in {path}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("version", help="Release version including v, e.g. v0.1.0")
    parser.add_argument("--install-root", type=Path, default=ROOT / "sdcard" / INSTALL_ROOT)
    parser.add_argument("--output-dir", type=Path, default=ROOT / "dist")
    args = parser.parse_args()
    if not re.fullmatch(r"v\d+\.\d+\.\d+(?:-[0-9A-Za-z]+(?:[.-][0-9A-Za-z]+)*)?", args.version):
        parser.error("version must be a version tag such as v0.1.0")

    for binary in BINARIES:
        verify_nro(args.install_root / binary)

    args.output_dir.mkdir(parents=True, exist_ok=True)
    output = args.output_dir / f"nx.rpg-{args.version}.zip"
    temporary = output.with_suffix(".zip.tmp")
    try:
        with ZipFile(temporary, "w", ZIP_DEFLATED, compresslevel=6) as archive:
            for binary in BINARIES:
                archive.write(args.install_root / binary, (INSTALL_ROOT / binary).as_posix())
            for notice in NOTICES:
                source = ROOT / notice
                archive.write(source, (INSTALL_ROOT / "cores/licenses" / source.name).as_posix())
        with ZipFile(temporary) as archive:
            bad = archive.testzip()
            if bad is not None:
                raise ValueError(f"ZIP integrity check failed for {bad}")
        temporary.replace(output)
    finally:
        temporary.unlink(missing_ok=True)
    print(output)


if __name__ == "__main__":
    main()
