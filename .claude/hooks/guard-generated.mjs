// PreToolUse guard for Edit|Write: generated files are never edited by hand.
// Exits 2 (block) with a message naming the source to change instead.
import { readFileSync } from "node:fs";
import { relative, resolve, sep } from "node:path";

const input = JSON.parse(readFileSync(0, "utf8"));
const filePath = input.tool_input?.file_path;
if (!filePath) process.exit(0);

const root = process.env.CLAUDE_PROJECT_DIR ?? input.cwd;
const rel = relative(root, resolve(root, filePath)).split(sep).join("/");

const RULES = [
  { test: (p) => /\.gen\.[a-z]+$/.test(p), source: "the schema/ file named in its header" },
  { test: (p) => p === "docs/reference/register-map.md", source: "schema/registers/tv5725.yaml" },
  { test: (p) => p.startsWith("docs/reference/settings"), source: "schema/settings.yaml" },
  { test: (p) => p.startsWith("docs/reference/api"), source: "schema/api.yaml" },
];

const hit = RULES.find((r) => r.test(rel));
if (hit) {
  process.stderr.write(
    `Blocked: ${rel} is generated. Edit ${hit.source}, then run ` +
      "`uv run tools/codegen/generate.py` (see the regenerate-code skill).\n",
  );
  process.exit(2);
}
process.exit(0);
