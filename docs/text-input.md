# Native single-line text input milestone

Implemented in the Linux SDL/Pango backend, not an Electron replacement claim.

- Left/Right use Pango strong-caret visual movement (including RTL); Shift extends the logical byte-range selection. Home/End mean logical start/end. Mixed bidi selection may paint multiple rectangles; no weak-caret toggle or separate directional affinity.
- Backspace/Delete remove a full Pango grapheme cluster, including combining accents, joined emoji, modifiers and flag pairs. This deliberately uses full-cluster deletion rather than language-specific single-character Backspace behavior.
- Mouse click places the caret at Pango's shaped hit position. Drag and Shift-click extend selection. Font, horizontal scrolling origin and strong caret agree with rendering. Drag scrolling is driven by pointer motion; stationary-edge timer autoscroll is not implemented.
- Ctrl/Cmd+Z undoes; Ctrl/Cmd+Shift+Z or Ctrl/Cmd+Y redoes. Each committed text edit is one snapshot (no typing coalescing). History is capped at 64 snapshots per stack; each text is bounded by the editor byte limit (256 in this host). New edits discard redo. External reset/load clears history. Caret/selection are restored; preedit is never a committed history entry.
- Host changes stage a copied editor, normalize positions to Pango grapheme boundaries after text changes, apply the model transaction, then publish the editor. Rejected validation/model changes do not publish history or caret changes.
- SDL preedit remains transient, editing keys/mouse positioning are ignored during active composition, and SDL TEXTINPUT commits once. **No system Chinese IME was tested.** Existing synthetic preedit tests are not an IME certification.

## Verification (2026-10-27)

`python -m unittest discover -s tests -v`: 39 tests passed. Expanded tests cover grapheme boundaries, RTL visual movement, bounded undo/redo, preedit history, clipboard/cut, mouse drag, scrolled hit geometry and transaction rejection. The insertion-before-combining-suffix regression was additionally rerun in the host test after the full-suite run.

Real cloud X11 desktop, native counter/list app and process-local no-SHM SDL (same cloud compatibility workaround as v0.4):

1. Entered `中文👩‍💻🇨🇳é` in Mousepad, copied with Ctrl+C and pasted into native search with Ctrl+V. `unicode-paste.png`.
2. Backspace removed the complete combining `é`; Shift+Left selected the complete flag. `flag-selected.png`.
3. Deleted flag, undid and redid; dragged from start through Chinese and joined emoji. `drag-selection.png`.
4. Clicked start, Shift-clicked after Chinese, cut; joined emoji remained intact. `shift-click-cut.png`.
5. Undo restored Chinese and the selection. `undo-restored.png`.

Screenshots are in `evidence/text-input/`. Desktop actions used actual window-system events and clipboard, not the dummy driver. The persistence title reached Saved during QA, but the screenshots do not substitute for byte-level state inspection. The existing persistence tests independently verify the store.

## API references

- [Pango LogAttr](https://docs.gtk.org/Pango/struct.LogAttr.html): `is_cursor_position` supplies Unicode grapheme boundaries.
- [Layout logical attributes](https://docs.gtk.org/Pango/method.Layout.get_log_attrs_readonly.html): character-indexed attributes, converted to UTF-8 byte offsets.
- [Visual cursor movement](https://docs.gtk.org/Pango/method.Layout.move_cursor_visually.html): strong-caret direction and trailing character offset conversion.

Portable `TextEdit::move_left/right`, `backspace` and `erase_forward` retain their explicitly tested codepoint semantics for compatibility. The native host does not call those methods; it uses Pango-backed adapters.

## Retained limits and data contracts

Core positions and preedit selection offsets are UTF-8 bytes, not character counts. Invalid UTF-8, interior-codepoint positions and excessive byte length are rejected before mutation. The generic core permits control characters; the single-line host rejects controls, NUL and line separators. SDL allocates its returned clipboard string before this host can enforce its bounded field policy; clipboard reads occur only on explicit Paste, not by polling.

The SDL adapter converts TEXTEDITING codepoint counts to byte offsets. The legacy fixed-size SDL composition event is supported, not extended composition events or arbitrarily long preedit. Focus loss cancels preedit. Candidate-window behavior, CJK IME commit/cancel and platform IME integration remain unverified. The caret is steady to avoid an idle timer; glyph availability depends on installed fonts. There is no Unicode normalization of stored text.

Initial field values now pass the same single-line/256-byte policy at layout
compilation, with a defensive host check before window creation. General model
text can still contain controls; it is the input widget that imposes this policy.
