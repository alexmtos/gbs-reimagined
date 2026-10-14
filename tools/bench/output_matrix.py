"""Walk the output modes with the connected source and capture each: the M1 gate's matrix.

    uv run --group bench --group board tools/bench/output_matrix.py --source 480p-wii \
        [--modes 960p 1024p 1080p 720p 480p] [--settle 18] [--port COM9]

For every mode: `output <mode>` and `keep` on the console, wait for the program to settle, read the
status (program, lock, rates) and capture frames. Prints one line per mode and writes each capture
to bench-runs/<timestamp>-<source>-<mode>/. Restores the output mode that was set before.
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


def main() -> int:
    sys.stdout.reconfigure(encoding="utf-8", line_buffering=True)
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--source", required=True, help="short name of the source, used in labels")
    ap.add_argument("--modes", nargs="+", default=["960p", "1024p", "1080p", "720p", "480p"])
    ap.add_argument("--settle", type=float, default=18.0, help="seconds after keep before reading")
    ap.add_argument("--port", default="COM9")
    args = ap.parse_args()

    before = re.search(
        r"output\.resolution = (\S+)", console(args.port, ["get output.resolution"], 1, 3)
    )
    for mode in args.modes:
        console(args.port, [f"output {mode}", "keep"], gap=4, seconds=6)
        time.sleep(args.settle)
        text = console(args.port, ["status"], gap=1, seconds=3)
        prog = re.search(r"program (\S+), loads (\d+), stable (\d+)", text)
        clock = re.search(r"clock (\d+) Hz \((\w+)\)( locked)?, source (\d+) mHz", text)
        label = f"{args.source}-{mode}"
        cap = subprocess.run(
            [
                sys.executable,
                str(ROOT / "tools/bench/capture.py"),
                "--frames",
                "3",
                "--label",
                label,
            ],
            capture_output=True,
            text=True,
            encoding="utf-8",
        )
        verdict = (cap.stdout.strip().splitlines() or [cap.stderr.strip()])[0].split(" -> ")[0]
        status = (f"{prog[1]} stable {prog[3]}" if prog else "no program") + (
            f", {int(clock[1]) / 1e6:.2f} MHz {'locked' if clock[3] else clock[2]}" if clock else ""
        )
        print(f"{mode:6s} | {status:40s} | {verdict}")
    if before:
        restore = before[1].lstrip("r") if before[1].startswith("r") else before[1]
        console(args.port, [f"output {restore}", "keep"], gap=4, seconds=6)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
