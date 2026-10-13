---
name: add-setting
description: Adds or changes a user-facing setting end to end — schema entry, English and Brazilian Portuguese strings, firmware apply function, persistence, web/OLED/console exposure and reference docs. Use when the user asks for a new option or control, a new value for an existing option, or a change to a setting's range, default, label or help.
---

A setting is the only way users configure the scaler (ADR 0007). Follow every step; a setting that
exists in one interface and not the others is a defect.

1. **Design it first** and confirm with the user when anything is uncertain:
   - Which single firmware function applies it, and which chip fields it owns. If an automatic
     algorithm (detector, watcher, auto gain…) also writes those fields, stop: one owner per field.
   - Name per the community vocabulary in `docs/research/market-scalers.md` §13.
   - Type, range, default (compatibility first), tier (essential/advanced), milestone, risky?
     (risky = can blank the output, arms the revert).
2. **Schema:** add the entry to `schema/settings.yaml` (fields documented at the top of the file).
   Never reuse an old id for a new meaning: ids are persisted by hash.
3. **Strings:** add `label` (≤ 20 chars), `hint` (≤ 21 chars), `help` and value labels to both
   `schema/i18n/en.yaml` and `schema/i18n/pt-BR.yaml`. Write natural Brazilian Portuguese, not a
   word-for-word translation.
4. **Regenerate:** run the `regenerate-code` skill.
5. **Firmware:** implement the apply function in the module that owns the behaviour and register it
   for the new `settings::Id`; add a host test that the setting moves the intended fields and nothing
   else (fake TV5725 write log).
6. **Interfaces:** the web page and the OLED pick it up from the generated metadata; check the page
   renders it (`pnpm -C web dev`) and that the console `get`/`set` round-trips.
7. **Docs:** if users need more than the help text, update both user guides (`update-user-guide`
   skill).
8. **Verify:** run `build-and-test`. If the setting changes the picture, it is not done until the
   `bench-verify` skill shows the effect on the capture card.
