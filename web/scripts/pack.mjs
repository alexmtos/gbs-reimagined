// Compresses the single-file build for embedding in the firmware (ADR 0008):
// dist/index.html -> dist/index.html.gz (gzip -9), and prints the size the budget checks.
import { readFileSync, writeFileSync } from "node:fs";
import { gzipSync } from "node:zlib";

const html = readFileSync("dist/index.html");
const gz = gzipSync(html, { level: 9 });
writeFileSync("dist/index.html.gz", gz);
console.log(`ui bundle: ${html.length} B html, ${gz.length} B gzip -> dist/index.html.gz`);
