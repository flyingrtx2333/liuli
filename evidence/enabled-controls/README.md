# Enabled-button verification — 2026-10-27

Branch based on 5496b7d; no main-worktree edits.

- Full scripts/check.sh: 45 tests passed, scripted and live smoke passed.
- Separate fresh CMake a11y directory: strict warning build; CTest 3/3.
- EUI_A11Y_SDK-enabled test_enabled_controls.py: 2/2, includes stale queued
  activation/focus suppression without creating a bus/provider.
- Native ATK tree test asserts disabled/enabled state sets, each change event,
  unchanged-state event suppression, rejected disabled action and focus.
- Dummy host checks fixed/flow binding validation, actual click/Enter/Space/Tab,
  loss of focus, preserved default behavior, pixel difference, no-op repaint
  suppression and direct Model action access.

## Own cloud desktop

After integrated-QA agent released the desktop, launched this example on real
X11 using the already task-local SDL no-shared-memory build. CUA screenshots
were observed in the task tool results:
1. Increment muted gray; disabled click/Enter kept Count: 0.
2. Two Tabs stayed on Toggle.
3. Toggle then Tab/Enter made Increment blue and Count: 1.
4. Toggle off then disabled click/Enter kept Count: 1.
5. Final run closed with Alt-F4, exit 0; final log records enabled=false,
   counter=Count: 1, patches=5, renders=8.

First titlebar-mouse-close attempt printed window-closed normally but then an
X11 BadWindow / X_SendEvent error at shutdown (exit 1). An idle Alt-F4 rerun and
a full-interaction Alt-F4 rerun both exited 0. Cause not established; no global
X11 error suppression or unrelated SDL change was made. This does not claim
all close paths are certified.

No live enabled-state AT-SPI client/screen-reader acceptance was performed.
AT provider state and queued-host request coverage is automated as above.
The source supports buttons only, not checkbox/visibility bindings.
