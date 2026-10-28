# Persistence revision correction

Follow-up to 5ff3c3c. The original dirty-title helper used the view repaint result,
which missed hidden persisted fields. A rollback-safe generated Data revision now
tracks actual final persisted-state changes, separate from view updates and UI
generation. Save completion also checks live preedit buffers. No persistence
snapshot copying/encoding is added to edits; existing state dirty flags are reused.

Tests include hidden persisted action after Saved, no-op/nonpersisted actions,
failed transactions, last-write final-value no-ops, save racing composition, and
conservative dirty-after-undo-back-to-equal-content behavior. Title cleanup after
composition cancellation/rejected save/collapsed input is tested separately.

This follow-up did not operate the desktop. Earlier forms screenshots and binary
hash belong to 5ff3c3c; do not describe them as final-revision desktop acceptance.
The final Release form build and default counter CTest are automated checks.

- Full revision regression: 55 tests passed in107.851s; script/compiler/live checks
  passed. This run preceded the final two title-cleanup edge assertions.
- Final focused tests exercise generated revision and both synthetic host variants
  (plain and direct ATK route), including those cleanup edges.
