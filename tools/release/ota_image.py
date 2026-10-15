"""Write the firmware update file: the image gzip-compressed, its header naming board and version.

    uv run tools/release/ota_image.py firmware/.pio/build/gbsc/firmware.bin \
        --version 1.0.0 --board gbsc --out gbs-reimagined-1.0.0-gbsc.ota.bin.gz

The header comment reads "gbsr board=<board> version=<version>": the board checks it before
writing anything (firmware/src/core/update_check.*, ADR 0012), and the ESP8266 bootloader skips it
when it decompresses the image. The output is deterministic (no timestamp) so release builds
reproduce.
"""

from __future__ import annotations

import argparse
import struct
import sys
import zlib
from pathlib import Path

FLAG_NAME = 0x08
FLAG_COMMENT = 0x10
OS_UNKNOWN = 255
XFL_MAX_COMPRESSION = 2


def ota_image(image: bytes, board: str, version: str, name: str) -> bytes:
    """The gzip member (RFC 1952) for `image`, with a file name and our comment."""
    for text in (board, version, name):
        if not text.isascii() or "\0" in text or " " in text:
            raise ValueError(f"not usable in the header: {text!r}")
    header = struct.pack(
        "<BBBBIBB", 0x1F, 0x8B, 8, FLAG_NAME | FLAG_COMMENT, 0, XFL_MAX_COMPRESSION, OS_UNKNOWN
    )
    header += name.encode("ascii") + b"\0"
    header += f"gbsr board={board} version={version}".encode("ascii") + b"\0"
    deflate = zlib.compressobj(9, zlib.DEFLATED, -15)
    body = deflate.compress(image) + deflate.flush()
    trailer = struct.pack("<II", zlib.crc32(image) & 0xFFFFFFFF, len(image) & 0xFFFFFFFF)
    return header + body + trailer


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("image", type=Path, help="firmware.bin from the build")
    ap.add_argument("--version", required=True)
    ap.add_argument("--board", default="gbsc")
    ap.add_argument("--out", type=Path, required=True)
    args = ap.parse_args()
    sys.stdout.reconfigure(encoding="utf-8")

    image = args.image.read_bytes()
    if not image or image[0] != 0xE9:
        print(f"{args.image}: not an ESP8266 image (first byte {image[:1].hex() or 'none'})")
        return 1
    name = f"gbs-reimagined-{args.version}-{args.board}.bin"
    data = ota_image(image, args.board, args.version, name)
    args.out.write_bytes(data)
    print(f"{args.out}: {len(data)} bytes from {len(image)} ({len(data) * 100 // len(image)} %)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
