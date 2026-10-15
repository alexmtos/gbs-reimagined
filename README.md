# GBS Reimagined

A new firmware for TV5725-based video scalers — starting with the **RetroScaler GBS-C** — that
turns analogue consoles and computers (RGBS/SCART, component, VGA) into a clean HDMI or VGA
picture, and can be controlled from a **web page**, the built-in **OLED and knob**, or a
**serial console**, with live monitoring of what the scaler is doing.

> **Install it from your browser** at **https://alexmtos.github.io/gbs-reimagined/** (Chrome or Edge
> on a computer, and a USB data cable); the [user guide](docs/user-guide/en/README.md) walks through
> it ([em português](docs/user-guide/pt-BR/README.md)). Releases and notes:
> [GitHub releases](https://github.com/alexmtos/gbs-reimagined/releases). What comes next: the
> [roadmap](docs/roadmap.md).

## What it aims to be

- **A good picture with zero setup**, and recognisable options when you want them — named the way
  RetroTINK and OSSC users already know them.
- **Never stranded:** risky changes undo themselves unless you confirm, a safe mode is always
  reachable, and the OLED keeps working when the TV shows nothing.
- **Honest status:** a sentence that says what the scaler is doing, followed by the numbers.
- **Easy to install and update** from the browser, keeping your settings.
- **Analogue output kept**: VGA/RGBS for CRT users, not only HDMI.

What the hardware cannot do (masks, HDR, VRR, 4K, image-based auto-phase) is out of scope and said
so plainly — see the [market study](docs/research/market-scalers.md).

## Documentation

| For | Start here |
|---|---|
| Users | [User guide (English)](docs/user-guide/en/README.md) · [Guia do usuário (Português)](docs/user-guide/pt-BR/README.md) |
| Contributors | [docs/README.md](docs/README.md) — architecture, video engine, UX, decisions, research |
| AI-assisted development | [CLAUDE.md](CLAUDE.md) and `.claude/` (rules, skills, agents, hooks) |

## Building

```sh
uv sync                                     # Python tooling (code generation, bench, board)
uv run tools/codegen/generate.py            # derived files from schema/
pnpm -C web install && pnpm -C web check    # web UI
pnpm -C installer install && pnpm -C installer check  # installer page
cd firmware && pio test -e native           # host tests
cd firmware && pio run -e gbsc              # firmware image
```

Requirements: Python 3.12 with [uv](https://docs.astral.sh/uv/), Node.js 24 with pnpm 10,
PlatformIO Core 6.2.

## Licence and credits

GPL-3.0 — see [LICENSE](LICENSE). The code is new; factual chip data (register map, baseline
register programs) comes from the [gbs-control](https://github.com/ramapcsx2/gbs-control) community,
whose work made TV5725 boards usable. Details in [NOTICE.md](NOTICE.md); third-party components in
[THIRD_PARTY.md](THIRD_PARTY.md).
