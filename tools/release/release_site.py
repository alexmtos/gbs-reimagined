"""Stage the installer site from the published GitHub releases (the pages workflow, ADR 0012).

    uv run tools/release/release_site.py [--repo alexmtos/gbs-reimagined] [--out installer/public]

Stable is the newest release; beta is the newest prerelease above it, when there is one. For each,
downloads the image, the update file and SHA256SUMS with the GitHub CLI (`gh`, authenticated by
GH_TOKEN in CI), checks the sums and stages them with installer_site.py. With no release yet,
stages nothing: the page then says it has no firmware to install.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from installer_site import BOARD, MANIFESTS, ROOT, stage  # noqa: E402

VERSION_RE = re.compile(r"^v?(\d+)\.(\d+)\.(\d+)(?:-([0-9A-Za-z.-]+))?(?:\+.*)?$")


def version_key(tag: str) -> tuple:
    """Orders versions as SemVer does (web/src/update/version.ts): a prerelease comes before its
    release; numeric identifiers sort below text ones."""
    m = VERSION_RE.match(tag.strip())
    if not m:
        raise ValueError(f"not a version: {tag!r}")
    core = tuple(int(m.group(i)) for i in (1, 2, 3))
    if not m.group(4):
        return (core, 1, ())
    pre = tuple((0, int(p), "") if p.isdigit() else (1, 0, p) for p in m.group(4).split("."))
    return (core, 0, pre)


def pick_channels(releases: list[dict]) -> dict[str, str]:
    """The tag to stage for each channel, from `gh release list --json ...` entries."""
    usable = [r for r in releases if not r.get("isDraft") and VERSION_RE.match(r["tagName"])]
    stable = [r["tagName"] for r in usable if not r.get("isPrerelease")]
    beta = [r["tagName"] for r in usable if r.get("isPrerelease")]
    picked: dict[str, str] = {}
    if stable:
        picked["stable"] = max(stable, key=version_key)
    if beta:
        newest = max(beta, key=version_key)
        if "stable" not in picked or version_key(newest) > version_key(picked["stable"]):
            picked["beta"] = newest
    return picked


def check_sums(sums: str, files: list[Path]) -> None:
    """Raises unless every file is listed in SHA256SUMS with its hash."""
    listed = {}
    for line in sums.splitlines():
        parts = line.split()
        if len(parts) == 2:
            listed[parts[1].lstrip("*")] = parts[0].lower()
    for path in files:
        want = listed.get(path.name)
        if want is None:
            raise ValueError(f"{path.name}: not in SHA256SUMS")
        if hashlib.sha256(path.read_bytes()).hexdigest() != want:
            raise ValueError(f"{path.name}: does not match SHA256SUMS")


def gh(*args: str) -> str:
    return subprocess.run(["gh", *args], check=True, capture_output=True, text=True).stdout


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--repo", default="alexmtos/gbs-reimagined")
    ap.add_argument("--out", type=Path, default=ROOT / "installer/public")
    args = ap.parse_args()
    sys.stdout.reconfigure(encoding="utf-8")

    releases = json.loads(
        gh("release", "list", "--repo", args.repo, "--limit", "100",
           "--json", "tagName,isPrerelease,isDraft")
    )  # fmt: skip
    picked = pick_channels(releases)
    for name in [*MANIFESTS.values(), "latest.json", "firmware"]:
        path = args.out / name
        if path.is_dir():
            shutil.rmtree(path)
        elif path.exists():
            path.unlink()
    if not picked:
        print("no release yet: nothing staged")
        return 0
    args.out.mkdir(parents=True, exist_ok=True)
    for channel, tag in picked.items():
        with tempfile.TemporaryDirectory() as tmp:
            gh("release", "download", tag, "--repo", args.repo, "--dir", tmp,
               "--pattern", f"gbs-reimagined-*-{BOARD}.bin",
               "--pattern", f"gbs-reimagined-*-{BOARD}.ota.bin.gz",
               "--pattern", "SHA256SUMS")  # fmt: skip
            folder = Path(tmp)
            image = next(folder.glob(f"gbs-reimagined-*-{BOARD}.bin"), None)
            ota = next(folder.glob(f"gbs-reimagined-*-{BOARD}.ota.bin.gz"), None)
            if image is None or ota is None:
                print(f"error: {tag} lacks the {BOARD} image or update file")
                return 1
            try:
                check_sums((folder / "SHA256SUMS").read_text(encoding="utf-8"), [image, ota])
                url = json.loads(gh("release", "view", tag, "--repo", args.repo, "--json", "url"))
                stage(
                    args.out,
                    channel,
                    tag.removeprefix("v"),
                    image.read_bytes(),
                    ota.read_bytes(),
                    url["url"],
                )
            except (ValueError, FileNotFoundError) as e:
                print(f"error: {tag}: {e}")
                return 1
        print(f"{channel}: {tag}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
