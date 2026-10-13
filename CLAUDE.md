# GBS Reimagined

Firmware for the RetroScaler GBS-C video scaler (Tvia TV5725 + Si5351 + ESP8266 + SSD1306 OLED +
rotary encoder + MS9288A HDMI converter), controlled from a web UI, the serial console and the
OLED. Written from scratch; only factual chip data comes from gbs-control (NOTICE.md, ADR 0002).

## Where things are

- `firmware/` — C++20 on the ESP8266 Arduino core via PlatformIO. Read `firmware/CLAUDE.md`.
- `web/` — Preact + TypeScript UI, embedded into the firmware image. Read `web/CLAUDE.md`.
- `schema/` — single sources of truth: registers, baseline programs, settings, translations.
- `tools/` — Python (uv): code generation, bench verification, board helpers, size budgets.
  Read `tools/CLAUDE.md`.
- `docs/` — `research/` (evidence), `adr/` (decisions), `design/` (architecture, video engine, UX,
  memory budget), `hardware/`, `user-guide/{en,pt-BR}/`, `reference/` (generated), `roadmap.md`.

Before changing a subsystem, read its design document; before reversing a decision, read its ADR.

## Commands

```sh
uv run tools/codegen/generate.py            # regenerate everything derived from schema/
uv run tools/codegen/generate.py --check    # CI: fail if generated files are stale
cd firmware && pio test -e native           # host tests (doctest, fake TV5725/Si5351)
cd firmware && pio run -e gbsc              # firmware for the board (writes size.json)
uv run tools/size/check_budget.py --size firmware/.pio/build/gbsc/size.json
pnpm -C web check                           # typecheck + lint + test + build the UI
```

Run the generator, the host tests and the web checks for whatever you changed before calling a
task done, and report their results.

## Rules that matter everywhere

- **Never edit generated files** (`*.gen.*`, `docs/reference/*`): edit `schema/` and regenerate.
  A hook blocks the edit and says which source to change.
- **Measured facts beat assumptions.** Any number about the chip or the board names its source
  (datasheet page, bench run, or a cited research note). Reference-project measurements are
  hypotheses until reproduced on this bench (`docs/research/README.md`).
- **The user decides scope.** The project owner asked to be consulted on doubts, improvements
  and corrections: ask before adding features, dependencies or changing the hardware assumptions.
- **Languages:** code, comments, technical docs, CLAUDE.md, skills and commit messages are in
  English. The user guide is maintained in English and Brazilian Portuguese together. The UI is
  English by default with pt-BR; the serial console is English only.
- **Commits:** Conventional Commits (`feat(video): …`, `fix(web): …`, `docs: …`); one logical change
  per commit. Releases are cut by release-please.
- **Flashing the real board** is a bench action: back up the flash first (`flash-board` skill)
  and say what will be written before writing it.

## Skills in this repository

`build-and-test`, `regenerate-code`, `add-setting`, `flash-board`, `bench-verify`,
`investigate-register`, `write-adr`, `release`, `update-user-guide`. Use them instead of
improvising those procedures.
