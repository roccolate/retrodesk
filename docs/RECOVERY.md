# Emergency Notepad Recovery

RetroDesk normally protects dirty documents through the Save / Discard / Cancel
close workflow. A terminal disconnect, input-backend failure, forced Desktop
teardown, or another abnormal destruction path may make that interactive
workflow impossible.

As a final safety net, Notepad writes its current UTF-8 document when a dirty
Desktop-owned instance reaches destruction without first being saved or
explicitly discarded.

## Recovery location

RetroDesk selects the first available location in this order:

1. `RETRODESK_RECOVERY_DIR`;
2. `TEMP` or `TMP` on Windows;
3. `TMPDIR` on POSIX hosts;
4. the current working directory.

Files use this form:

```text
retrodesk-notepad-recovery-<timestamp>-<instance>-<attempt>.txt
```

A successful recovery is also reported to standard error. Recovery files are
never used as the document's new save path and are not deleted automatically.
The user must inspect, save, or remove them.

## Normal close behavior

- Saving updates the normal document and clears the dirty state.
- Discarding explicitly clears the dirty state.
- Cancelling keeps the Notepad instance alive.
- Clean or explicitly discarded instances do not produce recovery files.

Emergency recovery is not a replacement for autosave, session restore, or
atomic storage conflict handling. It is a last-resort barrier against silent
data loss during forced destruction.
