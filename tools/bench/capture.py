"""Capture frames from the HDMI capture card and classify them: picture, black, or no frames.

    uv run --group bench tools/bench/capture.py [--frames 30] [--label name] [--host 192.168.1.50]

Exit codes: 0 picture, 1 black frame, 2 the card delivered no frames (a rig fault: the card is
busy in another program, unplugged, or has no signal lock — not a firmware verdict).

Results go to bench-runs/<timestamp>-<label>/: the frames as PNG, result.json with the metrics,
and the board's /api/status when --host is given (the input signature of the run).

Rig facts this script encodes (from the reference project,
docs/research/gbs-control-lessons-platform.md §7.4):
- the NZXT Signal HD60 presents yuyv422 on some modes and nv12 on others; naming the wrong one
  returns zero frames, which looks like a black picture, so formats are tried in turn;
- the card encodes BT.601 even at 1080 lines, while ffmpeg assumes BT.709 there;
- DirectShow can lock up after repeated opens: capture once per run and reuse the frames.
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import subprocess
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_DEVICE = os.environ.get("GBS_CAPTURE_DEVICE", "NZXT Signal HD60 Video")
PIXEL_FORMATS = (None, "yuyv422", "nv12")
BLACK_MEAN = 6.0  # 8-bit luma; the HDMI chain clamps black near 6 (reference calibration notes)
BLACK_MAX = 40


def ffmpeg_exe() -> str:
    import imageio_ffmpeg

    return imageio_ffmpeg.get_ffmpeg_exe()


def grab(out_dir: Path, device: str, frames: int, matrix: str) -> list[Path]:
    exe = ffmpeg_exe()
    for fmt in PIXEL_FORMATS:
        for old in out_dir.glob("frame_*.png"):
            old.unlink()
        cmd = [exe, "-hide_banner", "-loglevel", "error", "-f", "dshow", "-rtbufsize", "256M"]
        if fmt:
            cmd += ["-pixel_format", fmt]
        cmd += ["-video_size", "1920x1080", "-framerate", "60", "-i", f"video={device}"]
        cmd += ["-vf", f"scale=in_color_matrix={matrix}:out_range=full", "-frames:v", str(frames)]
        cmd += ["-y", str(out_dir / "frame_%03d.png")]
        try:
            subprocess.run(cmd, check=False, timeout=30, capture_output=True)
        except subprocess.TimeoutExpired:
            continue
        got = sorted(out_dir.glob("frame_*.png"))
        if got:
            return got
    return []


def analyse(paths: list[Path]) -> dict:
    import cv2
    import numpy as np

    lumas = []
    for p in paths:
        img = cv2.imread(str(p), cv2.IMREAD_COLOR)
        if img is None:
            continue
        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        lumas.append((float(gray.mean()), int(gray.max()), float((gray > 32).mean())))
    if not lumas:
        return {"frames": 0}
    arr = np.array(lumas)
    return {
        "frames": len(lumas),
        "mean_luma": round(float(arr[:, 0].mean()), 2),
        "max_luma": int(arr[:, 1].max()),
        "lit_fraction": round(float(arr[:, 2].mean()), 4),
    }


def board_status(host: str) -> dict | None:
    try:
        with urllib.request.urlopen(f"http://{host}/api/status", timeout=5) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except (OSError, ValueError) as exc:
        return {"error": str(exc)}


def main() -> int:
    sys.stdout.reconfigure(encoding="utf-8")
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--frames", type=int, default=30)
    ap.add_argument("--label", default="capture")
    ap.add_argument("--device", default=DEFAULT_DEVICE)
    ap.add_argument("--matrix", default="bt601", choices=["bt601", "bt709"])
    ap.add_argument("--host", default=os.environ.get("GBS_HOST"), help="board address for status")
    args = ap.parse_args()

    stamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    out_dir = ROOT / "bench-runs" / f"{stamp}-{args.label}"
    out_dir.mkdir(parents=True, exist_ok=True)

    status = board_status(args.host) if args.host else None
    frames = grab(out_dir, args.device, args.frames, args.matrix)
    metrics = analyse(frames)
    if metrics["frames"] == 0:
        verdict, code = "no-frames", 2
    elif metrics["mean_luma"] < BLACK_MEAN and metrics["max_luma"] < BLACK_MAX:
        verdict, code = "black", 1
    else:
        verdict, code = "picture", 0

    result = {"verdict": verdict, "metrics": metrics, "device": args.device, "status": status}
    (out_dir / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    first = frames[len(frames) // 2] if frames else None
    print(f"{verdict}: {metrics} -> {out_dir}")
    if first:
        print(f"look at: {first}")
    return code


if __name__ == "__main__":
    raise SystemExit(main())
