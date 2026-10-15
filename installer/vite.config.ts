import { defineConfig } from "vitest/config";

// A static site: index.html, the bundled ESP Web Tools, and public/ as staged by
// tools/release/installer_site.py (firmware, manifests, latest.json). Relative paths, so the same
// build works from localhost and from the GitHub Pages project path.
export default defineConfig({
  base: "./",
  build: {
    target: "es2022",
    reportCompressedSize: true,
  },
  preview: {
    // Web Serial needs a secure context: localhost is one, a LAN address is not.
    host: "localhost",
  },
  test: {
    environment: "happy-dom",
    include: ["tests/**/*.test.ts"],
  },
});
