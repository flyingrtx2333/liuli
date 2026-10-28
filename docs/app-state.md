# Bounded app-state storage

`native/state_store.h` is a synchronous, small storage seam, not a general filesystem capability, database, settings registry or application sandbox. Nothing is read or written until its caller explicitly supplies a path. The window host opts in with `--state-file PATH`; the application chooses the values it wants to persist. The storage core itself has no UI effects or implicit path.

## API

- `state_store::State`: ordered map of keys to `variant<int64_t,string,vector<string>>`.
- `encode(state)` and `decode(bytes)`: deterministic versioned serialization. Decoding builds a fresh map; it does not partially mutate an existing model.
- `load(path)`: returns a fully parsed state or throws.
- `save(path,state)`: validates all values before touching disk, then replaces the chosen path.

The host should validate loaded keys and types against its own allowlist, translate all permitted values into one `Model::apply` transaction, and only then replace UI editing state. Do not apply entries one by one. Missing keys, future versions or malformed files must not trigger silent partial restoration. Storage values are untrusted data, never executable code.

## Format and limits

New files without boolean values use header `EUISTATE 2\n`; files containing bool use `EUISTATE 3\n`. The reader accepts v1 integer/text, v2 integer/text/texts, and v3 including bool. Version 3 adds a `B` record with exactly `true` or `false` payload; see [typed conditions](conditions.md). Scalar records remain `I KEY_BYTES VALUE_BYTES\n` or `T KEY_BYTES VALUE_BYTES\n`, then exactly those key and value bytes followed by a newline. `END\n` terminates the file; trailing bytes are rejected. Integers are canonical signed decimal int64 values. Text is valid UTF-8, including embedded NUL/newlines, represented by lengths rather than escapes. Keys use ASCII identifiers beginning with a letter or underscore, followed by letters, digits, underscore, dot or hyphen.

Version2 additionally supports `A KEY_BYTES PAYLOAD_BYTES\n`, followed by key bytes, array payload, then one newline. Array payload is `COUNT\n` followed by each item as `BYTE_LENGTH\n`, exact UTF-8 bytes and one newline. Arrays contain only strings, never nested records. Empty arrays and empty items are distinct. Version1 rejects array records rather than misreading them. Counts and lengths are validated before reserving/copying item storage.

Maximums: 64 entries, 128 bytes per key, 4096 bytes per text value or array item, 1000 items per array, 65536 bytes for the whole file. The total limit applies across arrays and scalars together. Duplicate keys, malformed lengths, integer overflow, unsupported versions, invalid UTF-8 and truncation fail explicitly. This is not encrypted or authenticated storage; do not use it for secrets.

## Write and recovery semantics

The tested Linux/POSIX backend creates an exclusive temporary file in the same directory, writes and fsyncs it, closes it, then renames it over the destination. The old destination is never truncated. Temporary files are removed on ordinary exceptions; a process killed before cleanup may leave an unreferenced `.tmp.*` file. Files created by mkstemp are private to the user by default. No parent directory is created implicitly.

Errors before rename leave the original destination unchanged. Rename is the commit; there are no throwing operations afterward. The directory itself is **not fsynced**, so success is not a guarantee that a rename survives sudden power loss. Concurrent writers are last-successful-rename-wins, with no lock, version conflict detection or compare-and-swap. The caller must serialize writes if that matters. Parent directories must be trusted; this is not protection against hostile directory replacement. Replacing a symlink replaces that directory entry rather than following it. Linux loading rejects final symlinks and non-regular files.

Codec code is standard C++17. Linux file roundtrip/replacement is tested. Windows atomic save deliberately reports an unsupported backend rather than pretending std::rename overwrites existing files portably. Windows read fallback uses bounded binary stream reads and is not executed in this cloud. macOS POSIX behavior has not been tested. Platform-specific application data directories, native file dialogs, async I/O/cancellation, permissions prompts and migration are future integration work.

## Verification

`python3 -m unittest discover -s tests -p test_state_store.py -v` compiles a strict-warning C++ test and exercises empty/unicode/NUL/newline roundtrip, arrays, v1 compatibility, int64 extrema, exact total-size bounds, duplicate/malformed/truncated nested data, actual overwrite, missing-directory/write/rename failures, cleanup, directory/FIFO/symlink rejection and load failure leaving caller state unchanged. No user files are touched; fixtures live in a temporary directory.

## Generated opt-in persistence schema

The model JSON may declare `"persist": ["state_id", ...]`. Only declared mutable states may appear; duplicate, unknown and derived IDs are compile errors. Omitting the field exports no state. The counter example selects `count`, `query`, and `offset`, excluding its large source dataset; the task-list example selects its tasks, draft, selection and offset.

Generated `Model::persistent_state()` returns owning typed `Update` values for the selected fields. `restore_persistent_state(updates)` requires exactly those keys, each once, with the declared types, then routes all values through one existing atomic `apply`. Content validation, dependency evaluation and allocation failures therefore cannot partially restore state. Unknown, extra or missing fields reject rather than guessing migration policy. Captured values are independent of committed model storage.

`native/app_state.h` converts between this generated schema and `state_store::State` through `capture(model)` and `restore(model,state)`. It contains no counter/query/task-specific names. Text and array storage bounds still apply when encoding a save. Selecting more data than those bounds permits is an explicit save error, not a truncated file.

Widget policies are a separate host boundary: for example, a single-line editor must validate loaded text length/control characters and construct its candidate editing state **before** calling restore. The model's generic text type intentionally permits valid UTF-8 beyond the single-line editor's narrower policy. List scrolling limits likewise must not be inferred from a generic integer type.

Named actions also expose `prepare_action(id) const`, which evaluates all right-hand sides against the current committed snapshot and returns owning updates without mutation. A host may validate widget policies before one `apply`; `invoke_action(id)` remains the direct `apply(prepare_action(id))` convenience. Prepared actions are not a concurrency/version-control mechanism: callers must not delay them across unrelated updates while assuming a fresh snapshot.

A missing startup file is treated as a new-file destination, without creating it until an explicit Save. A malformed, unreadable or schema-rejected existing file instead blocks subsequent saves to that path, preserving the rejected original bytes. A successful explicit reload unblocks saves. No implicit recovery overwrite or repair occurs.

Closing while a requested save is pending defers ordinary window exit until that save succeeds. A save failure keeps the window open with a visible error. Ctrl/Command+Escape explicitly requests cancellation; already-committed writes cannot be undone, and a running filesystem call may still delay shutdown. There is no general unsaved-changes dialog or automatic save on close.
