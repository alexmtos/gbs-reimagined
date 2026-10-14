"""Measure where the picture sits in captured frames: its bounding box, size and centre offset.

    uv run --group bench tools/bench/picture_extent.py bench-runs/<run> [bench-runs/<run> ...]

For each run folder, reads frame_001.png and reports the box of rows and columns that hold more
than 20 pixels brighter than level 40 (the brightest channel), the box's width, height and ratio,
and how far its centre sits from the frame's. Used for the geometry work (docs/bench/M2.md): a test
pattern that reaches the edges of the source picture (the 240p Test Suite's grid or menu border)
gives the picture's extent on the capture.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

LEVEL = 40  # 8-bit; the HDMI chain clamps black near 6
MIN_PIXELS = 20  # per row or column, so isolated noise does not widen the box


def extent(frame: Path) -> dict | None:
    import cv2
    import numpy as np

    img = cv2.imread(str(frame), cv2.IMREAD_COLOR)
    if img is None:
        raise SystemExit(f"cannot read {frame}")
    lit = img.max(axis=2) > LEVEL
    cols = np.where(lit.sum(axis=0) > MIN_PIXELS)[0]
    rows = np.where(lit.sum(axis=1) > MIN_PIXELS)[0]
    if len(cols) == 0 or len(rows) == 0:
        return None
    h, w = lit.shape
    x0, x1, y0, y1 = int(cols[0]), int(cols[-1]), int(rows[0]), int(rows[-1])
    return {
        "frame": f"{w}x{h}",
        "x": (x0, x1),
        "y": (y0, y1),
        "width": x1 - x0 + 1,
        "height": y1 - y0 + 1,
        "centre_dx": (x0 + x1) / 2 - (w - 1) / 2,
        "centre_dy": (y0 + y1) / 2 - (h - 1) / 2,
    }


def main() -> int:
    sys.stdout.reconfigure(encoding="utf-8")
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("runs", nargs="+", type=Path)
    args = ap.parse_args()
    for run in args.runs:
        e = extent(run / "frame_001.png")
        if e is None:
            print(f"{run.name}: black")
            continue
        print(
            f"{run.name}: {e['frame']} x {e['x'][0]}..{e['x'][1]} ({e['width']})"
            f" y {e['y'][0]}..{e['y'][1]} ({e['height']}) ratio {e['width'] / e['height']:.3f}"
            f" centre {e['centre_dx']:+.1f},{e['centre_dy']:+.1f}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
