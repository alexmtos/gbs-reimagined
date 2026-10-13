import { defineConfig } from "vite";
import { viteSingleFile } from "vite-plugin-singlefile";

// One self-contained HTML file: scripts and styles inlined, so the firmware serves a single
// gzip-compressed asset (ADR 0008). `GBS_HOST=<ip> pnpm dev` proxies the API to a real board and
// disables the mock device.
const boardHost = process.env.GBS_HOST;
if (boardHost) process.env.VITE_BOARD = boardHost;

export default defineConfig(({ command }) => ({
  plugins: [viteSingleFile()],
  // public/ only holds the mock service worker; it must not reach the firmware bundle.
  publicDir: command === "build" ? false : "public",
  build: {
    target: "es2022",
    cssCodeSplit: false,
    assetsInlineLimit: 100_000,
    reportCompressedSize: true,
  },
  server: boardHost ? { proxy: { "/api": `http://${boardHost}` } } : {},
  test: {
    environment: "happy-dom",
    // Relative fetches in tests resolve against this origin (the mock board).
    environmentOptions: { happyDOM: { url: "http://board.test/" } },
    include: ["tests/**/*.test.ts", "tests/**/*.test.tsx"],
  },
}));
