---
name: release
description: Cuts a GBS Reimagined release through release-please — checks the gates, reviews the release PR's changelog, merges it, and verifies the published binaries, checksums and installer. Use only when the project owner asks for a release.
disable-model-invocation: true
---

1. Confirm the milestone's bench gate passed (`docs/bench/<milestone>.md`) and `main` is green.
2. Find the open release PR (`gh pr list --label "autorelease: pending"`). Review its CHANGELOG
   entries: user-facing wording, nothing internal leaking, breaking changes called out.
3. Ask the owner to approve the version number and the notes; merge only with their go-ahead
   (`gh pr merge <n> --squash`).
4. Watch the release workflow (`gh run watch`). When it finishes, check the release has the
   firmware `.bin`, the OTA `.ota.bin.gz` and `SHA256SUMS`, and that the sums match the files.
5. If the installer site is published, open it and confirm the manifest lists the new version.
6. Flash the released binary on the bench board with the `flash-board` skill and confirm the
   banner shows the release version.
