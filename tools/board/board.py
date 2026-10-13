"""Board helpers over USB (CH340): list ports, back up, flash, restore, talk to the console.

    uv run --group board tools/board/board.py ports
    uv run --group board tools/board/board.py backup  --port COM3
    uv run --group board tools/board/board.py flash   --port COM3 --image <firmware.bin>
    uv run --group board tools/board/board.py restore --port COM3 --image backups/<file>.flash.bin
    uv run --group board tools/board/board.py console --port COM3 [--send status] [--seconds 5]

Writes to the board only in `flash` and `restore`, and only after printing what will be written.
See the flash-board skill for the procedure.
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import re
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BACKUPS = ROOT / "backups"
FLASH_SIZE = 0x400000  # 4 MB, ESP-12F
BAUD = 460800
CH340_IDS = {(0x1A86, 0x7523), (0x1A86, 0x5523)}


def esptool(port: str, *args: str, capture: bool = False) -> str:
    cmd = [
        sys.executable,
        "-m",
        "esptool",
        "--chip",
        "esp8266",
        "--port",
        port,
        "--baud",
        str(BAUD),
    ]
    cmd += list(args)
    print("$ esptool " + " ".join(cmd[3:]), flush=True)
    if capture:
        result = subprocess.run(cmd, capture_output=True, text=True)
        sys.stdout.write(result.stdout)
        if result.returncode != 0:
            sys.stderr.write(result.stderr)
            raise SystemExit(f"esptool failed ({result.returncode})")
        return result.stdout
    subprocess.run(cmd, check=True)
    return ""


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def cmd_ports(_: argparse.Namespace) -> int:
    from serial.tools import list_ports

    found = False
    for p in list_ports.comports():
        tag = "  <- CH340 (GBS-C USB)" if (p.vid, p.pid) in CH340_IDS else ""
        print(f"{p.device:8s} {p.description}{tag}")
        found = found or bool(tag)
    if not found:
        print("no CH340 found: connect the board's micro-USB with a data cable")
    return 0


def cmd_backup(args: argparse.Namespace) -> int:
    info = esptool(args.port, "flash-id", capture=True)
    mac = re.search(r"MAC:\s*([0-9a-f:]{17})", info, re.I)
    tag = mac.group(1).replace(":", "") if mac else "unknown"
    BACKUPS.mkdir(exist_ok=True)
    stamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    first = BACKUPS / f"{stamp}-{tag}.flash.bin"
    second = BACKUPS / f"{stamp}-{tag}.verify.bin"
    esptool(args.port, "read-flash", "0", hex(FLASH_SIZE), str(first))
    esptool(args.port, "read-flash", "0", hex(FLASH_SIZE), str(second))
    h1, h2 = sha256(first), sha256(second)
    second.unlink()
    if h1 != h2:
        first.unlink()
        raise SystemExit("the two reads differ: unreliable connection, nothing kept — retry")
    first.with_suffix(".bin.sha256").write_text(f"{h1}  {first.name}\n", encoding="utf-8")
    print(f"backup: {first} ({first.stat().st_size} bytes) sha256 {h1}")
    return 0


def cmd_flash(args: argparse.Namespace) -> int:
    image = Path(args.image)
    if not image.is_file():
        raise SystemExit(f"no such image: {image}")
    print(f"will write {image} ({image.stat().st_size} bytes, sha256 {sha256(image)[:16]}…) at 0x0")
    extra = ["--erase-all"] if args.erase_all else []
    esptool(args.port, "write-flash", *extra, "0x0", str(image))
    return 0


def cmd_restore(args: argparse.Namespace) -> int:
    image = Path(args.image)
    sums = image.with_suffix(".bin.sha256")
    if not sums.is_file():
        raise SystemExit(f"missing checksum file {sums}")
    expected = sums.read_text(encoding="utf-8").split()[0]
    if sha256(image) != expected:
        raise SystemExit("image does not match its checksum: refusing to restore")
    if image.stat().st_size != FLASH_SIZE:
        raise SystemExit(f"expected a full {FLASH_SIZE}-byte image")
    print(f"will restore the full flash from {image}")
    esptool(args.port, "write-flash", "0x0", str(image))
    return 0


def cmd_console(args: argparse.Namespace) -> int:
    import serial

    # Opening the port does not reset this board (bench, 2026-10-13); a flash does. `--wait` gives
    # a freshly flashed or rebooted firmware time to come up before the first line is sent, and
    # every line after it waits `--gap` seconds (one value for all lines).
    with serial.Serial(args.port, 115200, timeout=0.2) as ser:
        start = time.monotonic()
        pending = list(args.send or [])
        deadline = start + args.seconds
        while time.monotonic() < deadline:
            if pending and time.monotonic() - start >= args.wait:
                ser.write((pending.pop(0) + "\r\n").encode("ascii"))
                start = time.monotonic() - args.wait + args.gap
            chunk = ser.read(512)
            if chunk:
                sys.stdout.write(chunk.decode("utf-8", errors="replace"))
                sys.stdout.flush()
    return 0


def main() -> int:
    sys.stdout.reconfigure(encoding="utf-8")
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("ports").set_defaults(fn=cmd_ports)
    p = sub.add_parser("backup")
    p.add_argument("--port", required=True)
    p.set_defaults(fn=cmd_backup)
    p = sub.add_parser("flash")
    p.add_argument("--port", required=True)
    p.add_argument("--image", required=True)
    p.add_argument("--erase-all", action="store_true", help="also wipe settings and Wi-Fi")
    p.set_defaults(fn=cmd_flash)
    p = sub.add_parser("restore")
    p.add_argument("--port", required=True)
    p.add_argument("--image", required=True)
    p.set_defaults(fn=cmd_restore)
    p = sub.add_parser("console")
    p.add_argument("--port", required=True)
    p.add_argument("--send", action="append", help="a line to send; repeat for several")
    p.add_argument("--wait", type=float, default=1.0, help="seconds after opening before sending")
    p.add_argument("--gap", type=float, default=1.0, help="seconds between sent lines")
    p.add_argument("--seconds", type=float, default=4.0)
    p.set_defaults(fn=cmd_console)
    args = ap.parse_args()
    return args.fn(args)


if __name__ == "__main__":
    raise SystemExit(main())
