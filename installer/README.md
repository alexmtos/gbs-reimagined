# Browser installer

For developers and maintainers. The page that installs GBS Reimagined from Chrome or Edge over Web
Serial and hands the board its Wi-Fi network through Improv (ADR 0011, ADR 0012). Users follow the
steps on the page itself.

## What is in it

- `index.html`, `src/` — the page, built by `src/main.ts`. Its text comes from `schema/i18n/*.yaml`
  (`installer:`), generated into `src/gen/i18n/` by `uv run tools/codegen/generate.py`.
- `esp-web-tools` 10.4.0 from npm, imported as the bundle the project publishes for the web
  (`dist/web/install-button.js`), which carries its own dependencies. Its plain modules import a
  file Material Web 2.5 no longer has, so they do not build.
- `public/` — the firmware to install, staged by `tools/release/installer_site.py` and never
  committed:
  - `firmware/gbs-reimagined-<version>-gbsc.bin`, the full image, flashed at offset 0;
  - `firmware/gbs-reimagined-<version>-gbsc.ota.bin.gz`, the update file the board's own page
    installs;
  - `manifest.json` (stable) and `manifest-beta.json`, one ESP Web Tools manifest per channel;
  - `latest.json`, what the board's page reads to offer an update (`web/src/update/update.ts`).

## Commands

```sh
pnpm install
uv run tools/release/installer_site.py --build   # stage this machine's firmware build
pnpm serve                   # build, then serve on http://localhost:4173
pnpm check                   # typecheck + lint + test + build (what CI runs)
```

Web Serial needs a secure context: `localhost` is one, a LAN address is not. Close every other
program that holds the board's port first (PlatformIO monitor, `tools/board/board.py console`).

`installer_site.py --version <v>` stages the build under another version. A manifest version that
differs from the board's makes the installer offer *Update GBS Reimagined*, which is how an
install over USB is tested without erasing the board.

## How the installer and the board talk

- Opening the port does not reset a running board ([bench M3](../docs/bench/M3.md),
  2026-10-15); right after a flash, opening it can reset the board once more
  ([bench M2](../docs/bench/M2.md)).
- After flashing, ESP Web Tools resets the board and asks for its state over Improv once a second
  for `new_install_improv_wait_time` seconds (20 in our manifests; 10 by default). The board
  answers 2.45 s after a reset, or once it has joined a saved network: 7.7 s on the bench
  ([bench M3](../docs/bench/M3.md)). By design it answers within about 10.5 s: 2.45 s plus the
  8 s it holds the state while joining (`kStateHoldMs`).
- On a board already running, the installer waits only 1.5 s for an answer (ESP Web Tools 10.4.0
  source, `install-dialog.ts`).
- The installer tells an update from another firmware by comparing the name and version the
  board reports (`GBS Reimagined`, `GBSR_VERSION`) with the manifest's `name` and `version`.
- The Wi-Fi password typed in the dialog goes from the browser to the board only.

## Publishing

`.github/workflows/pages.yml` builds the site and publishes it on GitHub Pages, at
`https://alexmtos.github.io/gbs-reimagined/` (`UPDATE_SITE` in `web/src/update/update.ts`):

- the page from the commit it runs on;
- `tools/release/release_site.py` stages the firmware of the newest release (stable) and of the
  newest prerelease above it (beta), downloaded with `gh` and checked against the release's
  `SHA256SUMS`.

The release workflow runs it after attaching the binaries to a release, unless the repository is
private (Pages needs a public repository on this plan). Run it by hand (Actions, *pages*, *Run
workflow*) after making the repository public, or to rebuild the site. The first time, set
Settings, Pages, *Build and deployment*, Source to *GitHub Actions*.
