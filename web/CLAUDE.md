# Web UI

Preact 11 + `@preact/signals`, TypeScript 7, Vite 8 (single-file build), Biome, Vitest 5 + MSW 3.
The build is gzip-compressed and embedded into the firmware image (ADR 0008, 0009).

## Commands

```sh
pnpm install
pnpm dev                     # mock device (MSW) answers /api — no board needed
GBS_HOST=<board-ip> pnpm dev # proxy /api to a real board instead
pnpm check                   # typecheck + lint + test + build (what CI runs)
pnpm build                   # dist/index.html and dist/index.html.gz (size printed)
```

## Rules

- Strings and setting metadata come from `src/gen/` (generated from `schema/`). Add or change text
  in `schema/i18n/*.yaml`, then `uv run tools/codegen/generate.py`; never hard-code UI text.
- Every request goes through `src/api/client.ts` (8 s deadline). Polling never overlaps
  (`src/state/status.ts`).
- Values that refresh from the board live in signals so forms are not re-rendered under the user.
- Keep the gzip bundle under 64 KiB (`tools/size/budget.json`); new dependencies need the project
  owner's agreement.
- The mock device (`src/mocks/handlers.ts`) must answer every route the UI calls; tests run against
  it with `onUnhandledFrame: "error"`.
