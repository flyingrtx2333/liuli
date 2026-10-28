# v0.5 merged checkpoint

Source code HEAD5496b7d before documentation/evidence commit. Native runtime binary SHA256 is recorded in SHA256SUMS and ACTUAL-DESKTOP-QA.md.

- Parent aggregate:43 Python tests in71.041s, strict scripted/live builds and smoke, exit0.
- Parent accessibility-enabled CMake build: exit0;3 CTests passed after the runtime guard/status publishing follow-up.
- Actual merged X11: independent AT-SPI probe,3 same-process lifecycle cycles, grapheme editing, pointer selection, clipboard, undo/redo, responsive resize, Saved/Load rejected/Loaded frame-name notifications and Alt-F4 close all passed under G_DEBUG=fatal-warnings.
- No GLib/X11 warning in the merged final run. Deliberately invalid input/state rejection remains in logs as expected evidence, not a hidden failure.
- Bool/conditions are native compiler/runtime tests, not a newly implemented checkbox. Accessibility is optional, not full screen-reader/platform acceptance.
- Later enabled-controls and text-geometry branches are not included in this checkpoint. See known-limitations.md for the separate unlocalized close investigation.
