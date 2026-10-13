# 0012. Distribution: browser installer, browser-assisted OTA, release-please, Renovate

- Status: accepted
- Date: 2026-10-13

## Context

Users of scalers are not firmware developers: installing and updating must not need a toolchain.
ESP Web Tools 10.4.0 flashes an ESP8266 from Chrome/Edge over Web Serial and can provision Wi-Fi
through Improv. GitHub release assets are served without CORS headers, so a browser cannot fetch
them directly; GitHub Pages serves `Access-Control-Allow-Origin: *`. The ESP8266 has no A/B
partitions: OTA stages the image and the bootloader copies it, and it can decompress gzip images
(−28 % transfer). TLS on the device is too heavy for its heap
([tech stack §2.5, §2.6, §2.11](../research/tech-stack.md)).

## Decision

- **Installer:** a static page with ESP Web Tools 10.4.0 and one manifest per channel (stable,
  beta), built by CI from the release binaries. It is published on GitHub Pages when the
  repository allows it, and can always be served locally (`pnpm -C installer serve`; Web Serial
  works on `localhost`).
- **OTA:** the device's own web page uploads a gzip image to `/api/update` (handled in the loop).
  "Check for updates" is done by the browser: it fetches `latest.json` and the image from the
  installer site, verifies the SHA-256, then posts it to the device. The device validates size, image
  magic and a board id before committing. Signing is supported by the core but **not enforced** in
  v1, so community builds stay installable; it can be enabled per build later.
- **Releases:** Conventional Commits; release-please opens release PRs and writes the changelog;
  the release workflow builds, attaches binaries and SHA-256 sums, and rebuilds the installer site.
- **Dependencies:** Renovate (github-actions, npm, uv) plus a custom regex manager and datasource for
  `platformio.ini`; actions pinned by digest.

## Consequences

Anyone with Chrome or Edge can install and update without tools. Pages hosting needs either a
public repository or a paid plan for a private one; until then the installer runs locally and
releases are downloaded manually. A power loss during the bootloader's copy may require a USB
re-install, which the installer makes easy.
