# 0007. Users configure settings, not registers

- Status: accepted
- Date: 2026-10-13

## Context

The target users operate scalers like the RetroTINK or the OSSC: they expect named options with
documented effects, profiles they can recall, and safe defaults
([market study](../research/market-scalers.md)). The reference fork exposed all 958 chip fields in
its web UI; most had never been measured, and fields rewritten by the firmware on its own looked
like controls that did not work.

## Decision

Every user-facing control is a **setting** defined in `schema/settings.yaml`: a name, a type and
range, a default, labels and help in each language, and the one firmware function that applies it.
A setting never shares ownership of a chip field with an automatic algorithm. Raw register access
exists only as an expert tool (serial console and a diagnostics page), clearly marked.

## Consequences

The interfaces stay small and explainable, and translations stay tractable. Exposing a new chip
capability requires designing a setting for it, which is deliberate friction.
