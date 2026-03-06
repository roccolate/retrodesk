# ADR-0003: Core Does Not Expose Curses Types

## Status

Accepted

## Context

Direct `curses` type leakage across modules increases coupling, blocks testability,
and makes backend portability harder.

## Decision

Restrict backend-native types to `platform` and `render` internals.
Domain and runtime headers expose only project-level types and contracts.

## Consequences

- The core can be reasoned about and tested independently.
- Backend migration or adaptation becomes cheaper.
- App and WM logic remain portable across capability profiles.
