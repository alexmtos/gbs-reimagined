# Research

What GBS Reimagined learned before writing code, and where each design decision gets its evidence.
Written for contributors; the conclusions that shaped the project are in the
[ADRs](../adr/) and [design documents](../design/).

| Document | Question it answers | Basis |
|---|---|---|
| [market-scalers.md](market-scalers.md) | What do the best scalers (RetroTINK 4K/5X/6X, OSSC/OSSC Pro, Morph 4K/2K, Framemeister, Extron) offer, how do users operate them, and which of those ideas fit a TV5725 + ESP8266? Includes a feature matrix, the community's option vocabulary and ~55 rated ideas. | Official manuals and changelogs, independent reviews, issue trackers (2026-10) |
| [market-shootout-2026.md](market-shootout-2026.md) | How an independent 2026 comparison scored usability | Wobbling Pixels shootout |
| [market-hardware-notes.md](market-hardware-notes.md) | Hardware facts behind the feasibility ratings | TV5725 register map, MS9288A datasheet |
| [tech-stack.md](tech-stack.md) | Which current tools and libraries to build, test, flash and update an ESP8266 firmware with | Release pages and registries (2026-10) |
| [gbs-control-lessons-platform.md](gbs-control-lessons-platform.md) | What the ESP8266 platform, the web server, the OLED and the bench cost the reference project, and the rules each failure produced | Reference fork's docs |
| [gbs-control-lessons-videopath.md](gbs-control-lessons-videopath.md) | What the TV5725 video path does, measured: limits, failure modes, timing, deinterlacing, colour | Reference fork's bench notes |
| [gbs-control-mechanics.md](gbs-control-mechanics.md) | How existing firmwares bring the TV5725 to a picture, step by step, with the constants the chip needs | Code reading of two gbs-control trees |
| [claude-code-practices.md](claude-code-practices.md) | How this repository is set up for Claude Code (CLAUDE.md, rules, skills, agents, hooks) | Official Claude Code documentation |

## How to treat these documents

- **Measurements from the reference project are hypotheses here** until reproduced on this
  project's bench. When a bench run confirms or contradicts one, record it in `docs/bench/` and link
  it from the design document that relied on it.
- The research documents are snapshots (dated in their headers). Update by adding a dated section,
  not by silently rewriting history.
- Product facts about third-party scalers come from public sources cited inline; they are used only
  to decide what GBS Reimagined should offer.
