"""Stage the installer site's firmware: the images, the ESP Web Tools manifests and latest.json.

    uv run tools/release/installer_site.py --build   # this machine's build (version.h)
    uv run tools/release/installer_site.py --image gbs-reimagined-1.0.0-gbsc.bin \
        --ota gbs-reimagined-1.0.0-gbsc.ota.bin.gz --version 1.0.0 --channel stable \
        --notes https://github.com/alexmtos/gbs-reimagined/releases/tag/v1.0.0

Writes into installer/public/ (or --out), which the installer build copies as it is (ADR 0012):
    firmware/<image> and firmware/<update file>
    manifest.json (stable) or manifest-beta.json: what the install button flashes
    latest.json: what the board's web page reads to offer an update (web/src/update/update.ts)
A channel staged again replaces that channel only; files no channel names any more are removed.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from ota_image import ota_image  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
BOARD = "gbsc"
# The installer compares this with the name the board reports over Improv (firmware/src/main.cpp)
# to tell an update of this firmware from another one.
NAME = "GBS Reimagined"
# After flashing, how long the installer waits for the board to answer over Improv (10 s by default
# in ESP Web Tools 10.4.0). The board answers 2.45 s after a reset, or once it has joined a saved
# network: 7.7 s on the bench; by design within about 10.5 s (docs/bench/M3.md,
# firmware/src/core/improv_serial.cpp).
IMPROV_WAIT_S = 20
MANIFESTS = {"stable": "manifest.json", "beta": "manifest-beta.json"}
VERSION_RE = re.compile(r"^[0-9A-Za-z.+-]+$")


def manifest(version: str, image_path: str) -> dict:
    return {
        "name": NAME,
        "version": version,
        "new_install_prompt_erase": True,
        "new_install_improv_wait_time": IMPROV_WAIT_S,
        "builds": [{"chipFamily": "ESP8266", "parts": [{"path": image_path, "offset": 0}]}],
    }


def stage(
    out: Path, channel: str, version: str, image: bytes, ota: bytes, notes: str | None
) -> list[Path]:
    """Writes one channel into `out`; returns the files written."""
    if not VERSION_RE.match(version):
        raise ValueError(f"not a usable version: {version!r}")
    if not image or image[0] != 0xE9:
        raise ValueError("the image is not an ESP8266 image")
    if ota[:2] != b"\x1f\x8b":
        raise ValueError("the update file is not a gzip file")
    stem = f"gbs-reimagined-{version}-{BOARD}"
    image_path = f"firmware/{stem}.bin"
    ota_path = f"firmware/{stem}.ota.bin.gz"
    (out / "firmware").mkdir(parents=True, exist_ok=True)
    (out / image_path).write_bytes(image)
    (out / ota_path).write_bytes(ota)
    write_json(out / MANIFESTS[channel], manifest(version, image_path))

    latest_file = out / "latest.json"
    latest = {"format": "gbsr-latest", "version": 1}
    if latest_file.exists():
        latest = json.loads(latest_file.read_text(encoding="utf-8"))
    entry: dict = {"version": version}
    if notes:
        entry["notes"] = notes
    entry["files"] = {
        BOARD: {"ota": ota_path, "sha256": hashlib.sha256(ota).hexdigest(), "size": len(ota)}
    }
    latest[channel] = entry
    write_json(latest_file, latest)
    prune(out)
    return [out / image_path, out / ota_path, out / MANIFESTS[channel], latest_file]


def prune(out: Path) -> None:
    """Removes firmware files that neither a manifest nor latest.json names."""
    named: set[str] = set()
    for name in [*MANIFESTS.values(), "latest.json"]:
        path = out / name
        if path.exists():
            named |= set(re.findall(r'"(firmware/[^"]+)"', path.read_text(encoding="utf-8")))
    for path in sorted((out / "firmware").glob("*")):
        if f"firmware/{path.name}" not in named:
            path.unlink()


def write_json(path: Path, data: dict) -> None:
    path.write_bytes((json.dumps(data, indent=2) + "\n").encode("utf-8"))


def build_version() -> str:
    text = (ROOT / "firmware/src/version.h").read_text(encoding="utf-8")
    found = re.search(r'#define GBSR_VERSION "([^"]+)"', text)
    if not found:
        raise SystemExit("firmware/src/version.h: no GBSR_VERSION")
    return found.group(1)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--build", action="store_true", help="stage firmware/.pio/build/gbsc")
    ap.add_argument("--image", type=Path, help="the full image, flashed at offset 0")
    ap.add_argument("--ota", type=Path, help="the update file (tools/release/ota_image.py)")
    ap.add_argument("--version")
    ap.add_argument("--channel", choices=list(MANIFESTS), default="stable")
    ap.add_argument("--notes", help="release notes URL")
    ap.add_argument("--out", type=Path, default=ROOT / "installer/public")
    ap.add_argument("--clean", action="store_true", help="drop every channel staged before")
    args = ap.parse_args()
    sys.stdout.reconfigure(encoding="utf-8")

    if args.build:
        version = args.version or build_version()
        image = (ROOT / "firmware/.pio/build/gbsc/firmware.bin").read_bytes()
        ota = ota_image(image, BOARD, version, f"gbs-reimagined-{version}-{BOARD}.bin")
    elif args.image and args.ota and args.version:
        version = args.version
        image = args.image.read_bytes()
        ota = args.ota.read_bytes()
    else:
        ap.error("give --build, or --image, --ota and --version")
    if args.clean:
        for name in [*MANIFESTS.values(), "latest.json", "firmware"]:
            path = args.out / name
            if path.is_dir():
                shutil.rmtree(path)
            elif path.exists():
                path.unlink()
    try:
        written = stage(args.out, args.channel, version, image, ota, args.notes)
    except ValueError as e:
        print(f"error: {e}")
        return 1
    for path in written:
        print(f"{path.relative_to(args.out).as_posix()}: {path.stat().st_size} bytes")
    print(f"{args.channel} {version} staged in {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
