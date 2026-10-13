---
paths:
  - "docs/**/*.md"
  - "**/README.md"
---

# Documentation rules

- Technical documentation is English. The user guide exists in English (`docs/user-guide/en/`)
  and Brazilian Portuguese (`docs/user-guide/pt-BR/`); a change to one is mirrored in the other in
  the same commit.
- A number in prose (a limit, a size, a measurement) names where it came from: a datasheet page,
  a bench run in `docs/bench/`, or a cited research note. Measurements quoted from the reference
  project are marked as such until reproduced on this bench.
- Decisions that change the architecture get an ADR in `docs/adr/` (copy `docs/adr/template.md`);
  never rewrite an accepted ADR, supersede it.
- Generated references (`docs/reference/*`) are never edited by hand.
- Write for the reader named at the top of each document; the user guide never assumes the reader
  knows what a register is.
