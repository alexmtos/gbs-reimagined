"""Poll the board's console status and print a line whenever something that matters changes.

    uv run --group board tools/bench/watch_status.py [--port COM9] [--seconds 180] [--every 2]

For bench sessions where the user operates the source (switching 240p/480i, swapping consoles):
each printed line is one change of state, standard, interlace, deinterlacer, program, load count
or sync loss, with the time since the start.
"""

from __future__ import annotations

import argparse
import re
import sys
import time

FIELDS = {
    "state": re.compile(r"^state (\S+) \| (.+?) on (\S+), (\S+) sync(, deinterlacing)?"),
    "lines": re.compile(
        r"^source lines (\d+), HPERIOD_IF (\d+), VPERIOD_IF (\d+)( \(interlaced\))?"
    ),
    "program": re.compile(r"^program (\S+), loads (\d+), stable (\d+), no-sync (\d+)"),
}


def main() -> int:
    import serial

    sys.stdout.reconfigure(encoding="utf-8", line_buffering=True)
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--port", default="COM9")
    ap.add_argument("--seconds", type=float, default=180)
    ap.add_argument("--every", type=float, default=2.0)
    args = ap.parse_args()

    last: tuple | None = None
    start = time.monotonic()
    buf = ""
    with serial.Serial(args.port, 115200, timeout=0.2) as ser:
        next_poll = start
        while time.monotonic() - start < args.seconds:
            if time.monotonic() >= next_poll:
                ser.write(b"status\r\n")
                next_poll += args.every
            buf += ser.read(1024).decode("utf-8", errors="replace")
            if "longest tick" not in buf:
                continue
            block, _, buf = buf.partition("longest tick")
            seen: dict[str, re.Match] = {}
            for line in block.splitlines():
                for key, rx in FIELDS.items():
                    m = rx.match(line.strip())
                    if m:
                        seen[key] = m
            if len(seen) < 3:
                continue
            st, ln, pr = seen["state"], seen["lines"], seen["program"]
            noSync = int(pr[4]) > 0
            key = (st[1], st[2], st[3], st[4], bool(st[5]), bool(ln[4]), pr[1], pr[2], noSync)
            if key != last:
                last = key
                print(
                    f"t={time.monotonic() - start:6.1f}s {st[1]:9s} {st[2]} {st[3]} {st[4]}"
                    f" | interlaced={'yes' if ln[4] else 'no '} VPERIOD_IF={ln[3]}"
                    f" deint={'on ' if st[5] else 'off'} | {pr[1]} loads={pr[2]}"
                    f"{' NO-SYNC' if noSync else ''}"
                )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
