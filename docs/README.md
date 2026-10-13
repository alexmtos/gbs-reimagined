# Documentation

For contributors. Users should start with the user guide
([English](user-guide/en/README.md) · [Português](user-guide/pt-BR/README.md)).

| Area | Documents |
|---|---|
| Plan | [roadmap.md](roadmap.md) — milestones, gates, open bench questions |
| Decisions | [adr/](adr/) — 0001–0012: record keeping, licensing and data reuse, bus ownership, baseline programs, sources of truth, frame lock, settings model, web server, UI stack, toolchain, device libraries, distribution |
| Design | [design/architecture.md](design/architecture.md) · [design/video-engine.md](design/video-engine.md) · [design/ux.md](design/ux.md) · [design/memory-budget.md](design/memory-budget.md) |
| Hardware | [hardware/board-gbsc.md](hardware/board-gbsc.md) · [hardware/datasheets.md](hardware/datasheets.md) |
| Research | [research/](research/README.md) — market study, technology stack, lessons and mechanics from gbs-control, Claude Code practices |
| Bench | [bench/](bench/) — dated logs of measurements on the real board |
| Reference (generated) | [reference/register-map.md](reference/register-map.md) · [reference/settings.md](reference/settings.md) |

Generated references are rebuilt by `uv run tools/codegen/generate.py`; never edit them by hand.
