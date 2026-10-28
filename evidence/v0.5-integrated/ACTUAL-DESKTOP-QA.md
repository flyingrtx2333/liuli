# Integrated actual desktop QA — 2026-10-27

Source HEAD: `5496b7d`. Binary: `build/a11y-integrated/eui-window`.
SHA-256: `7743cb9bcb9ba2c343841357e47cfced8cb6b9aa8bec9b927b80d9a72a9b4d87`.
This is new testing of the merged binary, not a restatement of isolated branch results.

## Environment and execution

Assistant-owned actual cloud X11 desktop, SDL X11 backend, process-local scoped no-SHM SDL and pinned official ATK/AT-SPI/bridge 2.62.1 runtime. `EUI_ACCESSIBILITY=1 G_DEBUG=fatal-warnings`; no global settings changed. `run-desktop.sh` and `run-probe.sh` record exact launch commands. Commands invoked via cloud Terminal; native input and window resizing via CUA/Sky. Source was not edited or rebuilt by QA.

## Results

- `lifecycle.log`, exit 0: three same-process bridge construction/teardown cycles, duplicate-provider rejection, invalid snapshot rejection.
- `probe.log`, exit 0: independently discovered application/roles, button action committed Count 0→1, focus, editable text, Unicode `中文🙂` length/caret 3 characters, filtered row, required events including deleted-text payload, retained removed row defunct, invalid newline edit rejected.
- Native keyboard: replace query with `Aé👨‍👩‍👧‍👦B`; two Backspaces yielded `Aé` (entire ZWJ family removed); third yielded `A` (combining grapheme removed).
- Native mouse drag selected `bcd` from `abcdef`; Ctrl+C, End, Ctrl+V yielded `abcdefbcd`. Undo yielded `abcdef`; redo yielded `abcdefbcd` (`undo-redo-tree.txt`).
- Ctrl+S published actual AT-SPI frame `Explicit UI / Saved` (`saved-tree.txt`). Deliberately malformed QA-only state file + Ctrl+O published `Explicit UI / Load rejected` without changing committed query (`error-tree.txt`). Restoring QA state and Ctrl+O yielded `Explicit UI / Loaded`; saved again successfully.
- Actual window resize 1120×840→519×840→1120×840. Native narrow layout stacks search/list below controls; wide layout restores columns. App-only genuine JPEG screenshots: `narrow.jpg`, `wide.jpg` (extensions verified).
- Window-manager close removed app from desktop inventory and provider exited 0. `provider.log` contains expected missing initial state, deliberately invalid single-line edit and deliberately malformed state rejection; no GLib warnings/criticals or X11 errors. Fatal-warning policy remained active.
- Read-only integration inspection: `editor_snap_graphemes(candidate)` remains before model mutation in `apply_editor`; ATK text methods use UTF-8 character length/offset conversion, and host converts ATK character offsets to editor byte offsets.

## Limits

This is Linux native/AT-SPI proof, not an Orca or audible screen-reader acceptance test, not macOS/Windows verification, and not a power benchmark. Orca/private-D-Bus work was not attempted in this QA. Bool language behavior is covered by root's separate 43-test suite; no visible checkbox or enabled-control feature is asserted by this run. Input via tool text insertion does not establish system IME behavior. Branch-only sanitizer results are not claimed as merged-binary sanitizer results.
