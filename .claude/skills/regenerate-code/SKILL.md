---
name: regenerate-code
description: Regenerates the firmware headers, web modules and reference docs derived from schema/ (register database, baseline programs, settings, translations) and validates the sources. Use after editing anything under schema/ or tools/codegen/, when a hook blocks an edit to a generated file, or when CI reports stale generated files.
---

1. Run `uv run tools/codegen/generate.py` from the repository root.
   - It validates first. A message such as `[pt-BR] output.aspect: hint longer than 21` or
     `message nav.foo: missing` names the exact key: fix the source in `schema/` and rerun. Do not
     weaken a limit to make a string fit; shorten the string (the OLED line really is 21 characters).
2. Inspect what changed: `git status --short` and `git diff --stat`. Only files carrying the
   GENERATED banner, plus the sources you edited, should appear.
3. Run the consumers: `cd firmware && pio test -e native`, then `pnpm -C web check`.
4. Commit the sources and the generated files together, in one commit.

Never hand-edit a generated file, even to "fix" one line — the next generation would undo it and
CI's `--check` would fail first.

When the generator itself changes (`tools/codegen/*.py`), also rerun `--check` after generating to
prove the output is stable (a second run must write nothing).
