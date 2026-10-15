"""Board helpers over USB (CH340): list ports, back up, flash, restore, talk to the console.

    uv run --group board tools/board/board.py ports
    uv run --group board tools/board/board.py backup  --port COM3
    uv run --group board tools/board/board.py flash   --port COM3 --image <firmware.bin>
    uv run --group board tools/board/board.py restore --port COM3 --image backups/<file>.flash.bin
    uv run --group board tools/board/board.py console --port COM3 [--send status] [--seconds 5]
    uv run --group board tools/board/board.py improv  --port COM3 [--scan]

Writes to the board only in `flash` and `restore`, and only after printing what will be written.
`improv` asks what the browser installer asks over Improv Wi-Fi Serial (state, device information,
networks) and never sends Wi-Fi settings: the password goes from the browser to the board only.
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


IMPROV_TYPES = {1: "state", 2: "error", 3: "rpc", 4: "result"}
IMPROV_STATES = {
    0: "stopped",
    1: "awaiting authorization",
    2: "ready",
    3: "provisioning",
    4: "provisioned",
}
IMPROV_ERRORS = {
    0: "none",
    1: "invalid packet",
    2: "unknown command",
    3: "unable to connect",
    0xFF: "unknown",
}


def improv_packet(kind: int, data: bytes) -> bytes:
    """A packet as the installer's client writes it, newline included."""
    body = b"IMPROV" + bytes([1, kind, len(data)]) + data
    return body + bytes([sum(body) & 0xFF]) + b"\n"


def improv_read(buf: bytearray) -> tuple[list[tuple[int, bytes]], list[str]]:
    """Takes complete packets and complete text lines out of `buf`, as the client finds them: a
    packet only where a line starts."""
    packets: list[tuple[int, bytes]] = []
    lines: list[str] = []
    while buf:
        if buf.startswith(b"IMPROV"):
            if len(buf) < 9 or len(buf) < 10 + buf[8]:
                break
            n = buf[8]
            if sum(buf[: 9 + n]) & 0xFF != buf[9 + n]:
                lines.append("(Improv packet with a bad checksum)")
            packets.append((buf[7], bytes(buf[9 : 9 + n])))
            del buf[: 10 + n]
            continue
        end = buf.find(b"\n")
        if end < 0:
            break
        lines.append(buf[:end].decode("utf-8", errors="replace").rstrip("\r"))
        del buf[: end + 1]
    return packets, lines


def improv_strings(data: bytes) -> list[str]:
    out, at = [], 2
    while at < 2 + data[1]:
        out.append(data[at + 1 : at + 1 + data[at]].decode("utf-8", errors="replace"))
        at += 1 + data[at]
    return out


def cmd_improv(args: argparse.Namespace) -> int:
    import serial

    commands = [(2, "current state"), (3, "device information")]
    if args.scan:
        commands.append((4, "networks"))
    with serial.Serial(args.port, 115200, timeout=0.1) as ser:
        buf = bytearray()
        for command, what in commands:
            print(f"> {what}")
            sent = time.monotonic()
            ser.write(improv_packet(3, bytes([command, 0])))
            answered = False
            while time.monotonic() - sent < args.seconds and not answered:
                buf += ser.read(512)
                packets, lines = improv_read(buf)
                for line in lines:
                    if line:
                        print(f"  console: {line}")
                for kind, data in packets:
                    ms = (time.monotonic() - sent) * 1000
                    if kind == 1:
                        print(f"  {ms:5.0f} ms state: {IMPROV_STATES.get(data[0], data[0])}")
                        answered = answered or (command == 2 and data[0] != 4)
                    elif kind == 2:
                        print(f"  {ms:5.0f} ms error: {IMPROV_ERRORS.get(data[0], data[0])}")
                        answered = answered or data[0] != 0
                    elif kind == 4:
                        strings = improv_strings(data)
                        if command == 4 and strings:
                            # SSIDs are the neighbours' business: show signal and security only.
                            print(f"  {ms:5.0f} ms network: {strings[1]} dBm, secured {strings[2]}")
                        else:
                            print(f"  {ms:5.0f} ms result {data[0]}: {strings}")
                            answered = True
                    else:
                        print(f"  {ms:5.0f} ms {IMPROV_TYPES.get(kind, kind)}: {data.hex()}")
            if not answered:
                print(f"  no answer in {args.seconds:.0f} s")
                return 1
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
    p = sub.add_parser("improv")
    p.add_argument("--port", required=True)
    p.add_argument("--scan", action="store_true", help="also list the networks the board hears")
    p.add_argument("--seconds", type=float, default=15.0, help="longest wait for each answer")
    p.set_defaults(fn=cmd_improv)
    args = ap.parse_args()
    return args.fn(args)


if __name__ == "__main__":
    raise SystemExit(main())
