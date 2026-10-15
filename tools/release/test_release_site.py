"""Which releases the installer site offers, and the checksum check before staging them."""

import hashlib
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).parent))
from release_site import check_sums, pick_channels, version_key  # noqa: E402


def rel(tag: str, pre: bool = False, draft: bool = False) -> dict:
    return {"tagName": tag, "isPrerelease": pre, "isDraft": draft}


def test_versions_order_as_semver():
    tags = ["v1.0.0", "v1.0.0-beta.2", "v1.0.0-beta.10", "v0.9.9", "v1.0.0-alpha", "v1.0.1"]
    assert sorted(tags, key=version_key) == [
        "v0.9.9",
        "v1.0.0-alpha",
        "v1.0.0-beta.2",
        "v1.0.0-beta.10",
        "v1.0.0",
        "v1.0.1",
    ]
    with pytest.raises(ValueError):
        version_key("nightly")


def test_stable_is_the_newest_release_and_beta_only_when_above_it():
    assert pick_channels([rel("v1.0.0"), rel("v1.1.0"), rel("v1.0.5")]) == {"stable": "v1.1.0"}
    assert pick_channels([rel("v1.0.0"), rel("v1.1.0-beta.1", pre=True)]) == {
        "stable": "v1.0.0",
        "beta": "v1.1.0-beta.1",
    }
    assert pick_channels([rel("v1.1.0"), rel("v1.1.0-beta.3", pre=True)]) == {"stable": "v1.1.0"}
    assert pick_channels([rel("v0.2.0-beta.1", pre=True)]) == {"beta": "v0.2.0-beta.1"}


def test_drafts_and_foreign_tags_are_ignored():
    assert pick_channels([rel("v2.0.0", draft=True), rel("tools-1"), rel("v1.0.0")]) == {
        "stable": "v1.0.0"
    }
    assert pick_channels([]) == {}


def test_files_must_match_their_sums(tmp_path: Path):
    a = tmp_path / "gbs-reimagined-v1.0.0-gbsc.bin"
    b = tmp_path / "gbs-reimagined-v1.0.0-gbsc.ota.bin.gz"
    a.write_bytes(b"image")
    b.write_bytes(b"update")
    sums = "".join(f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n" for p in (a, b))
    check_sums(sums, [a, b])
    b.write_bytes(b"tampered")
    with pytest.raises(ValueError, match="does not match"):
        check_sums(sums, [a, b])
    with pytest.raises(ValueError, match="not in SHA256SUMS"):
        check_sums(sums.splitlines()[0], [a, b])
