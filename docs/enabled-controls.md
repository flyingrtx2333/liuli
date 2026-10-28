# Boolean-enabled buttons (experimental)

Both fixed (v1) and flow (v2) button declarations accept optional
`"enabled_view": "view-id"`. The compiler requires an existing **bool view**,
not a state ID or text/int view. Omitting it preserves always-enabled behavior.
It works with named actions and legacy integer-delta buttons. Other widget kinds
reject this field; checkbox and visibility bindings are not implemented.

The SDL host resolves enabled state from the committed model before painting.
Disabled buttons use a muted fill/label, are excluded from hit-testing and Tab
order, and cannot activate via click, Enter, Space or queued accessibility
requests. Disabling the focused button clears focus; Tab then starts at the
first enabled control (Shift-Tab at the last). Reenabling does not steal focus.
Separately, zero/nonpositive-area controls cannot receive focus or activation
and are skipped by Tab. A resize that collapses the focused control clears
focus on layout publication. ATK FOCUSABLE/SHOWING follow the area, while
ENABLED/SENSITIVE retain the model bool value; this is not a visibility binding.
The action path rechecks the model rather than trusting an old accessibility
snapshot. Direct Model actions remain callable: UI enabled state is **not an
authorization boundary**.

Resolution does not repaint or publish during the model/editor transaction.
An unchanged bool view does not add an extra repaint. Optional Linux ATK exposes
ENABLED, SENSITIVE and FOCUSABLE consistently and emits their transitions.
This is provider/API coverage, not an assertion of real screen-reader acceptance.

## Example

`examples/enabled_controls.eui.json` and
`examples/enabled_controls.layout.json` contain an always-enabled Toggle and a
conditionally enabled Increment button. Increment starts disabled.

Configure a separate CMake directory with:

```sh
cmake -S . -B build/enabled-controls \
  -DEUI_MODEL="$PWD/examples/enabled_controls.eui.json" \
  -DEUI_LAYOUT="$PWD/examples/enabled_controls.layout.json"
cmake --build build/enabled-controls
./build/enabled-controls/eui-window
```

The generic host's `--smoke` is specific to its default counter/list fixture;
use `python3 -m unittest discover -s tests -p test_enabled_controls.py -v`
for this example. Setting `EUI_A11Y_SDK` to the extracted SDK root additionally
builds/tests the host's stale accessibility queue guards without starting a bus.
The ATK tree test covers provider states and notifications.

Widget/ViewModel/AccessibleNode layouts changed: clean-rebuild all consumers.
