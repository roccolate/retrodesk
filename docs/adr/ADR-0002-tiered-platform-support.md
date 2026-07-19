# ADR-0002: Tiered Platform Support

## Status

Accepted

## Context

Not all platforms can provide identical terminal capabilities (mouse semantics,
resize behavior, feature envelopes).

## Decision

- Tier 1: Windows, Linux.
- Tier 2: macOS, DOS.

Use capability-driven runtime behavior instead of forced parity.

## Consequences

- Universal support remains explicit and honest.
- Linux virtual console can stay keyboard-first without blocking progress.
- DOS can remain supported with reduced scope and clean fallbacks.
- A target tier is not a release claim. Each release candidate must document the
  current validation status and any adapter/gating gaps for that platform.
