"""One-time importer for baseline register programs (schema/programs/*.yaml).

A "program" is a register image the TV5725 needs before any computed adjustment: the values of
the configuration registers for one input standard and one output raster. They are factual chip
configuration data, originally dumped from the board's factory firmware and refined by the
gbs-control community (https://github.com/ramapcsx2/gbs-control, GPL-3.0); see NOTICE.md.

The source arrays annotate every value with its register (`0x7C, // s0_40`), which is what this
importer relies on: it never assumes an order, it reads the address beside each value.

Usage:
    uv run tools/codegen/import_programs.py --src <dir with *.h arrays> --commit <git sha> \
        [--out schema/programs]
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

# Some annotations drop the leading zero (`// s3_0` is s3_00), so the offset is 1-2 hex digits.
ENTRY_RE = re.compile(r"(0x[0-9A-Fa-f]+|\d+)\s*,\s*//\s*s([0-5])_([0-9A-Fa-f]{1,2})\b")

# Source file stem -> (program id, input standard, output description).
PROGRAMS: dict[str, tuple[str, str, str]] = {
    "ntsc_240p": ("ntsc-240p", "ntsc", "720x240 progressive, 15 kHz"),
    "ntsc_720x480": ("ntsc-480p", "ntsc", "720x480 progressive, 31 kHz"),
    "ntsc_1280x720": ("ntsc-720p", "ntsc", "1280x720"),
    "ntsc_1280x1024": ("ntsc-1024p", "ntsc", "1280x1024"),
    "ntsc_1920x1080": ("ntsc-1080p", "ntsc", "1920x1080"),
    "ntsc_downscale": ("ntsc-downscale", "ntsc", "downscale to 15 kHz"),
    "pal_240p": ("pal-288p", "pal", "768x288 progressive, 15 kHz"),
    "pal_768x576": ("pal-576p", "pal", "768x576 progressive, 31 kHz"),
    "pal_1280x720": ("pal-720p", "pal", "1280x720"),
    "pal_1280x1024": ("pal-1024p", "pal", "1280x1024"),
    "pal_1920x1080": ("pal-1080p", "pal", "1920x1080"),
    "pal_downscale": ("pal-downscale", "pal", "downscale to 15 kHz"),
    "ofw_RGBS": ("factory-rgbs", "any", "factory firmware dump, RGBS input"),
    "ofw_ypbpr": ("factory-ypbpr", "any", "factory firmware dump, YPbPr input"),
    # Constant sections the programs do not cover: written on their own, independent of the
    # output raster.
    "presetMdSection": ("section-mode-detect", "any", "mode detector windows, S1_60..S1_83"),
    "presetDeinterlacerSection": ("section-deinterlacer", "any", "deinterlacer, S2_00..S2_3F"),
    "presetHdBypassSection": ("section-hd-bypass", "any", "HD bypass path, S1_30..S1_5F"),
}


def parse(path: Path) -> dict[tuple[int, int], int]:
    image: dict[tuple[int, int], int] = {}
    for m in ENTRY_RE.finditer(path.read_text(encoding="utf-8")):
        value = int(m[1], 0)
        key = (int(m[2]), int(m[3], 16))
        if not 0 <= value <= 0xFF:
            sys.exit(f"{path.name}: S{key[0]}_{key[1]:02X} value {value} out of range")
        if key in image and image[key] != value:
            sys.exit(f"{path.name}: S{key[0]}_{key[1]:02X} assigned twice with different values")
        image[key] = value
    if not image:
        sys.exit(f"{path.name}: no annotated entries found")
    return image


def chunks(image: dict[tuple[int, int], int]) -> list[tuple[int, int, list[int]]]:
    """Group the image into runs of consecutive addresses, split at 16-byte boundaries."""
    runs: list[tuple[int, int, list[int]]] = []
    for seg, addr in sorted(image):
        last = runs[-1] if runs else None
        if last and last[0] == seg and last[1] + len(last[2]) == addr and addr % 16 != 0:
            last[2].append(image[(seg, addr)])
        else:
            runs.append((seg, addr, [image[(seg, addr)]]))
    return runs


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--src", type=Path, required=True)
    ap.add_argument("--commit", required=True, help="git commit of the source tree, for provenance")
    ap.add_argument("--out", type=Path, default=Path("schema/programs"))
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)

    for stem, (pid, standard, output) in PROGRAMS.items():
        src = args.src / f"{stem}.h"
        if not src.exists():
            sys.exit(f"missing {src}")
        image = parse(src)
        segs = sorted({s for s, _ in image})
        lines = [
            "# Baseline register program for the TV5725. Factual chip configuration data;",
            "# see NOTICE.md for provenance. Imported by tools/codegen/import_programs.py.",
            f"id: {pid}",
            f"standard: {standard}",
            f'output: "{output}"',
            "source:",
            f"  file: {stem}.h",
            f"  commit: {args.commit}",
            f"bytes: {len(image)}",
            f"segments: [{', '.join(str(s) for s in segs)}]",
            "# Runs of consecutive registers, at most 16 bytes, never crossing a 16-byte boundary.",
            "runs:",
        ]
        for seg, start, values in chunks(image):
            hexes = " ".join(f"{v:02X}" for v in values)
            lines.append(f'  S{seg}_{start:02X}: "{hexes}"')
        dest = args.out / f"{pid}.yaml"
        dest.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
        print(f"{pid:16s} {len(image):4d} bytes, segments {segs} -> {dest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
