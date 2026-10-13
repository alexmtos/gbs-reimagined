// PostToolUse formatter for Edit|Write: clang-format for C/C++, prettier for the web UI,
// ruff for Python. A missing formatter is not an error; a failing one is reported to Claude.
import { existsSync, readFileSync } from "node:fs";
import { extname, join, relative, resolve, sep } from "node:path";
import { spawnSync } from "node:child_process";

const input = JSON.parse(readFileSync(0, "utf8"));
const filePath = input.tool_input?.file_path;
if (!filePath) process.exit(0);

const root = process.env.CLAUDE_PROJECT_DIR ?? input.cwd;
const abs = resolve(root, filePath);
const rel = relative(root, abs).split(sep).join("/");
if (/\.gen\.[a-z]+$/.test(rel)) process.exit(0);

const ext = extname(rel).toLowerCase();
const isWin = process.platform === "win32";
let run = null;

if ([".c", ".h", ".cpp", ".hpp"].includes(ext) && rel.startsWith("firmware/")) {
  run = spawnSync("clang-format", ["-i", abs], { encoding: "utf8" });
} else if (
  [".ts", ".tsx", ".js", ".mjs", ".css", ".html", ".json"].includes(ext) &&
  rel.startsWith("web/")
) {
  const prettier = join(root, "web", "node_modules", ".bin", isWin ? "prettier.cmd" : "prettier");
  if (!existsSync(prettier)) process.exit(0);
  run = spawnSync(prettier, ["--write", abs], { encoding: "utf8", shell: isWin });
} else if (ext === ".py") {
  run = spawnSync("uv", ["run", "--quiet", "ruff", "format", abs], {
    encoding: "utf8",
    cwd: root,
  });
} else {
  process.exit(0);
}

if (run.error?.code === "ENOENT") process.exit(0);
if (run.status !== 0) {
  process.stderr.write(`Formatter failed on ${rel}: ${run.stderr || run.error?.message || ""}\n`);
  process.exit(2);
}
process.exit(0);
