# 0005. Single sources of truth in `schema/`, generated everywhere else

- Status: accepted
- Date: 2026-10-13

## Context

The firmware, the web UI, the serial console, the OLED menu and the documentation all need the same
facts: register loci, settings with their ranges and labels, the API shape, translated strings.
Hand copies drift; in the reference project a UI button sent a parameter the firmware did not read,
and nobody noticed for weeks.

## Decision

`schema/` holds the sources (YAML). `tools/codegen/generate.py` produces C++ headers, TypeScript
modules and reference docs from them. Generated files are committed, carry a "do not edit" banner,
are blocked from hand edits by a Claude Code hook and deny rule, and CI fails when they are stale
(`generate.py --check`). Firmware and UI share a schema hash; the UI refuses to write settings to a
firmware with a different hash.

## Consequences

Adding a setting is one edit plus a regeneration. Generated files show up in diffs, which is the
point: reviewers see the effect. The generator is a critical tool and has its own tests.
