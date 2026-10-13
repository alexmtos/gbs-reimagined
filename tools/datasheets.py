"""Fetch the vendor datasheets listed in docs/hardware/datasheets.json into docs/hardware/pdf/.

The PDFs are not committed (their licences do not allow redistribution, and the hosting pages are
not permanent). Each download is checked against its pinned SHA-256; an unpinned document prints
its hash so it can be pinned.

Usage:
    uv run tools/datasheets.py                 # download what is missing
    uv run tools/datasheets.py --from <dir>    # copy from a local folder instead (same file names)
"""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "docs/hardware/datasheets.json"
DEST = ROOT / "docs/hardware/pdf"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--from", dest="source", type=Path, help="local folder holding the PDFs")
    args = ap.parse_args()

    docs = json.loads(MANIFEST.read_text(encoding="utf-8"))["documents"]
    DEST.mkdir(parents=True, exist_ok=True)
    failures = 0
    for doc in docs:
        target = DEST / doc["file"]
        if not target.exists():
            try:
                if args.source:
                    shutil.copyfile(args.source / doc["file"], target)
                else:
                    req = urllib.request.Request(doc["url"], headers={"User-Agent": "Mozilla/5.0"})
                    with urllib.request.urlopen(req, timeout=60) as resp, target.open("wb") as out:
                        shutil.copyfileobj(resp, out)
            except (OSError, ValueError) as exc:
                print(f"FAIL  {doc['file']}: {exc}")
                target.unlink(missing_ok=True)
                failures += 1
                continue
        digest = sha256(target)
        if doc["sha256"] is None:
            print(f"UNPINNED {doc['file']} sha256={digest}")
        elif digest != doc["sha256"]:
            print(f"MISMATCH {doc['file']} expected {doc['sha256']} got {digest}")
            failures += 1
        else:
            print(f"OK    {doc['file']}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
