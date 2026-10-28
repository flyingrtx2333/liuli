# Pango/Cairo single-line text backend

`native/shaped_text.h/.cpp` uses installed Pango/Cairo rather than treating UTF-8 bytes as individually drawn glyphs. Pango supplies shaping, font fallback and bidi layout. The wrapper owns a Pango layout and font-description resources through RAII; layout and returned selection allocations are released. This is not a bundled font engine.

## Contract

`ShapedText(cairo_t*, text, pixel_size, bold)` creates a single-line layout using the `sans` font family and absolute pixel size. The Cairo context must be valid and remain configured consistently for measurement and drawing. Inputs are strict UTF-8, at most 64 KiB, with no ASCII controls, NUL or Unicode line/paragraph separator. Font size must be finite in 1..256 pixels. No markup is parsed.

- `width()/height()` are logical pixel dimensions.
- `caret(byte_index)` returns the **strong** cursor rectangle. The weak bidi cursor is not exposed. Indices must be UTF-8 codepoint boundaries.
- `hit_test(x)` delegates to Pango, then converts its grapheme trailing-character count to a validated byte offset. Coordinates outside the line are clamped by Pango; bidi visual order is respected.
- `selection_rects(start,end)` returns one or more visual rectangles for a logical byte range. Bidi selection may have disjoint ranges.
- `draw(cairo_t*,x,y)` uses a **top-left** origin, caller source color and clip. It is not a baseline coordinate.
- `unknown_glyphs()` allows the actual installed font coverage to be checked rather than assumed.

The editor's arrow/deletion semantics remain a separate contract. Rendering a joined emoji correctly does **not** prove that codepoint editing will delete it as one grapheme. Platform IME composition, selection navigation direction, accessibility, line wrapping and full document editing are not provided by this wrapper.

## Proof and dependencies

`tests/test_shaped_text.py` compiles the real wrapper with strict C++17 warnings and invokes installed libraries. It checks Chinese and Arabic/Hebrew glyph coverage, valid byte geometry, bidi range generation, empty text, invalid UTF-8/indices/limits and repeated construction/destruction. `evidence/v0.4/shaped-text.png` is the actual Cairo output, visually inspected for Chinese, mixed bidi, combining accent and joined emoji. This is not a memory-leak benchmark or system IME test.

The graphics builds require pangocairo, Pango, GObject and GLib in addition to Cairo/SDL2. Standard vendor headers are preferred. The existing Linux environment without development headers uses a small public opaque ABI declaration subset and explicit shared runtime links. Other platforms still require actual dependencies and build/run validation. `EUI_CONSOLE_ONLY=1` skips this graphics test and keeps the model-only path independent.

Official API references:
- [Pango cursor positions](https://docs.gtk.org/Pango/method.Layout.get_cursor_pos.html)
- [Position to UTF-8 index and trailing grapheme count](https://docs.gtk.org/Pango/method.Layout.xy_to_index.html)
- [Logical selection to visual ranges and ownership](https://docs.gtk.org/Pango/method.LayoutLine.get_x_ranges.html)
- [PangoCairo layout creation](https://docs.gtk.org/PangoCairo/func.create_layout.html)
