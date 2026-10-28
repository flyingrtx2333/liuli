# v0.3 fixed-canvas declarative layout

`layout_compiler.py` validates `examples/window.layout.json` against the typed model and emits C++ `make_layout()` data. There is no JSON parser in the executable. Widget placement, labels, text/list bindings, button integer state/delta, and list scroll state/step come from declarations rather than demo IDs in the host.

This deliberately narrow schema supports `text`, `button`, `search` and `list`, one search field, and a fixed 1120 × 840 logical canvas. Window resizing scales this canvas; it is not responsive reflow, flexbox, constraints, CSS, or a general UI toolkit. Rectangles are half-open and must be inside the canvas. The last declared interactive widget wins hit testing. Binding/state types, unknown fields, duplicate keys/IDs, integer limits and source-size limits fail at compile time. Interactive rectangles may overlap intentionally.

Text uses Cairo's toy UTF-8 text API with bounded clipping; it does not implement shaped text, rich text, selection, or IME preedit. Search presentation takes the current state from the host. Widget text is clipped, not wrapped. The list draws as many rows as its fixed rectangle permits and labels any clipped runtime rows; wheel scrolling changes the declared offset state. The model still filters/copies the whole source collection, so this is not full data virtualization.

The existing v0.2 PNG renderer is preserved separately. The new window renderer builds one cached decorative background per thread, paints declared widgets into an owning native-endian ARGB32 pixel buffer, and hands that buffer to SDL for presentation. It is CPU-rendered, not a GPU material or Apple's Liquid Glass API. Each frame currently allocates/copies one pixel buffer. Accessibility, native macOS/Windows builds, high-DPI text rasterization, and power measurements remain unverified.

## CPU presentation and SDL surface acceleration

The window defaults to `SDL_FRAMEBUFFER_ACCELERATION=0` for its CPU surface path; an explicitly supplied environment value can override this for experiments. SDL's documented default may route a screen surface through streaming textures and a 3D engine; this prototype does not need that extra path. See [SDL2's official hint reference](https://wiki.libsdl.org/SDL2/SDL_HINT_FRAMEBUFFER_ACCELERATION). This is not a universal recommendation for GPU-based applications.

A same-binary cloud X11 observation, changing only the environment hint, measured settled RSS from 113976 KiB to 26548 KiB (about 111.3 to 25.9 MiB). The second process's peak RSS was 37960 KiB. Both short 2-second idle samples recorded zero additional CPU ticks at 100 ticks/sec. These are single-process observations, not statistically robust benchmarking, system-wide graphics memory accounting, battery measurements or proof against Electron. Raw records and binary identity are in `evidence/v0.3/`.

## v0.4 responsive flow

The window now defaults to `examples/responsive.layout.json` (layout version2). A tree of `row` and `column` containers lays out declared leaves at the actual client dimensions; resizing recomputes rectangles and rasterizes text/widgets at the new resolution. Only the cached decorative glass backdrop scales. Version1 rectangle documents remain supported, with rectangle scaling for compatibility.

Every node has `basis: [width, height]` and integer `grow`. On a row the width basis applies, on a column the height basis applies. Remaining main-axis space is distributed in proportion to grow weights; rounding is cumulative and deterministic. If basis sizes exceed the available space, they shrink proportionally to nonnegative sizes. Children fill the cross axis. Containers add `padding` and `gap`; both are bounded by actual space. This is an explicit deterministic flow contract, not CSS flexbox.

A row's `stack_below` switches its children to column flow when its inner width is below the declared threshold. Zero disables switching. The sample switches from side-by-side counter/list to stacked content; controls keep their declared heights rather than shrinking a screenshot. A column must use `stack_below: 0`.

Limits:200 total nodes,16 nesting levels,100 children per container, one search field,4096 pixels per surface axis. The normal window has a320×320 minimum; pure layout tests additionally cover1×1 and other undersized geometry. At constrained sizes content may be clipped or zero-sized, but computed rectangles remain inside the client bounds and do not overlap through negative sizing. Text remains clipped and single-line, not wrapped. No intrinsic text measurement, baseline alignment, min/max-content, CSS grid, DPI policy or accessibility layout tree is implemented.

`tests/test_responsive_layout.py` verifies narrow/wide breakpoint placement, deterministic output, bounds from1×1 to4096×4096, invalid constraints, depth limits and hit-testing alignment. The SDL smoke checks a resize to520×740, exact raster dimensions, stacked search placement and an actual hit at the recomputed button center.

## Bounded multi-input forms (current schema)

Both layout versions accept up to **16** single-line leaves in total, counting
`input` and legacy `search` together. `input` uses the same required `text` and
`state` properties as `search` and the same native widget kind. The text state
must initially fit 256 UTF-8 bytes and contain no ASCII controls, DEL, U+2028 or
U+2029; these are UI restrictions, not restrictions on general model text.
Multiple leaves may intentionally bind the same state.

`reset_states: ["offset"]` optionally names unique integer states to set to zero
atomically when that field's committed text changes. A generic `input` defaults
to no resets, so editing a form does not unexpectedly move an unrelated list.
For compatibility, an omitted `reset_states` on **legacy `search`** compiles to
all declared list-offset states. An explicit empty array disables those resets.
The policy is compiled into each widget; the host must not infer resets from all
lists at edit time. Integer targets need not have a list widget. This field is
rejected on non-input widgets, and unknown/non-integer/duplicate targets fail
compilation.

`examples/form.eui.json` and `examples/form.layout.json` define two independent
inputs, a computed preview, a named reset-both action and explicit persistence
for both states. This is deliberately not multiline text, passwords, a rich
editor, or a claim of platform IME certification. Earlier one-search limits in
this document describe historical milestones and are superseded by this section.
