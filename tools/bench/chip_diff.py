"""Compare two TV5725 register images field by field.

    uv run tools/bench/chip_diff.py A.txt B.txt [--all]

Each input is a text dump in either format:
- annotated, one register per line:  0x7C, // s0_40   (the program arrays, and the serial `d`
  command of upstream-style firmware: how the board's original firmware is read)
- segment rows, 16 bytes per line:   S0_40: 7C 85 00 …  (GBS Reimagined's console `dump <seg>`)

Only registers present in both are compared. Read-only status registers (S0_00..S0_2F) are left
out unless --all is given. Differences are reported per field, named from
schema/registers/tv5725.yaml, so "VDS_Y_GAIN 0x80 -> 0x90" instead of "S3_35 differs".
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[2]
ANNOTATED = re.compile(r"(0x[0-9A-Fa-f]+|\d+)\s*,\s*//\s*s([0-5])_([0-9A-Fa-f]{1,2})\b")
ROW = re.compile(r"S([0-5])_([0-9A-Fa-f]{2}):((?:\s+[0-9A-Fa-f]{2}){1,16})")


def load_image(path: Path) -> dict[tuple[int, int], int]:
    text = path.read_text(encoding="utf-8", errors="replace")
    image: dict[tuple[int, int], int] = {}
    for m in ANNOTATED.finditer(text):
        image[(int(m[2]), int(m[3], 16))] = int(m[1], 0) & 0xFF
    for m in ROW.finditer(text):
        seg, start = int(m[1]), int(m[2], 16)
        for i, hexval in enumerate(m[3].split()):
            image[(seg, start + i)] = int(hexval, 16)
    return image


def field_value(image: dict[tuple[int, int], int], f: dict) -> int | None:
    nbytes = (f["lsb"] + f["width"] + 7) // 8
    word = 0
    for i in range(nbytes):
        key = (f["seg"], f["addr"] + i)
        if key not in image:
            return None
        word |= image[key] << (8 * i)
    return (word >> f["lsb"]) & ((1 << f["width"]) - 1)


def main() -> int:
    sys.stdout.reconfigure(encoding="utf-8")
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("a", type=Path)
    ap.add_argument("b", type=Path)
    ap.add_argument("--all", action="store_true", help="include read-only status registers")
    args = ap.parse_args()

    a, b = load_image(args.a), load_image(args.b)
    db = yaml.safe_load((ROOT / "schema/registers/tv5725.yaml").read_text(encoding="utf-8"))
    common = set(a) & set(b)
    differing = {k for k in common if a[k] != b[k]}
    if not args.all:
        differing = {k for k in differing if not (k[0] == 0 and k[1] < 0x30)}
    print(f"{len(common)} registers in both, {len(differing)} differ")

    # Report the narrowest fields covering each differing byte; skip a byte-wide catch-all when
    # narrower fields already explain the change.
    reported: set[str] = set()
    rows = []
    for name, f in db["fields"].items():
        nbytes = (f["lsb"] + f["width"] + 7) // 8
        keys = {(f["seg"], f["addr"] + i) for i in range(nbytes)}
        if not keys & differing:
            continue
        va, vb = field_value(a, f), field_value(b, f)
        if va is None or vb is None or va == vb:
            continue
        rows.append((f["seg"], f["addr"], f["lsb"], -f["width"], name, va, vb))
    rows.sort()
    for seg, addr, _lsb, negw, name, va, vb in rows:
        if -negw == 8 and any(
            r[0] == seg and r[1] == addr and r[4] != name and -r[3] < 8 for r in rows
        ):
            continue  # a whole-byte alias; the narrow fields say more
        if name in reported:
            continue
        reported.add(name)
        print(f"  S{seg}_{addr:02X} {name:28s} {va:#x} -> {vb:#x}  ({va} -> {vb})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
