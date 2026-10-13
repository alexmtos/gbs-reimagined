# 0001. Record architecture decisions

- Status: accepted
- Date: 2026-10-13

## Context

GBS Reimagined is developed with AI assistance (Claude Code) and by humans. Decisions taken in a
conversation are lost to the next session unless written down; the reference project kept its
rationale in 400+ KB of prose that drifted out of sync with itself.

## Decision

Every decision that shapes the architecture, a public interface, or a safety rule gets a short ADR
in `docs/adr/`, numbered, copied from `template.md`. Accepted ADRs are never rewritten; a changed
decision is a new ADR that supersedes the old one. `CLAUDE.md` points here in one line.

## Consequences

Decisions are cheap to find and to challenge. The cost is a few minutes per decision and the
discipline to supersede instead of editing.
