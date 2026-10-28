# Linux accessibility spike (opt-in, not screen-reader acceptance)

## What exists

`EUI_LINUX_ACCESSIBILITY=ON` builds a real ATK provider; setting
`EUI_ACCESSIBILITY=1` starts it in the SDL host. The default build/launch is unchanged.
The provider uses the existing ATK → AT-SPI bridge, not synthetic accessibility
JSON and not a replacement D-Bus protocol implementation.

Semantic tree:

- Application → frame with the actual SDL title → labels, buttons, search, list.
- Buttons expose `AtkAction.click`, routed through the SDL main-thread queue.
- Search exposes a name from the sanitized label/placeholder (widget ID fallback), `AtkText` value,
  Unicode-character count/caret, selection read/set, caret set, `AtkEditableText`
  full replacement and `AtkComponent` focus request.
- Only rendered list rows are exposed, with `LIST_ITEM` roles. No Selection
  interface is claimed for the unselectable list.
- Component extents are captured from actual window position/layout. Window
  movement, resize, painting and focus changes republish semantic state.
- Status/title changes republish frame names even without a repaint (for example Saved or load errors).
- Changed focus, text, caret, selection, child and name properties emit ATK
  notifications. Removed rows become defunct; retained references remain safe.

Widget, row and frame IDs use disjoint namespaces; row components are length-prefixed; action targets
retain the original layout ID. Labels/rows/buttons use the same `display_text`
sanitizer as the renderer (including NUL/control replacement and bounded display
text), rather than accidentally truncating at a C-string boundary.

## Ownership and limits

The SDL thread remains the only owner of `Model`, `TextEdit`, the layout and the
window. It publishes immutable snapshots. One dedicated worker owns **the default
GLib main context**, all ATK objects and bridge callbacks. Attached one-shot
sources wake that worker; `g_main_loop_run` blocks between events. SDL requests
wake `SDL_WaitEvent`; there is no periodic polling timer.

This is an **exclusive-default-context integration contract**, not a general
GTK/GIO embedding API. The current host has no other GLib event loop or submitted
GLib async UI work; Pango shaping stays synchronous on the SDL thread. Do not add
GTK/GIO/default-context callbacks expecting the UI thread without redesigning the
integration seam. A future richer host should service GLib on the UI thread with
an event-driven SDL wakeup, or use independently owned contexts with a bridge that
correctly supports cleanup there. A private-context experiment exposed bridge
`g_source_remove` cleanup warnings; the tested spike does not use it.

Only one provider can exist per process. A lease rejects duplicate construction
before touching the first provider. During shutdown, descendants detach, become
defunct and release while the bridge registry is alive; the root stays alive
through bridge deregistration, then releases. The worker joins before SDL closes.

Snapshots are capped at 1,024 objects/1 MiB, with bounded fields. IDs must be unique
and parents must precede children. Existing IDs retain their role and parent.
Validation rejects malformed snapshots; callbacks contain C++ exceptions. Latest
snapshots coalesce, so this is current-state accessibility, not an event-history
replay system. The request queue is bounded at 64 entries; full text replacement
is checked at 256 bytes before constructing the queued string. The model performs
its normal UTF-8/single-line/transaction validation again on the UI thread.

**An action's return value means queued, not committed.** Full text replacement
has the ATK void-returning contract; clients must observe resulting text/events.
Rejected edits leave committed state unchanged. Requests queued during save-on-close
are discarded to preserve the save snapshot. Snapshot text offsets use Unicode
characters, while the editor uses UTF-8 bytes.

## Build and run

