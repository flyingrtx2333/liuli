# Native snapshot preview

Run `bash scripts/render-preview.sh` from the project (or invoke it by absolute path).
Requires Python 3 for compilation/build-time data conversion, GNU-compatible C++17,
and the installed Linux Cairo runtime `libcairo.so.2`. No dependency download occurs.

The script compiles the example into a standalone native executable, executes its
scenario, and captures `build/preview/snapshots.jsonl`. It selects the actual `initial`
and `batch_filter_scroll` checkpoints, converts their JSON values into a generated
C++ include, then compiles and runs the Cairo renderer. All counter text, list rows,
patch counts, materialization counts and evaluation totals come from those captured
runtime snapshots. Labels and presentation styling are renderer-owned chrome.

Outputs in `build/preview/`:
- `before.png`, `after.png`: 1200 x 900 native CPU-rasterized images.
- `selected-snapshots.json`: the exact source snapshots for visual verification.
- `preview_data.inc`: safely escaped C++ literals generated from those snapshots.
- `runtime` and `cairo-preview`: separate native executables.

The first checkpoint has Count: 0 and the initial list window. The second reflects
the scenario's real increment, filter and offset changes. The renderer displays up
to 12 rows; larger windows are explicitly labeled as clipped, not silently hidden.
Metrics are cumulative and named accordingly. No performance values are invented.

## Boundaries

This is a **snapshot renderer**, not interactive runtime integration. The generated
state program and renderer run separately; Python is a build-time JSON bridge, not
a UI runtime. PNGs are exported native raster images, not screenshots of a desktop
window. There is no display server, event loop, input handling, accessibility tree,
GPU path, cross-platform validation, power measurement or frame-time claim.

Cairo's toy text API is adequate for this English demo, not a full international
text/layout/IME stack. Text is clipped to its box. System font availability can
change raster output. The build uses a writable project-local font cache.

`native/cairo_abi.h` uses installed official headers when present, otherwise a small
public ABI subset with opaque structs and exact signatures/enums. This workaround
is deliberately limited to this prototype. Versioned `-l:libcairo.so.2` linkage is
GNU/Linux-specific. A distributable backend should use official development headers
and normal platform dependency discovery rather than grow this shim.

## Verification

The script checks Cairo context/surface/PNG-write statuses, PNG signatures and
1200 x 900 dimensions, source view differences and differing before/after output.
PNG decode and visual inspection should supplement these checks; image differences
alone do not prove correct state binding. `selected-snapshots.json` makes that
binding inspectable without relying on the pixels alone.
