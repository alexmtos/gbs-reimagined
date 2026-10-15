"""The installer site's manifests and latest.json, as ESP Web Tools and the web UI read them."""

import hashlib
import json
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).parent))
from installer_site import stage  # noqa: E402

IMAGE = bytes([0xE9]) + bytes(range(256)) * 4


def ota(tag: bytes) -> bytes:
    return b"\x1f\x8b" + tag


def read(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def test_a_stable_release_gives_the_manifest_and_latest_json(tmp_path: Path):
    stage(tmp_path, "stable", "1.0.0", IMAGE, ota(b"one"), "https://example.org/v1.0.0")
    m = read(tmp_path / "manifest.json")
    assert m["name"] == "GBS Reimagined"
    assert m["version"] == "1.0.0"
    assert m["new_install_prompt_erase"] is True
    assert m["builds"] == [
        {
            "chipFamily": "ESP8266",
            "parts": [{"path": "firmware/gbs-reimagined-1.0.0-gbsc.bin", "offset": 0}],
        }
    ]
    assert (tmp_path / "firmware/gbs-reimagined-1.0.0-gbsc.bin").read_bytes() == IMAGE
    latest = read(tmp_path / "latest.json")
    assert latest == {
        "format": "gbsr-latest",
        "version": 1,
        "stable": {
            "version": "1.0.0",
            "notes": "https://example.org/v1.0.0",
            "files": {
                "gbsc": {
                    "ota": "firmware/gbs-reimagined-1.0.0-gbsc.ota.bin.gz",
                    "sha256": hashlib.sha256(ota(b"one")).hexdigest(),
                    "size": len(ota(b"one")),
                }
            },
        },
    }


def test_channels_are_kept_apart_and_old_files_go(tmp_path: Path):
    stage(tmp_path, "stable", "1.0.0", IMAGE, ota(b"one"), None)
    stage(tmp_path, "beta", "1.1.0-beta.1", IMAGE, ota(b"two"), None)
    latest = read(tmp_path / "latest.json")
    assert latest["stable"]["version"] == "1.0.0"
    assert latest["beta"]["version"] == "1.1.0-beta.1"
    assert read(tmp_path / "manifest-beta.json")["version"] == "1.1.0-beta.1"
    stage(tmp_path, "stable", "1.1.0", IMAGE, ota(b"three"), None)
    names = sorted(p.name for p in (tmp_path / "firmware").iterdir())
    assert names == [
        "gbs-reimagined-1.1.0-beta.1-gbsc.bin",
        "gbs-reimagined-1.1.0-beta.1-gbsc.ota.bin.gz",
        "gbs-reimagined-1.1.0-gbsc.bin",
        "gbs-reimagined-1.1.0-gbsc.ota.bin.gz",
    ]
    assert "notes" not in read(tmp_path / "latest.json")["stable"]


@pytest.mark.parametrize(
    ("version", "image", "update"),
    [
        ("1.0.0 beta", IMAGE, ota(b"x")),
        ("1.0.0", b"\x00" + IMAGE, ota(b"x")),
        ("1.0.0", IMAGE, b"PK\x03\x04"),
    ],
)
def test_unusable_inputs_are_refused(tmp_path: Path, version: str, image: bytes, update: bytes):
    with pytest.raises(ValueError):
        stage(tmp_path, "stable", version, image, update, None)
    assert not (tmp_path / "manifest.json").exists()
