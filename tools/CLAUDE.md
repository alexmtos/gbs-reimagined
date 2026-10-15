# Tools

Python 3.12 managed by uv from the root `pyproject.toml`; run everything as
`uv run tools/<path>.py` from the repository root.

| Directory | Needs | Purpose |
|---|---|---|
| `codegen/` | nothing | `generate.py` (all derived files, `--check` for CI), one-time importers with provenance |
| `size/` | a firmware build | `pio_size_report.py` (PlatformIO post-script), `check_budget.py` |
| `board/` | the board on USB | flash backup/restore and upload helpers (esptool), Improv probe |
| `release/` | a build, or `gh` for releases | update file (`ota_image.py`), installer site staging from a build (`installer_site.py`) or from the releases (`release_site.py`) |
| `bench/` | board + capture card | picture verification on the NZXT Signal HD60 |
| `datasheets.py` | network | fetch and verify the vendor PDFs |

## Rules

- `import_tv5725.py` and `import_programs.py` are one-time importers: run them only when the
  upstream source changes, and record the new provenance in NOTICE.md.
- Scripts that touch the board print what they will do and require an explicit flag for anything
  that writes flash or registers.
- Bench scripts separate "the capture card delivered no frames" (rig fault) from "the frame is
  black" (picture fault) and record the input signature with every result
  (`docs/research/gbs-control-lessons-platform.md` §7).
- Windows consoles default to cp1252: scripts that print non-ASCII call
  `sys.stdout.reconfigure(encoding="utf-8")`.
