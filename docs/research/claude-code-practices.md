# Claude Code repository practices (condensed from official docs, fetched 2026-10-13)

Sources: code.claude.com/docs/en/{memory,settings,permissions,claude-directory,skills,sub-agents,hooks,hooks-guide,mcp,output-styles,best-practices,large-codebases,features-overview}; platform.claude.com/docs/en/agents-and-tools/agent-skills/best-practices; agentskills.io/specification

## CLAUDE.md
- Locations: ./CLAUDE.md or ./.claude/CLAUDE.md (team), ./CLAUDE.local.md (personal, gitignore), nested <subdir>/CLAUDE.md (loads when Claude touches files in that subtree), ~/.claude/CLAUDE.md (user).
- @path imports (relative to the importing file, up to 4 hops); imports load at launch -> do NOT @import docs/; name docs by path instead.
- Block-level HTML comments are stripped before injection.
- Keep each file < 200 lines. Include: commands Claude can't guess, conventions differing from defaults, test instructions, architecture decisions, gotchas. Exclude: what the code says, standard conventions, tutorials, fast-changing facts.
- Make instructions checkable. Use IMPORTANT sparingly. Procedures -> skills; path-specific rules -> .claude/rules/ with `paths:`; must-happen -> hooks/deny rules.
- AGENTS.md: CLAUDE.md can start with `@AGENTS.md` to share with other tools.

## .claude/
- settings.json (team, committed) / settings.local.json (personal). Strict JSON. "$schema": "https://json.schemastore.org/claude-code-settings.json".
- permissions: deny -> ask -> allow; first match wins. `Bash(npm run *)`, `Edit(/firmware/generated/**)`, `Read(./.env)`, `WebFetch(domain:x)`, `Skill(name)`.
- defaultMode auto/bypassPermissions ignored in project settings.
- .claude/rules/**/*.md with optional frontmatter `paths:` (list of globs) -> loads only when matching files are touched.

## Skills (.claude/skills/<name>/SKILL.md + scripts/ references/ assets/)
- Frontmatter (all optional in Claude Code; kebab-case except when_to_use): name, description, when_to_use, argument-hint, arguments, disable-model-invocation, user-invocable, allowed-tools, disallowed-tools, model, effort, context: fork, agent, background, hooks, paths, shell, license, compatibility, metadata.
- Portable (open standard): name (1-64, lowercase/digits/hyphens, equals dir name), description (<=1024 chars, third person, what + when), license, compatibility, metadata, allowed-tools.
- Body < 500 lines; references one level deep; ToC for reference files > 100 lines. Descriptions always in context; body loads on invocation.
- Substitutions: $ARGUMENTS, $N, ${CLAUDE_SKILL_DIR}, ${CLAUDE_PROJECT_DIR}; !`cmd` injects command output.
- Commands merged into skills (.claude/commands still works; prefer skills).
- Authoring: assume Claude is capable; match specificity to fragility (exact commands for fragile sequences); say whether to RUN or READ a script; scripts handle their own errors; forward slashes; numbered steps + validate-fix-repeat loops; one default not a menu; avoid time-sensitive wording; names in gerund/action form.

## Subagents (.claude/agents/*.md)
- Frontmatter camelCase: name, description (trigger), tools, disallowedTools, model (sonnet/opus/haiku/fable/inherit), permissionMode, maxTurns, skills, mcpServers, hooks, memory, isolation: worktree, effort, omitClaudeMd.
- Use for: reading many files, enforced tool limits, independent review.

## Hooks (settings.json "hooks")
- Events: SessionStart, UserPromptSubmit, PreToolUse, PermissionRequest, PostToolUse, PostToolUseFailure, Stop, SubagentStop, PreCompact, PostCompact, InstructionsLoaded, ConfigChange, SessionEnd, ...
- Schema: { "hooks": { "PostToolUse": [ { "matcher": "Edit|Write", "hooks": [ { "type": "command", "command": "node", "args": ["${CLAUDE_PROJECT_DIR}/.claude/hooks/x.mjs"], "timeout": 60 } ] } ] } }
- Exec form (command + args) avoids shell differences (good on Windows). Input JSON on stdin (tool_input.file_path). Exit 2 = block (PreToolUse) / feedback (PostToolUse). Other non-zero = non-blocking.
- PreToolUse JSON decision: {"hookSpecificOutput":{"hookEventName":"PreToolUse","permissionDecision":"deny","permissionDecisionReason":"..."}}
- Stop gate: check stop_hook_active.

## Repo structure guidance
- Root CLAUDE.md repo-wide; per-area CLAUDE.md; per-area skills in <area>/.claude/skills/ (load when working there).
- Project settings are not inherited from parent dirs; launch from repo root.
- Generated code: block with Edit deny rules + a PreToolUse hook explaining where to change instead.
- ADRs are a project convention (not prescribed) — link docs/adr/ from CLAUDE.md in one line.
- Review .claude/ and CLAUDE.md changes in PRs like CI config (CODEOWNERS).
- Setup checks: /context, /memory, /status, /permissions, /hooks, claude doctor, /skill-doctor.