A coherent distro installation of **ATK, AT-SPI and atk-bridge >= 2.62.1** is
required for the normal route. Old distro bridges are not safe for this spike.
The normal pkg-config build enforces that minimum; custom/SDK runtime deployment
must still supply the coherent fixed trio. Before any provider, GLib context or
worker is created, supported ATK and AT-SPI runtime APIs reject versions below
2.62.1 (or missing/unparseable versions). The APIs are
[`atk_get_version`](https://github.com/GNOME/at-spi2-core/blob/main/atk/atkutil.h)
and [`atspi_get_version`](https://github.com/GNOME/at-spi2-core/blob/main/atspi/atspi-misc.h)
(the latter is available since 2.50).

**This is not a bridge-version attestation.** atk-bridge has no public runtime
version API. `atk_get_version()` independently describes ATK only; even successful
ATK/AT-SPI checks cannot certify an arbitrary mixed/custom bridge. Coherent fixed
packages are a deployment requirement. No runtime binary-hash/path heuristic or
unsafe bypass is used; the pinned helper below is the verified cloud route.
This conservative minimum also rejects older version-numbered backports; such
support would require review, not an environment override.

Normal Linux systems meeting that coherent-package requirement:

```sh
pkg-config --exists 'atk >= 2.62.1' 'atk-bridge-2.0 >= 2.62.1' 'atspi-2 >= 2.62.1' || exit 1
cmake -S . -B build/a11y -DEUI_LINUX_ACCESSIBILITY=ON
cmake --build build/a11y
build/a11y/eui-accessibility-runtime-check # ATK/AT-SPI APIs, not mixed-bridge proof
EUI_ACCESSIBILITY=1 build/a11y/eui-window
# In a separate process on the same existing desktop/session bus:
build/a11y/eui-atspi-probe
```

For this amd64 cloud development environment (no global install):

```sh
python3 scripts/prepare-a11y-sdk.py
cmake -S . -B build/a11y -DEUI_LINUX_ACCESSIBILITY=ON \
  -DEUI_A11Y_SDK="$PWD/build/a11y-sdk/root"
cmake --build build/a11y
python3 scripts/prepare-a11y-runtime.py
LD_LIBRARY_PATH="$PWD/build/a11y-runtime/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
  build/a11y/eui-accessibility-runtime-check
# Use the existing actual desktop/session; do not create/toggle global settings.
EUI_ACCESSIBILITY=1 \
LD_LIBRARY_PATH="$PWD/build/a11y-runtime/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
  build/a11y/eui-window
```

The cloud's separately diagnosed SDL MIT-SHM issue also requires its existing
process-local no-SHM SDL library path; see [`cloud-x11.md`](cloud-x11.md).
No `dummy`/`offscreen` run substitutes for actual display evidence.

The SDK helper extracts pinned official Debian 2.46 public ABI headers. The runtime
helper stages the matching ATK/AT-SPI/bridge **2.62.1-1 trio** under `build/`, pinned
by SHA-256. It requires compatible amd64 libc (>=2.38), GLib (>=2.80), D-Bus and
other shared dependencies; `ldd -r` found no missing symbols in this environment.
These libraries are selected only by the child process loader path, never installed
or substituted system-wide. Package license notices remain under
`build/a11y-runtime/usr/share/doc/*/copyright`; code is LGPL-2-or-later/2.1 as
specified per file in those notices. Binary packages/source are available from
[Debian's official source/package directory](https://deb.debian.org/debian/pool/main/a/at-spi2-core/).

## Confirmed upstream teardown defect, not suppressed

The system bridge's cache finalizer called `g_object_weak_unref` on hash-table
**values** (NULL) instead of object **keys**. A task-local tracing build established
that exact NULL argument; an earlier variant also caused a hash-table assertion.
Official Debian 2.56.2 source contains the faulty line. Upstream
[commit c4ddb104](https://github.com/GNOME/at-spi2-core/commit/c4ddb104fb25ba9b1f839cea28f280bb3a4146ba)
fixes precisely this error. The staged official 2.62.1 release contains that fix
and the later duplicate-cache-weak-reference fix
[8db3f580](https://github.com/GNOME/at-spi2-core/commit/8db3f580bd024095ddd86981b07f975575992c5d).
No log filtering, ignored X errors, skipped bridge cleanup or leaked provider is
used as a workaround. Before/after logs are retained in `evidence/v0.5-a11y/`.

## Evidence and remaining gates

See `evidence/v0.5-a11y/README.md`. Actual X11 proof includes independent libatspi
application discovery, role/name/value queries, counter activation, search edits,
Unicode caret/count, filtered rows, focus/text/caret/child events, rejected newline
edit, retained-row defunct state, and clean exit with `G_DEBUG=fatal-warnings`.
The independent client uses the system libatspi, not provider internals.

This is **not Orca acceptance** and not Electron accessibility parity. Missing:

- Actual screen-reader spoken output/navigation acceptance.
- Text character/range geometry and granularity queries; incremental insertion,
  deletion and clipboard editing through EditableText; complete selection APIs.
- Labels' full AtkText interface, authored human accessibility names/descriptions,
  relations, live regions, errors/status announcements, and useful window events.
- Proven multi-window/application-root lifecycle, OS-level focus activation,
  screen-coordinate correctness under mixed-DPI/multi-monitor/Wayland setups.
- Remote client held alive across process exit (retained **row removal** was tested;
  clean process teardown and same-process restart were separately tested).
- macOS NSAccessibility and Windows UI Automation providers.

The capability is opt-in specifically so these gaps cannot be mistaken for a
finished platform accessibility layer.
