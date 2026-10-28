# Bounded single-line forms

`input` is a plain single-line editor; `search` is its compatibility alias with
legacy list-reset defaults. Both use the existing native search editor widget.
There are at most 16 fields per layout and 256 UTF-8 bytes per field. Controls,
DEL, line/paragraph separators and invalid UTF-8 are rejected on initial values,
edits, actions and restores. This is not a password, multiline or rich-text API.

Each widget owns its caret, selection, preedit and bounded undo/redo stacks.
Changing focus cancels the old field's transient preedit; it does not commit it.
Editing one field leaves unrelated editor buffers, selection and history untouched.
If multiple widgets bind the same state, their committed text is synchronized in
the same transaction. The initiating editor retains its own undo history; peers
whose text changes reset selection to the end and clear their now-stale history.
Action/load replacements likewise reset only changed fields (or a field whose
composition is being cancelled), preserving unrelated editor state.

Updates stage affected editors and validate list offsets before committing the
model once; editor moves after the model commit are statically checked noexcept.
No rendering/ATK publication occurs between model and editor publication. Invalid
multi-field actions/restores leave both model and all editor states unchanged.
Rendering or allocation failure outside the validated input transaction is fatal,
not reported as an ordinary input rejection.

Plain inputs have no implicit list-reset behavior. `reset_states` explicitly
lists integer states to set to zero in the same edit transaction. An unchanged
committed value (caret/selection/preedit-only edit) never resets them. Legacy
search declarations without the property materialize all layout list offset
states as reset targets. Set `reset_states: []` to opt out, or migrate to input.

Ctrl+S during nonempty preedit is rejected visibly: finish/cancel composition and
press Ctrl+S again. Committed changes mark the title Unsaved changes (a conservative
notification, not a full saved-baseline diff); active composition is identified
separately. No synthetic IME commit, automatic delayed save or promise to
save transient text is made. Async stale-load guards cover edits to any field.
Existing snapshot-save, corruption protection and save-on-close semantics remain.

The ATK provider publishes each field's own text, selection and Pango geometry.
Real platform IME certification and complete screen-reader acceptance remain open.

Build the two-field persisted sample with:

```sh
cmake -S . -B build/forms -DCMAKE_BUILD_TYPE=Release \
  -DEUI_MODEL="$PWD/examples/form.eui.json" \
  -DEUI_LAYOUT="$PWD/examples/form.layout.json"
cmake --build build/forms
./build/forms/eui-window --state-file /path/to/form.state
```

The sample has a computed preview, a named atomic reset action, and an explicit
persistence schema. It does not automatically save edits or autosave on close.

## Persistence revision and status

Generated `Model.persistence_revision()` starts at zero and advances once per
successful transaction iff the **final** values of declared persisted fields
change. It reuses the already-computed state dirty flags, including immutable
text/array comparisons; it does not capture or encode the persistence payload on
every edit. Revision is stored in the staged Data and published by the same model
swap, so failed transactions and final-value no-ops leave it unchanged. Overflow
is rejected before publication. Nonpersisted changes do not advance it.

The host records the revision on successful load/save. A hidden persisted state
changed by an action marks the file dirty even when Model.apply returns false
(no visible view changed). Nonpersisted changes and caret-only movements do not
make stored data dirty. Undoing back to saved content still advances the revision:
Unsaved changes is conservatively modified-since-save, not an expensive full
snapshot equality check. Explicit successful load/save resets that baseline.

An in-flight save compares its captured persistence revision on completion and
also checks every field for transient composition. It reports Saved only when
both match/no composition is active; otherwise it reports Saved snapshot with
newer edits pending or text composition pending. Broad UI generation remains the
separate stale-load guard. No implicit IME commit is performed.
