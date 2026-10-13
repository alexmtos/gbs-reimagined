"""One-time importer for the TV5725 register database (schema/registers/tv5725.yaml).

The database is factual chip data assembled from two sources:

1. Field loci (name, segment, byte offset, bit offset, width) as declared in gbs-control's
   `tv5725.h` (https://github.com/ramapcsx2/gbs-control, GPL-3.0). Only the facts are taken;
   see NOTICE.md.
2. The vendor register definition RD-5725-1.1 (text produced by `pdftotext -layout`): the access
   (RO or R/W) of every register, its block title and the page it is described on.

Run it again only when one of the sources changes. Afterwards the YAML is the source of truth:
it is reviewed by hand and consumed by tools/codegen/gen_registers.py.

Usage:
    uv run tools/codegen/import_tv5725.py --header <tv5725.h> --regdef <RD-5725-1.1.pdf> \
        [--out schema/registers/tv5725.yaml]
"""

from __future__ import annotations

import argparse
import hashlib
import re
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

FIELD_RE = re.compile(
    r"typedef\s+UReg<\s*0x([0-9A-Fa-f]+)\s*,\s*0x([0-9A-Fa-f]+)\s*,\s*(\d+)\s*,\s*(\d+)\s*>\s*(\w+)\s*;"
)
# "R/W" must be tried before the bare "R"; the lookahead stops "R" from matching the R of "R/W".
REG_RE = re.compile(
    r"^\s*(?P<title>\S.*?)\s{2,}REG\s+S(?P<seg>[0-5])_(?P<addr>[0-9A-Fa-f]{2})\s*,\s*(?P<acc>R/W|RO|R(?![/\w]))"
)
# The only field the vendor documents as signed ("the highest bit ... is sign bit").
SIGNED_FIELDS = {"VDS_UV_BLK_VAL"}
# Declarations in tv5725.h that are not chip facts: gbs-control's own scratch-storage
# conventions in undocumented bytes, and a test helper.
EXCLUDED_FIELDS = {
    "GBS_PRESET_ID",
    "GBS_OPTION_SCALING_RGBHV",
    "GBS_RUNTIME_FTL_ADJUSTED",
    "GBS_PRESET_DISPLAY_CLOCK",
    "VERYWIDEDUMMYREG",
}


@dataclass
class Register:
    seg: int
    addr: int
    access: str
    title: str
    pages: list[int]


@dataclass
class Field:
    name: str
    seg: int
    addr: int
    lsb: int
    width: int


def pdf_to_text(pdf: Path) -> str:
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "regdef.txt"
        subprocess.run(["pdftotext", "-layout", str(pdf), str(out)], check=True)
        return out.read_text(encoding="utf-8", errors="replace")


def parse_registers(text: str) -> dict[tuple[int, int], Register]:
    regs: dict[tuple[int, int], Register] = {}
    for page_no, page in enumerate(text.split("\f"), start=1):
        for line in page.splitlines():
            m = REG_RE.match(line)
            if not m:
                continue
            key = (int(m["seg"]), int(m["addr"], 16))
            access = "rw" if m["acc"] == "R/W" else "ro"
            title = re.sub(r"\s+", " ", m["title"]).strip()
            title = re.sub(r"\s+[0-9A-Fa-f]{1,2}$", "", title)
            if key in regs:
                prev = regs[key]
                if prev.access != access:
                    sys.exit(f"S{key[0]}_{key[1]:02X}: access conflict {prev.access} vs {access}")
                if page_no not in prev.pages:
                    prev.pages.append(page_no)
            else:
                regs[key] = Register(key[0], key[1], access, title, [page_no])
    return regs


def parse_fields(header: str) -> list[Field]:
    fields = [
        Field(m[5], int(m[1], 16), int(m[2], 16), int(m[3]), int(m[4]))
        for m in FIELD_RE.finditer(header)
        if m[5] not in EXCLUDED_FIELDS
    ]
    names = [f.name for f in fields]
    dupes = {n for n in names if names.count(n) > 1}
    if dupes:
        sys.exit(f"duplicate field names: {sorted(dupes)}")
    return fields


def field_bytes(f: Field) -> list[int]:
    nbytes = (f.lsb + f.width + 7) // 8
    return [f.addr + i for i in range(nbytes)]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--header", type=Path, required=True)
    ap.add_argument("--regdef", type=Path, required=True)
    ap.add_argument("--out", type=Path, default=Path("schema/registers/tv5725.yaml"))
    args = ap.parse_args()

    regs = parse_registers(pdf_to_text(args.regdef))
    fields = parse_fields(args.header.read_text(encoding="utf-8"))
    pdf_sha = hashlib.sha256(args.regdef.read_bytes()).hexdigest()

    lines: list[str] = []
    w = lines.append
    w("# TV5725 register database. Factual chip data; see NOTICE.md for provenance.")
    w("# Edit by hand, then run `uv run tools/codegen/generate.py`. Imported by")
    w("# tools/codegen/import_tv5725.py from field loci in gbs-control's tv5725.h and the vendor")
    w("# register definition RD-5725-1.1.")
    w("chip: TV5725")
    w("i2c_address: 0x17")
    w("segment_select: 0xF0")
    w("segments: 6")
    w("datasheet:")
    w("  id: RD-5725-1.1")
    w(f"  sha256: {pdf_sha}")
    w("")
    w("# Every register the vendor documents: access, block title, datasheet page(s).")
    w("registers:")
    for (seg, addr), r in sorted(regs.items()):
        pages = ", ".join(str(p) for p in r.pages)
        w(f'  S{seg}_{addr:02X}: {{access: {r.access}, block: "{r.title}", pages: [{pages}]}}')
    w("")
    w("# Fields: segment, byte offset, lowest bit, width in bits (little-endian across bytes).")
    w("# access is derived from the registers the field occupies; doc: false means the vendor")
    w("# document does not describe that register.")
    w("fields:")
    undocumented = 0
    for f in sorted(fields, key=lambda f: (f.seg, f.addr, f.lsb, -f.width, f.name)):
        occupied = [regs.get((f.seg, b)) for b in field_bytes(f)]
        documented = all(r is not None for r in occupied)
        access = "ro" if any(r is not None and r.access == "ro" for r in occupied) else "rw"
        extra = ""
        if f.name in SIGNED_FIELDS:
            extra += ", signed: true"
        if not documented:
            extra += ", doc: false"
            undocumented += 1
        w(
            f"  {f.name}: {{seg: {f.seg}, addr: 0x{f.addr:02X}, lsb: {f.lsb}, width: {f.width},"
            f" access: {access}{extra}}}"
        )

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    ro = sum(1 for r in regs.values() if r.access == "ro")
    print(
        f"{len(regs)} registers ({ro} RO), {len(fields)} fields "
        f"({undocumented} on undocumented registers) -> {args.out}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
