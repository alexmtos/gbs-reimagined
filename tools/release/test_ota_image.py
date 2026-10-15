"""The update file is a standard gzip member that decompresses to the image, with our comment."""

import gzip
import sys
import zlib
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).parent))
from ota_image import ota_image  # noqa: E402


def test_decompresses_to_the_image_and_carries_the_comment():
    image = bytes([0xE9]) + bytes(range(256)) * 40
    data = ota_image(image, "gbsc", "1.2.3", "gbs-reimagined-1.2.3-gbsc.bin")
    assert gzip.decompress(data) == image
    # FNAME then FCOMMENT after the 10-byte fixed header.
    assert data[:4] == bytes([0x1F, 0x8B, 8, 0x18])
    rest = data[10:]
    name, rest = rest.split(b"\0", 1)
    comment, _ = rest.split(b"\0", 1)
    assert name == b"gbs-reimagined-1.2.3-gbsc.bin"
    assert comment == b"gbsr board=gbsc version=1.2.3"
    # Deterministic: no timestamp.
    assert data[4:8] == b"\0\0\0\0"
    assert data == ota_image(image, "gbsc", "1.2.3", "gbs-reimagined-1.2.3-gbsc.bin")
    # The raw deflate stream ends where the trailer begins.
    assert zlib.crc32(image) == int.from_bytes(data[-8:-4], "little")


def test_refuses_text_that_would_break_the_header():
    with pytest.raises(ValueError):
        ota_image(b"\xe9", "gb sc", "1.0", "x.bin")
