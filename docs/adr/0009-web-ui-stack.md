# 0009. Web UI: Preact 11 + signals, TypeScript 7, Vite 8, Biome, Vitest, MSW, Playwright

- Status: accepted
- Date: 2026-10-13

## Context

The UI is embedded in a 1 MB firmware image and polls state every 1.5 s; it must stay small, keep
form controls stable while values refresh (the reference UI lost focus and closed selects when it
rebuilt the DOM), and be typed against the generated settings schema. Measured on 2026-10-13 with
Vite 8 (gzip -9 of an equivalent micro-app): vanilla 0.35 KB, VanJS 1.3 KB, Solid 4.8 KB, Preact
5.6 KB, Preact + signals 8.1 KB, Lit 6.6 KB, Svelte 5 13.1 KB
([tech stack §2.7](../research/tech-stack.md)).

## Decision

- **Preact 11 with `@preact/signals`**, TypeScript, JSX. Signals update polled values in place
  without re-rendering forms. Preact 10.29.x is the fallback if 11 (released 2026-09-30) misbehaves.
- **Vite 8** with `vite-plugin-singlefile`; a post-build step gzips at level 9 and emits the header
  the firmware embeds.
- **TypeScript 7** (`tsc --noEmit` for type checking) and **Biome** for lint and format
  (typescript-eslint does not support TypeScript 7 yet).
- **Vitest 5** with happy-dom for units, **MSW 3** as the device mock shared by development and
  tests, **Playwright** for end-to-end runs against `vite preview` plus the mock.
- No CSS framework: a small hand-written stylesheet with design tokens, dark theme by default.
- pnpm manages dependencies (lockfile committed).
- Budget: the compressed bundle stays at or below 64 KiB, enforced in CI.

## Consequences

The UI baseline is ~8 KB; the rest is our code. The newest major versions (Preact 11, Vite 8,
TypeScript 7, Vitest 5, MSW 3) carry early-adopter risk, mitigated by Renovate grouping and CI.
