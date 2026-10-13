---
name: build-and-test
description: Builds and tests every part of GBS Reimagined — generated-file freshness, firmware host tests, the board firmware build with its memory budget, and the web UI typecheck, lint, tests and build. Use before declaring any change done, when asked whether the project builds, or when CI fails and the failure must be reproduced locally.
---

Run the steps in order from the repository root and stop at the first failure; fix it, then rerun
from that step. Report each step's result (pass/fail and the key numbers) in your answer.

1. Generated files are fresh:
   `uv run tools/codegen/generate.py --check`
   If it fails, run the `regenerate-code` skill, then continue.
2. Firmware host tests:
   `cd firmware && pio test -e native`
3. Board firmware and memory budget:
   `cd firmware && pio run -e gbsc`
   `uv run tools/size/check_budget.py --size firmware/.pio/build/gbsc/size.json`
   Quote static RAM, IRAM and flash from the table. If static RAM grew by more than 512 bytes
   against the previous build, say why and how it is paid back (`docs/design/memory-budget.md`).
4. Web UI:
   `pnpm -C web install --frozen-lockfile` (only if `node_modules` is missing or the lockfile
   changed), then `pnpm -C web check`. Quote the gzip size the build prints (budget 64 KiB).
5. Python tooling, when `tools/` changed:
   `uv run ruff check tools` and `uv run pytest`

Warnings from the Arduino core's own sources are expected noise; warnings from `firmware/src`
are not — fix them.
