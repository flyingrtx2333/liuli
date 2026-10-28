# Compiled application actions

A version1 model may optionally declare `actions`, a list of strict `{ "id": "name", "set": { "state": expression } }` objects. IDs are unique ASCII identifiers, 1–64 characters, beginning with a letter. The compiler validates all assignment targets and expression types even when emitting only the legacy scripted executable. Unknown fields, derived-value write targets, duplicate action IDs and unknown references are rejected.

The persistent generated model exposes `bool invoke_action(const std::string& id)`. Each expression reads the same committed pre-action state. All resulting updates are staged before the existing `apply` performs one atomic transaction, propagates dependencies, and swaps its completed candidate. Any expression, allocation or apply failure leaves the committed state unchanged. The return value means a view value changed, not that an action existed or that all state was unchanged. Unknown action IDs throw `std::invalid_argument`; empty/equal-state actions return false.

Named actions are not automatically run by the legacy scripted executable. Its existing `transactions` retain their original behavior. Documents without `actions` remain valid. There is no runtime JSON evaluator, JavaScript or raw-code escape.

Two pure expressions support list applications:

- `append(texts, text) -> texts`: returns a new array with one appended value; rejects an input array already at 100000 elements using `std::length_error`.
- `remove_at(texts, int) -> texts`: returns a new array without the zero-based element; negative/past-end indices throw `std::out_of_range`.

These operators intentionally copy the changed array; immutable sharing avoids copying unrelated arrays but is not a persistent-vector implementation. A host must handle these recoverable action errors without corrupting the UI and must not call them through unchecked selection indices.

`examples/task_list.eui.json` demonstrates adding the current draft and clearing it in the same transaction, and removing the selected index. It is a model example, not yet a fully wired task-list application: no selection widget, blank-task policy or task-array persistence is claimed here. `state_texts(id)` exposes a const reference to a declared text-array state, with the same validity contract as the existing getters: callers must not retain it past a successful mutation.

Tests compile real C++ and verify simultaneous assignments, Unicode draft capture, empty/no-op behavior, unknown actions, bounds/size/overflow rollback, strict type checking and legacy scripted compatibility.

## Declarative button binding

Both layout versions accept action buttons with `text` and `action` instead of `text`, `state`, `delta`. The two forms are mutually exclusive. The layout compiler verifies the action ID against the model and emits `Widget.action`; legacy integer buttons emit an empty action string and retain their original fields. Hosts should call `invoke_action`, then synchronize any edited field whose state the action changed before repainting.

`examples/task_list.layout.json` is a responsive scene for the task model: editable draft, add button, list and a remove-first button. Until a selection widget is supplied, the model's selected index remains its initial zero; the label does not imply mouse selection support. It is a scene/model pair, not proof of persistence or complete application parity.
