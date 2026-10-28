# v0.6 merged verification

Source commit: `3007dde`. Validated source checkpoint; Library delivery is recorded separately by the parent.

- `CMAKE=/tmp/explicit-ui-cmake/cmake/data/bin/cmake sh scripts/check.sh`: **56 tests, PASS**, 95.450 s; full log `forms-final-check.log`.
- Release custom form build `build/v06-forms`, Linux accessibility enabled with extracted SDK: PASS. Build and CTest log `forms-build.log`.
- Applicable custom-form CTest: scripted_smoke and accessibility_tree, **2/2 PASS**. Counter-only window_synthetic fixture intentionally not registered for this custom model/layout; not substitute for form desktop validation.
- Final merged actual X11 proof: PASS for Unicode independent fields, undo/redo, save/reset/load/restart, native AT-SPI text geometry and enabled-control admission. See ACTUAL-DESKTOP-QA.md for exact scope; old screenshots were not reused.
- Windows/macOS, real IME and full Orca: not passed. Existing close-time X11 investigation remains open.

Older `check.log`, `cmake.log` and `enabled-build.log` predate the final forms merge;
they are retained historical intermediate output, not the final 56-test run.
Use `forms-final-check.log` for the 56-test final source proof, `forms-build.log`
for the final form build, and `enabled-final-build.log` for the rebuilt final
source enabled-controls fixture. Generated probe executable is under build/,
not shipped in evidence; probe-build-command.txt records reproduction.
