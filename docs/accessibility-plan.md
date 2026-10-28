> Update: the opt-in Linux [ATK/AT-SPI spike](linux-accessibility-spike.md) now has actual X11 programmatic evidence. This does not close Orca or cross-platform acceptance gates below.

# Native accessibility bridge: next parity gate

Status: investigation only, 2026-10-27. No provider is implemented by this document.
Keyboard navigation and Pango text shaping are useful prerequisites, **not a native
accessibility tree or screen-reader acceptance**.

## Decision

For the existing Linux SDL/Cairo host, implement a small ATK object tree and use
`libatk-bridge-2.0` to export it to AT-SPI. Do not hand-write the D-Bus protocol;
`libatspi` is useful for client inspection/tests, not a substitute for the provider.
ATK supplies object semantics and interfaces including Action, Component, Text and
EditableText. [ATK overview](https://gnome.pages.gitlab.gnome.org/at-spi2-core/atk/),
[AT-SPI client library](https://gnome.pages.gitlab.gnome.org/at-spi2-core/libatspi/).

Do not introduce GTK merely to obtain a bridge. GTK4's Accessible interface cannot
currently create an independent top-level accessible; it requires an accessible
parent. Adopting a GTK window would be a separate host redesign, not a drop-in
adapter for this SDL window. [GTK Accessible](https://docs.gtk.org/gtk4/iface.Accessible.html).

## Observed environment, not assumptions

- Own-cloud shell exposes runtime `libatk-1.0.so.0`, `libatk-bridge-2.0.so.0`,
  `libatspi.so.0`, GTK3, GObject and GIO. `nm -D` confirms bridge init, cleanup and
  event-context symbols. Runtime versions were not established by package queries.
- ATK/AT-SPI headers were not found in `/usr/include`; pkg-config cannot resolve
  `atk`, `atk-bridge-2.0`, `atspi-2` or `glib-2.0` in the inspected exec environment.
  A proper development SDK is needed: do not repeat handwritten ABI declarations
  for GObject classes, vtables or interface layouts.
- Exec shell has no DISPLAY or session-bus variable. The **actual cloud desktop**
  has `DISPLAY=:0`, a session bus, and running AT-SPI bus-launcher/registry processes.
  Evidence: [`desktop-a11y-environment.txt`](../evidence/v0.4/desktop-a11y-environment.txt).
  This establishes service presence, not successful application registration.
- `orca` and `accerciser` were absent from PATH in both inspected contexts.
  `gdbus` and `dbus-send` exist. No installation, service start, accessibility setting
  or desktop-session change was performed.

Immediate blockers: supported dev headers/build metadata; then real provider code
and client tests; then a usable screen reader for end-to-end acceptance. A missing
screen reader does not block implementing and testing the provider API.

## Smallest coherent implementation

### 1. Semantic snapshot and host seam

Add a platform-neutral snapshot built from compiled widgets and committed model
state, not pixels. Current `Layout::widgets` is flattened and has stable widget
IDs, kind and bounds; `text`, `button`, `search`, `list` cover the first slice.
Use application → window → widgets → list rows. Ignore decorative flow containers.

| Widget | Native semantics | Operations |
|---|---|---|
| Window | application/frame, title, active state | focused child, child navigation |
| Text | label with resolved displayed string | read name; Text if content needs richer navigation |
| Button | push button, resolved label, enabled/focusable | Action invokes existing `activate` |
| Search/editor | single-line entry, explicit accessible name, editable text | Text + EditableText, caret, selection, focus |
| List | list containing named list items | child navigation; no Selection interface unless selection actually exists |

Expose focused/enabled/sensitive/showing/editable states honestly. Name is not the
entry's current value or placeholder. Add an explicit label/description field or
relation where the current schema lacks an unambiguous name. Do not invent controls
or claim unsupported actions. `AtkValue` is for numeric value semantics, not a
replacement for an entry's Text/EditableText interface.

Rows need identities stable across filtering/reordering; current widget IDs alone
do not solve dynamic row identity. Keep offscreen rows discoverable where feasible
but mark them not showing; provide an honest scroll-to mechanism before claiming
complete long-list usability. Recompute screen-coordinate bounds after resize,
move, scale or scrolling, with correct clipping and hit-testing.

### 2. ATK provider and lifecycle

Implement GObject/AtkObject subclasses with required interfaces. Supply the root
and toolkit metadata through `AtkUtil`; retain the default global event-listener
machinery. [AtkUtil](https://gnome.pages.gitlab.gnome.org/at-spi2-core/atk/class.Util.html).

Create the application root before `atk_bridge_adaptor_init`; the bridge explicitly
calls `atk_get_root`, registers interfaces, connects its bus and registers the
application. Check the result. Use the public `atk-bridge.h`, not private bridge
structs. Cleanup on exit; preserve object references while clients hold them and
mark removed nodes defunct rather than leaving dangling widget pointers.
[Bridge implementation](https://raw.githubusercontent.com/GNOME/at-spi2-core/main/atk-adaptor/bridge.c),
[public entry points](https://raw.githubusercontent.com/GNOME/at-spi2-core/main/atk-adaptor/atk-bridge.h).

Use the actual desktop session environment. Do not create an unrelated private bus
and describe it as desktop integration. Respect disabled accessibility; report
connection failure without crashing the app. Service presence alone is insufficient:
verify that an independent AT-SPI client can discover this application's root.

### 3. Dispatch and editing correctness

The current host blocks in `SDL_WaitEvent`. ATK bridge traffic requires a serviced
GLib main context. First implement a small integration spike demonstrating an
external Action call wakes an idle window. Keep provider callbacks, model mutation
and notifications on the UI owner thread. Choose an event-driven integration
(GLib sources plus SDL wakeup/dispatch, or a poll-source integration with a proven
wakeup path); do not add a permanent fast timer or run model callbacks on an
uncoordinated worker. Test shutdown and synchronous client queries for deadlocks.
The bridge exposes `atk_bridge_set_event_context`; validate ordering and behavior
against the selected runtime before relying on a non-default context.

Route actions through `activate`; text changes through `set_query`/`apply_editor`
and existing validation/atomic commit paths. Never bypass UTF-8, size, single-line
or state validation. Publish semantic events only after a successful commit.
Update focus from the same host `focus` path. Emit text insertion/removal, caret,
selection, focus and child-change events as appropriate, with no unchanged-frame
notification flood.

ATK Text uses character offsets while this editor stores UTF-8 byte offsets.
Provide tested conversion and Pango-derived character geometry; byte offsets,
Unicode characters, graphemes and glyph clusters are not interchangeable.
Committed text and temporary IME composition need distinct handling.
[AtkText contract](https://gnome.pages.gitlab.gnome.org/at-spi2-core/atk/iface.Text.html).

## Acceptance gates

1. **Provider API:** independent libatspi test discovers application/window,
   verifies exact roles/names/parent-child structure and state, changes entry text,
   invokes Add, and verifies committed model and visible list agree. Rejected edits
   leave both unchanged. Include Chinese, combining marks and emoji.
2. **Events/lifecycle:** independently record focus/text/caret/children events;
   test filtering, removed rows, resize, window move, focus loss and app close.
   Retained client references must not crash after deletion. Test unavailable bus.
3. **Idle cost:** idle AT-SPI client attached and detached; record CPU/wakeups and
   prove the bridge does not turn the sleeping host into a polling render loop.
4. **Real screen reader:** with Orca on the actual session, read title and labels,
   enter/edit a Unicode task, activate Add, inspect/scroll the list, save/reopen and
   read restored content. Include keyboard-only operation. Record versions and
   observable spoken/braille output. A tree inspector passing is not this gate.

Suggested first implementation deliverable: window + named entry + Add button +
list rows, external AT-SPI Action/Edit tests, and lifecycle tests. Mark long-list
scroll navigation, full text geometry or reader behavior as incomplete if deferred.
No claim of Electron accessibility parity until these gates and corresponding
platform tests pass.

## Other native platforms (planning only)

**macOS:** represent custom-drawn controls using `NSAccessibilityElement` children
of the native containing view; expose role, label, value, parent/children, frame,
focus and actions, and post accessibility notifications. Use role-specific
protocols and editable value setters where supported. Validate with VoiceOver,
including text navigation; the Linux provider is not reusable as a native macOS
bridge. [Apple custom-control guide](https://developer.apple.com/library/archive/documentation/Accessibility/Conceptual/AccessibilityMacOSX/ImplementingAccessibilityforCustomControls.html)
(archived official guide; verify SDK declarations during implementation).

**Windows:** custom controls need UI Automation providers. Implement
`IRawElementProviderSimple`, plus Fragment/FragmentRoot for the virtual tree;
expose the root through `WM_GETOBJECT`, provide properties, appropriate Invoke,
Value/Text patterns, navigation, bounds and focus, and raise UIA events. Validate
using an independent UIA inspector and Narrator on Windows; no Windows bridge is
implemented by this plan. [Provider overview](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-providersoverview),
[server-side implementation](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-serversideprovider).
