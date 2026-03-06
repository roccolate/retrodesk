# Technical Debt Policy

## Goals

- Prevent structural debt while the codebase scales.
- Make temporary debt explicit, bounded, and reviewable.

## Acceptable Temporary Debt

- Small compatibility shims with documented removal criteria.
- Platform-specific fallback branches required by capability differences.

## Unacceptable Debt

- hidden global runtime ownership,
- duplicate canonical build logic,
- backend types in public domain headers,
- multi-loop input handling,
- silent partial-feature behavior without capability guard.

## Debt Expiration Rule

Any temporary debt item must include:

- owner,
- reason,
- removal trigger,
- target milestone.

If trigger is met, removal becomes mandatory.

## Review Checklist

- Are module boundaries still respected?
- Is event flow still normalized?
- Is frame flush still centralized?
- Does this change reduce or expand platform divergence?
- Does it add new undocumented assumptions?

## Structural Metrics to Track

- count of global runtime owner variables,
- count of public headers exposing backend-native types,
- number of frame flush call sites,
- count of duplicated source-list definitions across build systems.

## v0.1 Rule

`v0.1.0` cannot be tagged with unresolved debt that violates
`FOUNDATION_PRINCIPLES.md`. Temporary exceptions must be documented in
`RELEASE_0.1_CHECKLIST.md` with owner and removal trigger.
