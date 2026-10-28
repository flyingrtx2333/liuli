# Linux single-line accessible text geometry

Opt-in continuation of the ATK spike, not screen-reader parity. Build/runtime setup
is unchanged: see `linux-accessibility-spike.md` (coherent fixed 2.62.1 runtime).

## Added methods and offsets

* `AtkText.get_character_extents`: normalized nonnegative Pango grapheme rectangles,
  outward-rounded to integer pixels, indexed by Unicode character offsets.
* `AtkText.get_offset_at_point`: Pango's character/grapheme-leading index at the
  requested integer pixel, converted from UTF-8 bytes to Unicode characters.
  It is **not** the editor's nearest-caret/trailing-edge hit test. Combining/ZWJ
  members need not each have distinct geometry or round-trip to their own offset.
* `AtkText.get_string_at_offset`: CHAR (one Unicode character), WORD (Pango word
  start at/before offset through next word start, including intervening spaces),
  LINE and PARAGRAPH (entire committed single line). Invalid offsets and unsupported
  SENTENCE return NULL with offsets -1. No legacy before/after boundary methods.

SCREEN, WINDOW and PARENT coordinates are supported. WINDOW uses the frame's
**client origin**, not the window manager decoration origin. Offscreen character
rectangles remain available; point lookup returns -1 outside painted text/clip.
No range extents, attributes, bounded ranges or scroll-substring implementation.
Empty text has no valid character offset to query.

## Thread and bounds contract

`editor_text_geometry` runs on the SDL/UI thread. It uses the same 16-pixel sans
`ShapedText` and `editor_origin` as the renderer. The unfocused renderer has a
separate unscrolled origin/baseline, reproduced explicitly. Active preedit disables
geometry rather than misrepresenting committed text as the composed display.
Word boundaries still describe committed text. This is not an IME-support claim.

`TextGeometry` owns only integer rectangles, compressed integer-x hit spans and
word-start character offsets. No Pango, Cairo, model, renderer or SDL object crosses
to the accessibility worker. The worker performs lookup/conversion only.
Pango is sampled at each integer x in the visible clip, then equal adjacent results
are compressed. Work is bounded by the existing 4096-pixel layout width; it runs on
snapshot publication, never on an idle polling timer.

Limits validated before publication: 256 UTF-8 bytes and at most 256 character
rectangles, 4096 hit spans, 257 word boundaries per entry. Geometry payload bytes
join strings in the existing 1 MiB snapshot budget (1024 semantic nodes). Invalid
counts, rectangles, overlapping hit spans, offsets or word ordering are rejected.

## Evidence and scope

`evidence/text-geometry/` records independent libatspi calls on the real cloud
X11 desktop using task-local noSHM SDL and fixed 2.62.1 ATK/bridge/AT-SPI,
`G_DEBUG=fatal-warnings`. `xwininfo` independently measured client origin (92,120),
relative decoration offset (4,32), 1180x812 client size. After a real titlebar drag
and border resize: client origin (219,164), 488x840. AT-SPI frame/text screen/window
transforms matched both measurements; no guessed decoration correction was added.

The probe checks Chinese/emoji character count and geometry, combining/ZWJ hits,
RTL order/positive widths, whitespace word ranges, line/paragraph ranges, focused
horizontal scroll, unfocused origin and clip rejection. Pure tests cover capture,
preedit-unavailable geometry, invalid/bounded snapshots and overflow-safe geometry
coordinate conversion. Only this 1x cloud display is verified; no HiDPI, Wayland,
macOS/Windows, multiline, full bidi caret affinity, Orca or full AT parity claim.

## Primary references

* [ATK string-at-offset semantics](https://docs.gtk.org/atk/vfunc.Text.get_string_at_offset.html)
* [ATK point lookup](https://docs.gtk.org/atk/method.Text.get_offset_at_point.html)
* [Pango index-to-position and RTL rectangles](https://docs.gtk.org/Pango/method.Layout.index_to_pos.html)
* [Pango point-to-byte-index and trailing caret information](https://docs.gtk.org/Pango/method.Layout.xy_to_index.html)
* [GTK 3 entry accessibility reference implementation](https://raw.githubusercontent.com/GNOME/gtk/gtk-3-24/gtk/a11y/gtkentryaccessible.c)
