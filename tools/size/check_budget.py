"""Fail when the firmware or the web bundle exceeds its memory budget.

    uv run tools/size/check_budget.py --size firmware/.pio/build/gbsc/size.json \
        [--baseline previous-size.json] [--web web/dist/index.html.gz] [--summary out.md]

Budgets live in tools/size/budget.json; the reasons in docs/design/memory-budget.md.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--size", type=Path, required=True, help="size.json from the firmware build")
    ap.add_argument("--baseline", type=Path, help="size.json of the base branch, for growth")
    ap.add_argument("--web", type=Path, help="gzip web bundle to check")
    ap.add_argument("--summary", type=Path, help="append a Markdown table here (CI step summary)")
    args = ap.parse_args()
    sys.stdout.reconfigure(encoding="utf-8")

    budget = json.loads((ROOT / "tools/size/budget.json").read_text(encoding="utf-8"))
    size = json.loads(args.size.read_text(encoding="utf-8"))
    rows: list[tuple[str, int, str, bool]] = []

    def check(name: str, value: int, limit: int) -> None:
        rows.append((name, value, f"≤ {limit}", value <= limit))

    check("static RAM", size["static_ram"], budget["static_ram_max"])
    check("IRAM", size["iram"], budget["iram_max"])
    check("flash", size["flash"], budget["flash_max"])
    if args.baseline and args.baseline.exists():
        base = json.loads(args.baseline.read_text(encoding="utf-8"))
        growth = size["static_ram"] - base["static_ram"]
        check("static RAM growth", growth, budget["static_ram_growth_max"])
    if args.web:
        check("web bundle (gzip)", args.web.stat().st_size, budget["web_bundle_gzip_max"])

    lines = ["| Quantity | Bytes | Budget | |", "|---|---:|---|---|"]
    for name, value, limit, ok in rows:
        lines.append(f"| {name} | {value} | {limit} | {'✅' if ok else '❌'} |")
    table = "\n".join(lines)
    print(table)
    if args.summary:
        with args.summary.open("a", encoding="utf-8") as fh:
            fh.write("### Memory budget\n\n" + table + "\n")
    failed = [name for name, _, _, ok in rows if not ok]
    if failed:
        print("over budget: " + ", ".join(failed), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
