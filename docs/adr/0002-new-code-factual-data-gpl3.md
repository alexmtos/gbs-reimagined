# 0002. Write all code new; reuse only factual chip data; license GPL-3.0

- Status: accepted
- Date: 2026-10-13

## Context

The project owner asked for a new project *inspired by*, not based on, gbs-control. A working
TV5725 picture, however, needs about 500 register values per output mode that took the community
years to establish (dumped from the factory firmware and refined, GPL-3.0). Re-deriving them from
the 235-page datasheet alone would delay a first picture by months, with a high risk of never
matching the existing quality.

## Decision

- All code, architecture, interfaces and documentation are written for this project.
- Factual chip data is reused, converted into this project's formats with recorded provenance:
  register field loci (`schema/registers/tv5725.yaml`) and the baseline register programs and
  constant sections (`schema/programs/`). See `NOTICE.md`.
- Measured knowledge from the reference project is used as cited research (`docs/research/`),
  marked as unverified until reproduced on this project's bench.
- The project is licensed GPL-3.0, compatible with the data's origin.

## Consequences

A first picture is reachable in the first implementation milestone. The licence is fixed at GPL-3.0
(a permissive licence would require a clean-room re-derivation of the data). Every value taken this
way can be traced to its source file and commit.
