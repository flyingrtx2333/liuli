# Accessible text geometry: own-cloud proof, 2026-10-27

Branch: `accessible-text-geometry`, based on `5496b7d` (not merged into main here).
See `../../docs/accessible-text-geometry.md` for methods and explicit limits.

## Reproduction

Build with CMake, `-DEUI_LINUX_ACCESSIBILITY=ON` and the task-local SDK.
`run-provider.sh` and `run-probe.sh` are exact cloud-desktop-session commands.
They use the existing session accessibility bus, never a new private D-Bus session.
Both scope `G_DEBUG=fatal-warnings` and coherent task-local 2.62.1 dependencies.
Provider uses main's pinned noSHM SDL launcher; no installed files/settings changed.

## Results

* `check.log` / `check.exit`: full 44-test Python suite, strict scripted/live builds, exit 0.
* `geometry-tests.log`: focused real Pango + renderer snapshot tests after final arithmetic hardening, 2/2 passed.
* `ctest.log`: final accessibility-tree test, scripted and synthetic-window tests, 3/3 passed.
* `binaries.sha256`: exact final provider/client binaries used for both final desktop probes.
* `probe-final-wide.log`, `xwininfo-final-wide.txt`: 1180x812 at client origin (92,120), all checks passed.
* `probe-final-narrow.log`, `xwininfo-final-narrow.txt`: titlebar moved and right border dragged; 488x840 at (219,164), all checks passed.
* `provider.log` / `provider.exit`: final provider shutdown via **titlebar mouse close**, exit 0; deliberate newline-edit rejections are expected.
* `wide.png`, `narrow.png`: final frozen binary screenshots, not mockups.

The independent libatspi client verifies Unicode/codepoint counts, Chinese/emoji
character rectangles; combining/ZWJ grapheme hits; pure and mixed RTL coordinates;
word-start whitespace boundaries; character/line/paragraph strings; focused scroll,
unfocused origin, clip rejection, actual screen/window transforms and existing
focus/action/text/event/retained-row behavior. `xwininfo` independently supplies
client origins; relative decoration offset (4,32) is observed, not assumed.

Only 1x cloud X11 tested. No Orca, private D-Bus, multiline, IME, HiDPI, other-OS,
full screen-reader parity or Electron-replacement claim.
