# Final merged actual desktop QA — 2026-10-27

Source: `3007dde`; no source edits during QA. Form binary SHA-256:
`4ab299b6f5a4240573c059e39bf3a198a2f66223ad424d36e898212e47b40443`.
Enabled-controls binary rebuilt from this same source:
`158847b66488da0d6ecd75ade72418f928c1163505e6164aebe9f36d560be587`.

## Environment

Own cloud actual X11 desktop, SDL X11 CPU renderer, scoped no-SHM SDL runtime,
coherent task-local ATK/AT-SPI 2.62.1; `EUI_ACCESSIBILITY=1 G_DEBUG=fatal-warnings`.
Launchers and logs here preserve commands. No global settings, private D-Bus,
user-computer work or permission bypass. GUI via CUA/Sky; independent native
AT-SPI form probe built from the merged tests/native/forms_atspi_probe.cpp.

## Actual results

- Lifecycle test: three construct/teardown cycles, exit0 (`lifecycle.*`).
- Two distinct fields: Name=文文🙂 and Project=跨端表单. Undo Project restored
  Native UI without changing Name; redo then undo Name restored Ada without
  changing Project; redo restored both Unicode values (`independent-undo-tree.txt`).
- Ctrl+S published Saved; named Reset both fields cleared both and published
  Unsaved changes; Ctrl+O restored both and published Loaded. Thus ordinary
  persisted action dirty-status behavior is verified in the final merged binary.
- Native AT-SPI client exit0 (`forms-atspi.*`): changed only fixture fields to
  名🙂A / 项目é; checked independent Unicode-scalar caret/selection, focus,
  screen origins (140,238)/(140,308), each character rectangle/point mapping,
  clipped-point rejection. Probe uses supported set_selection(index0), not the
  unsupported add_selection method. This is not an Orca test.
- Loaded saved Unicode values, titlebar mouse close exit0 (`first-run.*`).
  Restarted same binary and state file: both fields and computed preview restored
  (`restart-tree.txt`, `restart.log`); window-manager Alt-F4 exit0.
- Actual screenshots `form-wide.jpg` (1180x812) and `form-default.jpg` (760x520)
  were captured from separate actual form sessions. They are not generated images;
  size difference is the existing window-manager placement, not a tested resize gesture.
- Freshly rebuilt enabled-controls fixture exposed Increment disabled in AT-SPI.
  Pointer click had no effect; two Tabs kept focus on enabled Toggle. Toggled on,
  accessibility action incremented Count0→1; toggled off, pointer click left
  Count1. AT-SPI trees retained, clean Alt-F4 exit0 (`enabled-desktop.*`).
- Desktop released: no framework windows remain; QA-only terminal tab closed.

## Scope and retained limits

The two-field example has no shared-state mirror, no hidden persisted state,
no composition fixture and no collapsed/overlapping control fixture. Those edge
cases are covered by the root's 56-test automated suite, NOT asserted as real
system desktop proof here. Mirrors and hidden-field revision tests are synthetic.
No installed system IME, audible Orca, Windows/macOS, HiDPI or power claim.
The known intermittent X11 close anomaly remains OPEN despite these clean exits.
Terminal typeText first failed twice with “cannot identify focused text field;
no text was sent”; documented typeCharacters succeeded. No app failure hidden.
