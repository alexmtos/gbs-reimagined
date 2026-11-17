# 0016. Update dependencies by hand, without Renovate

- Status: accepted
- Date: 2026-11-17

## Context

[ADR 0012](0012-distribution-and-releases.md) chose Renovate to propose dependency updates
(github-actions, npm, uv and the PlatformIO libraries through a custom manager). The configuration
was committed (`renovate.json`), but the Renovate app was never installed for the repository. From
the 1.0.0 release (2026-10-15) to 2026-11-17 it opened no pull request and no dependency dashboard.
The project owner decided not to use it (2026-11-17).

Every dependency is pinned:

- `firmware/platformio.ini`: platform and libraries, exact versions;
- `web/package.json` and `installer/package.json`, exact versions with their lockfiles;
- `uv.lock`;
- the GitHub Actions in `.github/workflows/`, by version tag (`@v7`, `@v10.3.0`).

CI builds and tests everything on every pull request.

## Decision

- No automated dependency updates. `renovate.json` is removed.
- Dependencies change only in a pull request the owner asks for, one dependency or one toolchain
  group at a time. The commit message cites the release notes read.
- An update of ESP Web Tools needs a bench check: the installer must flash a board and give it Wi-Fi
  over Improv (`installer/README.md`). It changes how boards are installed.
- An update of the ESP8266 platform or core needs the firmware gate of the milestone it touches: a
  bench run with the capture card. It changes the code that runs the scaler.
- Everything else of ADR 0012 stands.

## Consequences

- Nothing changes under the project's feet between releases, and no bot pull requests need review.
- Security fixes in dependencies are not proposed automatically. They have to be looked for: before
  each release, read the advisories for the pinned versions (`pnpm audit`, `uv pip list --outdated`,
  and the release notes of the ESP8266 core, ESP Web Tools and ArduinoJson).
- ADR 0009 counted on Renovate's grouping to soften the early-adopter risk of its toolchain. That
  risk now rests on CI and on deliberate updates.
