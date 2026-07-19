# ADR-0001: CMake Is Canonical Build System

## Status

Accepted

## Context

Multiple independent build definitions create drift and inconsistent binaries.

## Decision

Use CMake as canonical build definition for modern targets.
Use Makefile only as wrapper convenience, and keep DJGPP makefile specialized for DOS.

## Consequences

- Build behavior becomes reproducible across Tier 1 platforms.
- Source list drift is reduced.
- CI can validate a single canonical target definition.
- Specialized profiles such as DJGPP must be checked against the canonical
  CMake source manifest.
