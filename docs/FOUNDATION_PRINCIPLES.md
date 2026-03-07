# Foundation Principles

## Runtime Rules

- Exactly one main event loop per process.
- Exactly one frame flush point per loop tick.
- No module outside `platform` and `render` may call backend primitives directly.
- No global owner state for desktop runtime.

## API and Layering Rules

- Domain/public headers must not expose `WINDOW *`, `MEVENT`, `getch`, `wgetch`, or `doupdate`.
- `core` consumes `RetroEvent`, not backend-native events.
- `wm` owns focus, z-order, hit testing, and drag policy.
- `app` modules are hosted units; they never poll input directly.
- `wm`, `app`, and `ui` produce draw lists; only `render` executes backend draw calls.
- Visual styles must come from theme tokens, not ad-hoc hardcoded palettes in runtime loops.

## Lifecycle Rules

- Every owned object follows explicit create/run/destroy lifecycle.
- Init failures must rollback previously created resources.
- Shutdown APIs must be idempotent where practical.

## Capability Rules

- Platform differences are represented as capabilities, not scattered `#ifdef` in business logic.
- Linux virtual console (`TERM=linux`) defaults to keyboard-first behavior.
- Features unsupported by current capability profile must degrade explicitly.

## Debt Rejection Rules

The following are not acceptable:

- duplicate source-of-truth build logic,
- nested modal input loops,
- frame flush from multiple unrelated modules,
- exposing backend types into domain headers,
- feature additions that break another platform build.
