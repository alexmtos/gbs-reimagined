---
name: write-adr
description: Records an architecture decision as a numbered ADR in docs/adr/ — context with evidence links, the decision stated so it can be checked, and its consequences — and supersedes an older ADR when a decision changes. Use when a choice affects the architecture, a public interface (API, settings, file formats), a dependency, or a safety rule.
---

1. List `docs/adr/` and take the next number. Copy `docs/adr/template.md` to
   `docs/adr/NNNN-short-imperative-title.md`.
2. **Context:** the forces, with links to the evidence (`docs/research/`, bench runs, issues).
   Quote numbers with their source.
3. **Decision:** what will be done, in checkable statements ("X does Y", "never Z").
4. **Consequences:** what gets easier, what gets harder, what must now be watched.
5. Status `proposed` while the user has not agreed; `accepted` once they have. Ask the user when
   the decision is theirs to make (scope, dependencies, hardware, licensing, hosting).
6. To change an accepted decision, write a new ADR and set the old one's status to
   `superseded by [NNNN](…)` — the only edit ever made to an accepted ADR.
7. Link the ADR from the design document it shapes.
