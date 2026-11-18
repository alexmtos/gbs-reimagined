---
name: release
description: Cuts a GBS Reimagined release through release-please — checks the gates, reviews the release PR's changelog, merges it, and verifies the published binaries, checksums and installer. Use only when the project owner asks for a release.
disable-model-invocation: true
---

1. Confirm the milestone's bench gate passed (`docs/bench/<milestone>.md`) and `main` is green.
2. Find the open release PR (`gh pr list --label "autorelease: pending"`). Review its CHANGELOG
   entries: user-facing wording, nothing internal leaking, breaking changes called out. Rewrite
   them for users in `CHANGELOG.md` on the release branch and in the PR body (the owner's
   standing practice). The CI run release-please's own push triggers fails at once with no jobs:
   GitHub does not run other workflows for events made with the default `GITHUB_TOKEN`
   (release-please-action's README). That is expected
   (owner's decision, 2026-11-18, not to add a token): the notes commit, pushed by a person, runs
   the CI, and the release workflow runs it again before publishing.
3. Ask the owner to approve the version number and the notes; merge only with their go-ahead
   (`gh pr merge <n> --squash`).
4. Watch the release workflow (`gh run watch`). When it finishes, check the release has the
   firmware `.bin`, the OTA `.ota.bin.gz` and `SHA256SUMS`, and that the sums match the files.
5. Installer site (`installer/README.md`, "Publishing"): the release workflow's `site` job
   publishes it when the repository is public. While it is private the job is skipped. After the
   owner makes the repository public, set Settings, Pages, Source to *GitHub Actions* once, then
   run the *pages* workflow by hand (`gh workflow run pages.yml`). Open
   `https://alexmtos.github.io/gbs-reimagined/` and confirm:
   - the page offers the new version;
   - `manifest.json` names it;
   - `latest.json` names it and the update file's SHA-256 matches `SHA256SUMS`.
6. Flash the released binary on the bench board with the `flash-board` skill and confirm the
   banner shows the release version.
