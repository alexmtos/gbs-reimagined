"""B-05: which output program gives a clean picture for the connected source.

    uv run --group bench tools/bench/program_matrix.py --source ps1-240p-menu \
        [--host 10.0.0.119] [--modes r960p r1024p r1080p r720p r480p] [--frames 30] [--settle 20]

For every output mode, over the board's HTTP API (no USB cable needed):
1. set output.resolution and keep it, with frame lock on and integer scale off;
2. wait for `scaling` with the lock settled, then the HDMI chain (--settle seconds);
3. read the status: the mode in effect, the frame-lock state, source and output rates;
4. capture frames with tools/bench/capture.py and measure how still a still pattern stays.

The criterion for a clean program is not "gives a picture"
(docs/research/gbs-control-lessons-videopath.md §3.4):
- the output follows the source's rate within 0.001 Hz (frame lock);
- the vertical ratio, output lines over source lines, sits within 0.5 % of a whole number. That is a
  property of the program's window, read from its geometry, not from these captures;
- the picture does not shimmer or tear. The capture card always delivers 1920x1080 and rescales the
  other modes itself, so line thickness can be judged only at 1080p. Shimmer, tearing and dropped
  frames still show as frame-to-frame change in every mode, given a still test pattern.

With a 50 Hz source, 1280x960 with frame lock disturbs the board's own Wi-Fi (docs/bench/B05.md):
leave r960p out of --modes and capture it with the board driven over the serial console.

The settings changed for the run (output resolution, frame lock, integer scale) are put back at the
end, keeping the reply for each. Results: one bench-runs/<timestamp>-b05-<source>-<mode>/ folder per
mode (from capture.py), plus bench-runs/<timestamp>-b05-<source>.json with the table.
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import subprocess
import sys
import time
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MODES = ["r960p", "r1024p", "r1080p", "r720p", "r480p"]
STILL_LEVEL = 8.0  # 8-bit luma: a pixel whose level moves more than this over the run is unstable


def api(host: str, path: str, body: dict | None = None) -> dict:
    data = None if body is None else json.dumps(body).encode()
    req = urllib.request.Request(
        f"http://{host}{path}",
        data=data,
        headers={"Content-Type": "application/json"} if data else {},
        method="POST" if data else "GET",
    )
    with urllib.request.urlopen(req, timeout=10) as r:
        return json.loads(r.read().decode())


def set_kept(host: str, values: dict) -> dict:
    """Sets settings and keeps them at once (a risky change would otherwise revert)."""
    answer = api(host, "/api/settings", values)
    if answer.get("revert"):
        api(host, "/api/command", {"name": "keep"})
    return answer


def settled(host: str, timeout: float) -> dict:
    """Waits for `scaling` with frame lock settled (locked, or inactive with a reason other than
    still locking); returns the last status."""
    end = time.monotonic() + timeout
    s: dict = {}
    while time.monotonic() < end:
        s = api(host, "/api/status")
        out = s["output"]
        if s["headline"] == "scaling" and (
            out["frameLock"] == "locked" or out.get("frameLockReason") not in (None, "locking")
        ):
            return s
        time.sleep(1)
    return s


def stillness(run: Path) -> dict:
    """How still the captured frames are: per-pixel luma spread over the run."""
    import cv2
    import numpy as np

    frames = sorted(run.glob("frame_*.png"))
    stack = np.stack(
        [cv2.cvtColor(cv2.imread(str(f)), cv2.COLOR_BGR2GRAY).astype(np.float32) for f in frames]
    )
    spread = stack.max(axis=0) - stack.min(axis=0)
    lit = stack.mean(axis=0) > 40
    means = stack.mean(axis=(1, 2))
    return {
        "frames": len(frames),
        "unstable_share": round(float((spread > STILL_LEVEL)[lit].mean()) if lit.any() else 0.0, 5),
        "spread_p99": round(float(np.percentile(spread[lit], 99)) if lit.any() else 0.0, 1),
        "frame_mean_range": round(float(means.max() - means.min()), 2),
    }


def main() -> int:
    sys.stdout.reconfigure(encoding="utf-8", line_buffering=True)
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--source", required=True, help="short name of the source and pattern")
    ap.add_argument("--host", default="10.0.0.119")
    ap.add_argument("--modes", nargs="+", default=MODES)
    ap.add_argument("--frames", type=int, default=30)
    ap.add_argument("--settle", type=float, default=20.0, help="seconds for the HDMI chain")
    args = ap.parse_args()

    before = api(args.host, "/api/settings")["values"]
    kept = {k: before[k] for k in ("output.resolution", "output.frameLock", "output.integerScale")}
    print(f"settings before the run: {kept}")
    rows = []
    try:
        set_kept(args.host, {"output.frameLock": "on", "output.integerScale": "off"})
        for mode in args.modes:
            set_kept(args.host, {"output.resolution": mode})
            s = settled(args.host, 30)
            time.sleep(args.settle)
            s = api(args.host, "/api/status")
            src, out = s["source"], s["output"]
            label = f"b05-{args.source}-{mode}"
            cap = subprocess.run(
                [sys.executable, str(ROOT / "tools/bench/capture.py"),
                 "--frames", str(args.frames), "--label", label, "--host", args.host],
                capture_output=True, text=True, encoding="utf-8",
            )  # fmt: skip
            run = sorted((ROOT / "bench-runs").glob(f"*-{label}"))[-1]
            still = stillness(run) if cap.returncode == 0 else {}
            row = {
                "mode": mode,
                "in_effect": out["mode"],
                # The input signature of the result (tools/CLAUDE.md).
                "source": f"{src['port']} {src['sync']} {src['standard']} {src['lines']} lines",
                "lock": out["frameLock"],
                "lock_reason": out.get("frameLockReason"),
                "rate_error_hz": round(out["rateHz"] - src["vFreqHz"], 4),
                "picture": {0: "picture", 1: "black", 2: "no frames"}.get(cap.returncode, "?"),
                **still,
                "run": run.name,
            }
            rows.append(row)
            print(
                f"{mode:8} {row['picture']:9} lock {row['lock']:8} "
                f"rate {row['rate_error_hz']:+.4f} Hz  "
                f"unstable {row.get('unstable_share', '-')}  p99 {row.get('spread_p99', '-')}"
            )
    finally:
        set_kept(args.host, kept)
        print(f"settings put back: {kept}")
    stamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    out_file = ROOT / "bench-runs" / f"{stamp}-b05-{args.source}.json"
    out_file.write_text(json.dumps({"source": args.source, "rows": rows}, indent=2) + "\n")
    print(f"table: {out_file}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
