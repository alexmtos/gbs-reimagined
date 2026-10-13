# Notice and attribution

GBS Reimagined is free software, licensed under the GNU General Public License version 3
(see [LICENSE](LICENSE)). All source code, architecture, user interfaces and documentation in this
repository were written for this project. This file records the **factual data** the project takes
from elsewhere, and the knowledge it builds on.

## Chip data taken from gbs-control

[gbs-control](https://github.com/ramapcsx2/gbs-control) (GPL-3.0) is the community firmware for
TV5725-based scalers, maintained by ramapcsx2 and continuing the work of dooklink, mybook4,
Ian Stedman and others. GBS Reimagined uses two kinds of data from it, in this repository's own
formats, under the same licence:

| Data | Where it lives here | Taken from |
|---|---|---|
| TV5725 register field loci (name, segment, offset, bit, width) | `schema/registers/tv5725.yaml` | `tv5725.h` of the [JuergenLeber/gbs-control](https://github.com/JuergenLeber/gbs-control) fork as extended by a local branch (958 declarations, 953 kept) |
| Baseline register programs (register images per input standard and output raster, and two factory-firmware dumps) | `schema/programs/*.yaml` | the `ntsc_*.h`, `pal_*.h` and `ofw_*.h` arrays at JuergenLeber/gbs-control@7bfb9b3 |

Both were converted by the importers in `tools/codegen/` (`import_tv5725.py`,
`import_programs.py`), which read the address annotated beside every value rather than assuming an
order. No code was taken.

## Vendor documentation

Register access (read-only or read/write), block names and page numbers in
`schema/registers/tv5725.yaml` come from the Tvia *TrueView5725 Registers Definition*
(RD-5725-1.1). The vendor PDFs are not redistributed here; `docs/hardware/datasheets.md` lists them
with their sources and checksums.

## Knowledge

The bench measurements, limits and failure modes recorded by the gbs-control community — in
particular the extensive notes of the gbs-control fork this project's author worked with — are
cited where they are used, in `docs/research/`. A measurement quoted from there is marked as such
until it is reproduced on this project's own bench.
