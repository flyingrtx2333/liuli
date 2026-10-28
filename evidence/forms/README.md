# Bounded forms verification — 2026-10-27

Baseline: 86ea5c6; feature branch multi-input-forms. Build: Release C++17,
SDL2/Cairo/Pango, EUI_LINUX_ACCESSIBILITY=ON with task-local SDK. Final form binary
SHA256: 52deb7b23c0a24b505d71480d1e98f5f3d6775b5b3d0fa2a2a123fa2faac38cc.

## Automated

- `check.log`: 54 Python tests, including optional direct ATK host request routing,
  plus compiler/script/live smoke. The host test uses synthetic SDL dummy events,
  not system IME or actual desktop proof.
- `final-focused.log`: five transaction/form/initial-state tests after title-status fix.
- `default-ctest.log`: bundled counter configuration CTest 3/3. The form's custom
  configuration initially ran the existing counter-only smoke and rejected its
  missing increment/list fixture; that is not a form interaction test. Root owns
  the separate CMake registration fix. Form build itself succeeded.

## Actual own-cloud X11 desktop

Used `scripts/run-cloud-window.sh` from the main workspace (process-local no-SHM
SDL build), coherent pinned task-local ATK/AT-SPI 2.62.1 libraries and
`G_DEBUG=fatal-warnings`, no global settings or library replacement.

1. Started generated `form.eui.json` / `form.layout.json`, missing explicit state
   file accepted as new; AT-SPI exposed distinct Name and Project entries.
2. Edited Name=Lin, Tab to Project=Glass forms. Undo in Project restored Native UI
   while Name stayed Lin; redo, Shift-Tab, undo in Name restored Ada while Project
   stayed Glass forms. Redo restored Lin.
3. Ctrl+S => Saved, named Reset both fields cleared both atomically; Ctrl+O restored
   both. First titlebar mouse close exited 0 (`first-run.*`).
4. Rebuilt with conservative Unsaved changes title fix; restarted with same file,
   both values restored (`restart-tree.txt`). Real Unicode edits via supported
   desktop text input (not an installed IME) produced 文文🙂 / 跨端表单, independent
   fields and computed preview (`unicode-tree.txt`, `unicode.jpg`).
5. Saved, reset => Unsaved changes, loaded => both Unicode values restored
   (`final-reset-tree.txt`). Earlier `reset-tree.txt` records the old stale Saved
   title and is deliberately retained as the defect evidence, not final behavior.

`edited.jpg` and `unicode.jpg` are actual app screenshots, not generated mockups.
The renderer is a glass-inspired CPU approximation, not Apple Liquid Glass.

## Scope

Maximum16 fields /256 UTF-8 bytes each. Shared-state peer synchronization resets
stale peer histories; unrelated fields retain independent caret/selection/history.
No multiline, password, system-IME, Orca, HiDPI, Windows/macOS or Electron parity
claim. The pre-existing rare X11 shutdown race remains open despite clean closes.

6. Independent native AT-SPI client `tests/native/forms_atspi_probe.cpp --edit`
   passed (`atspi-probe.log`, exit0): Name=名🙂A and Project=项目é, independent
   Unicode-scalar caret/selection, focus, distinct screen origins (140,238) and
   (140,308), every character rectangle/point mapping and clipped-point rejection.
   Initial probe used AT-SPI add_selection, unsupported by the existing bridge;
   its failure is retained in `probe-initial-api-assumption.*`. The corrected probe
   explicitly uses the implemented set_selection(index0), not a claim of complete
   selection API support. No bridge API implementation was changed to mask this.
7. Final Ctrl+S and titlebar mouse close exited0 (`desktop.log`, `desktop.exit`).
   Desktop released, no form window remains; extra probe terminal tab closed.
