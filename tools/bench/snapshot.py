"""One bench snapshot: the board's status and ADC levels from the console, plus a capture.

    uv run --group bench --group board tools/bench/snapshot.py --label <name> [--port COM9]

Prints a compact summary and writes bench-runs/<timestamp>-<label>/snapshot.txt next to the
capture's frames. Used to compare the board before and after a physical change (a switch, a cable,
a source), e.g. for the board controls investigation (docs/hardware/board-gbsc.md, B-02).
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def console(port: str, lines: list[str], gap: float, seconds: float) -> str:
    import serial

    out = []
    with serial.Serial(port, 115200, timeout=0.2) as ser:
        start = time.monotonic()
        pending = list(lines)
        next_at = start + 0.5
        while time.monotonic() - start < seconds:
            if pending and time.monotonic() >= next_at:
                ser.write((pending.pop(0) + "\r\n").encode("ascii"))
                next_at = time.monotonic() + gap
            chunk = ser.read(512)
            if chunk:
                out.append(chunk.decode("utf-8", errors="replace"))
    return "".join(out)


def level_summary(line: str) -> str:
    m = re.match(r"top (\w+), at ceiling (\d+) permille \| level:count(.*)", line.strip())
    if not m:
        return "?"
    hist = {}
    for tok in m[3].split():
        v, c = tok.split(":")
        hist[int(v, 16)] = int(c)
    total = sum(hist.values()) or 1
    black = sum(c for v, c in hist.items() if v <= 2) * 100 / total
    return f"top {m[1]}, clipped {int(m[2]) / 10:.1f}%, black {black:.0f}%"


def main() -> int:
    sys.stdout.reconfigure(encoding="utf-8")
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--label", required=True)
    ap.add_argument("--port", default="COM9")
    args = ap.parse_args()

    text = console(
        args.port,
        ["expert on", "status", "levels r 1500", "levels g 1500", "levels b 1500"],
        gap=1.2,
        seconds=8,
    )
    status = [
        ln.strip()
        for ln in text.splitlines()
        if ln.startswith(
            ("state ", "source lines", "program ", "clock ", "geometry ", "ADC offset", "SOG ")
        )
    ]
    levels = [ln for ln in text.splitlines() if ln.startswith("top ")]
    cap = subprocess.run(
        [
            sys.executable,
            str(ROOT / "tools/bench/capture.py"),
            "--frames",
            "5",
            "--label",
            args.label,
        ],
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    verdict = cap.stdout.strip().splitlines()[0] if cap.stdout.strip() else cap.stderr.strip()
    run_dir = next(iter(sorted((ROOT / "bench-runs").glob(f"*-{args.label}"))[-1:]), None)

    lines = [f"[{args.label}]", *status]
    for name, ln in zip("RGB", levels, strict=False):
        lines.append(f"{name}: {level_summary(ln)}")
    lines.append(f"capture: {verdict}")
    summary = "\n".join(lines)
    print(summary)
    if run_dir is not None:
        (run_dir / "snapshot.txt").write_text(summary + "\n\n" + text, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
