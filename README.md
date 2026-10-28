# Explicit UI v0.6 — experimental native reactive compiler

**Provisional name. This is a working, small compiler experiment, not an Electron replacement.**
The initial target is predictable typed state propagation with a bounded list view, before committing to a graphics/platform stack. Authoring syntax prioritizes explicit machine-checkable structure over brevity.

The current tree adds responsive flow layout, immutable shared model storage, compiled actions, strict booleans/lazy conditionals, Pango grapheme editing with pointer selection/undo, opt-in typed persistence and an experimental Linux ATK bridge. See `docs/electron-replacement.md` for acceptance gates. Automated checks and actual desktop checks are separate: `evidence/v0.4/` records diagnosis attempts and final Linux acceptance. The v0.4 source-frozen aggregate passed39 tests. The merged v0.5 checkpoint passed43 Python tests and3 native CTests, plus real X11/AT-SPI editing, events, status-name and close tests; see `evidence/v0.5-integrated/`. This is a verified Linux slice, not full Electron or three-platform parity. Earlier v0.2/v0.3 memory measurements apply only to their recorded binaries.


The v0.6 source adds bounded multi-input forms, independent/mirrored editor state,
strict button-enabled bindings, text geometry and persistence revisions independent
of visible view changes. Source commit `3007dde` passed56 Python tests and2
applicable custom-form CTests. See `evidence/v0.6-integrated/` for exact final desktop
coverage and limitations; older branch screenshots are not proof of this binary.
Qt Widgets was evaluated separately but is not included in the default backend:
rejected native edits can lose redo history, so a model prepare API alone does not
establish equivalent transactional editing.

## Run in the cloud workspace

Requires Python 3.9+ and a C++17 compiler; no Python dependencies or web runtime. The graphics paths require Cairo and Pango; the native window additionally requires SDL2. On Linux the installed shared libraries can be used when development metadata is unavailable. Use `EUI_CONSOLE_ONLY=1 ./scripts/check.sh` for dependency-free core validation.

```sh
cd explicit-ui
./scripts/check.sh
# Or individually:
mkdir -p build
python3 compiler.py examples/counter_list.eui.json -o build/demo.cpp
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror build/demo.cpp -o build/demo
./build/demo
```

`build/metrics.jsonl` holds actual native executable output. `CXX=clang++ ./scripts/check.sh` selects another compiler if installed. The default code and shell workflow were tested in the existing Linux cloud workspace, not the user's computer. No remote repository, deployment, or saved Codex environment was created.

A minimal **unbuilt** container recipe is supplied:

```sh
docker build -t explicit-ui .
docker run --rm explicit-ui
```

The Dockerfile uses Debian's Python, g++, Cairo/Pango/SDL2 runtimes and fonts; the recipe requires network package access and Docker. It is not evidence of a tested container image. A future saved Codex configuration can import this project and run `./scripts/check.sh`; none has been provisioned by this prototype.

## Implemented slice

- JSON AST is the v0 declarative language. Unknown keys, duplicate names/JSON keys, unknown/forward references, wrong types and unsupported expressions are rejected before C++ generation.
- Static types: signed 64-bit `int`, strict `bool`, UTF-8 `text`, `texts` (text array). Booleans/floats are not integers. Pure operators include `add`, `concat`, `to_text`, `filter`, `window`, `append`, `remove_at`, comparisons and lazy `if`/`and`/`or`; see [conditions](docs/conditions.md).
- The compiler derives direct dependencies from expressions. Declaration order must be topological; cycles/forward references are rejected. No handwritten invalidation list.
- Transactions stage all update expressions against the pre-transaction state, then commit state, then make one topological propagation pass. A changed input triggers evaluation; only a changed result propagates further. Each affected derived node evaluates at most once per transaction.
- No `eval`, `exec`, raw C++, runtime JS, DOM or browser engine. Generated identifiers are internal, text becomes fixed-width escaped UTF-8 bytes, and integer addition checks overflow.
- Headless typed view value holders are scoped C++ objects with actual constructor/destructor lifecycle counters. These are not OS-native widgets.
- The list demo filters 10,000 strings and copies a 12-row window into its headless list view. The `window` operator caps requested rows at 1,000. Filtering still scans/copies the full matching dataset; this is **windowed view materialization, not virtualized data storage or a complete scrolling widget**.

## DSL example

```json
{
  "version": 1,
  "states": [{"id": "count", "type": "int", "initial": 0}],
  "derived": [{"id": "title", "type": "text", "expr": {
    "op": "concat", "args": [
      {"literal": "Count: ", "type": "text"},
      {"op": "to_text", "args": [{"ref": "count"}]}
    ]
  }}],
  "views": [{"id": "counter", "source": "title"}],
  "transactions": [{"label": "increment", "set": {
    "count": {"op": "add", "args": [{"ref": "count"}, {"literal": 1, "type": "int"}]}
  }}]
}
```

The full example is `examples/counter_list.eui.json`. Regenerate it with `python3 examples/make_demo.py`.
`filter(texts, text)` is case-sensitive UTF-8 byte substring matching, not locale-aware search. `window(texts, start, count)` rejects negative indices/counts and counts above 1,000; an offset past the end returns an empty array. No arbitrary application I/O is expressible.

## Evidence and metric definitions

Each JSON line is a post-transaction snapshot. Counts are cumulative:

| Step | title evaluations | filter evaluations | window evaluations | view patches | window rows copied to views |
|---|---:|---:|---:|---:|---:|
| initial | 1 | 1 | 1 | 2 | 12 |
| unchanged assignment | 1 | 1 | 1 | 2 | 12 |
| unrelated state changes | 1 | 1 | 1 | 2 | 12 |
| counter increment | 2 | 1 | 1 | 3 | 12 |
| filter + offset, one batch | 2 | 2 | 2 | 4 | 24 |
| narrow to one result | 2 | 3 | 3 | 5 | 25 |
| new query, same filter result | 2 | 4 | 3 | 5 | 25 |
| empty batch | 2 | 4 | 3 | 5 | 25 |

`patches` counts initial headless view initialization and changed value assignments, **not pixel draws**. `rows_materialized` counts rows copied to list view values, **not all allocations/copies**: intermediate filtering, window expressions and JSON serialization also cost work. Unchanged updates cause zero derived/view work, but state comparisons, dependency checks and snapshot logging still execute. Scheduling resets dirty bits and checks guards across the whole graph (O(graph size)); it is dependency-gated expression execution, not a sparse work-queue scheduler. Two view constructors and two destructors are recorded at normal exit.

Tests cover exact output, selective invalidation, batching, equivalent-result propagation suppression, simultaneous swap semantics, safe Unicode/control-character/code-like strings, overflow and invalid windows, determinism, malformed DSL rejection and bounded process completion.

## Important boundaries

- The original v0.1 executable remains a **finite compile-time scripted workload**. v0.2 additionally emits a persistent typed model and blocking-stdin host (below). These two earlier hosts are not desktop windows. v0.3 adds a separate SDL2 window host; accessibility remains absent.
- The original v0.1 scripted executable is fail-fast on runtime errors. The persistent model stages a candidate, evaluates it fully, then commits with a nonthrowing pointer swap; state and counters survive rejected updates unchanged. Host rendering failures after commit are fatal and do not pretend to roll back the committed model.
- All data is in memory; some values are copied. No claim of optimal memory, low power or lower resource usage than Electron has been measured.
- No completed three-platform validation, accessibility integration, verified system IME, GPU compositor, animation system, package format or security sandbox exists yet. Current development adds Pango shaping and a bounded row/column flow layout, not CSS compatibility. C++17 portability is a design choice, not a tested Windows/macOS claim.
- Compiler size/depth limits bound common malformed input, not a hostile-input resource sandbox. Use trusted ASTs; generated native programs are not sandboxed.

The v0.3 slice connects the typed model to declared widget rectangles/actions and a desktop input loop. It does not complete accessibility, responsive layout or cross-platform packaging; comparative memory/energy claims require equivalent full workloads.

## Optional native raster preview

See [native rendering notes](docs/rendering.md) for `./scripts/render-preview.sh`.
This separate Linux/Cairo path draws compiled-state snapshots to PNG without a browser or GUI display; it does not change the dependency-free core requirement or establish interactive/GPU/cross-platform performance.

## v0.2: persistent typed model and direct native renderer

```sh
./scripts/build-live.sh
./build/live/live-host build/live/frames
# Type commands while this same process remains alive:
# set-int count 7
# begin
# set-text query "99"
# set-int offset 2
# commit
# quit
```

`compiler.py INPUT -o scripted.cpp --model-header model.h` emits both the original scripted program and a reusable C++17 `explicit_ui::Model`. Include paths must contain the generated header directory and `native/` (`model_api.h`). The host does **not** execute the AST's scripted transactions: it starts at declared initial state and accepts external updates. State types, view bindings and dependency guards come from the compiler, not handwritten demo invalidation rules.

`Model::apply(vector<Update>)` accepts typed `int64_t`, `string` or `vector<string>` values via `Value` (`std::variant`). It returns whether any view value actually changed. `view_text(id)`, `view_texts(id)`, `view_int(id)` and `stats()` expose committed data; unknown IDs/type mismatches throw. `ViewModel` is the small read-only renderer seam, not a general widget/layout API. References remain valid only until the next successful apply. Native calls must use valid UTF-8; invalid runtime text is rejected.

The host blocks on standard-input reads between input lines: no polling, timer or continuous animation. Initial state and every committed transaction produce a flushed JSON snapshot on stdout. Other commands return a flushed JSON acknowledgement/error. JSON is **diagnostic output only**; the native callback reads the same C++ model directly, with no capture/recompile or JSON-to-render bridge. `renders` counts invocations of the host's render sink (including its console-only sink); it is not a GPU draw or compositor frame counter. With the optional PNG directory, native Cairo rendering runs on initial state and only after a commit with changed view values, writing `redraw-0.png`, `redraw-1.png`, etc. No new frame is written for same-value, unrelated-state or equivalent-derived-result changes.

### Line protocol

- `set-int STATE SIGNED_DECIMAL`: checked signed 64-bit value (no `+` prefix).
- `set-text STATE "TEXT"`: C++ `std::quoted` syntax. Backslash escapes the next character; this is **not JSON** (`\n` becomes `n`, not newline). Spaces and UTF-8 are accepted inside quotes; empty quotes clear text.
- Set commands outside a batch commit immediately. Inside `begin` / `commit`, writes are staged, the last write to a state wins, and propagation runs once. State/type validation of a staged update happens at commit.
- `abort` discards an open batch. Any malformed command or failed apply discards the entire open batch and leaves committed model data/counters intact. The host then accepts more input.
- `renderer-stats` reports actual native `cache_builds` / `render_frames` (both zero without PNG rendering).
- `snapshot` reports committed state, even during a batch. `quit` and EOF discard pending writes and exit cleanly. `quit` reports `bye`; EOF reports `eof`.

The line host intentionally exposes only integer/text setters; native C++ callers can also replace a text array. Limits: 1,000 staged writes, 100,000 items per replacement text array and 1 MiB per command line. The reader retains at most 1 MiB of command bytes, drains oversized lines without retaining their tail, then reports an error and recovers on the next line. Stream/string capacity and parsed copies have additional overhead; this is not a process-memory limit or hostile-input resource sandbox. No concurrency/thread safety, arbitrary expressions or application I/O are offered by this protocol.

### Costs and evidence boundaries

The current generated model shares immutable text and array buffers between committed and candidate states. Each apply still allocates a candidate and copies scalar fields/counters/ownership handles; changed non-scalars and derived results allocate new values. Final no-op updates return after validation without committing. Dependency guards are still scanned across the graph. See [allocation and rollback evidence](evidence/v0.4/model-storage.md). Unchanged views avoid derivation work when inputs do not change and avoid the renderer callback, **not all CPU work or allocations**. This is a correctness-first prototype, not proven optimal memory or data virtualization. `rows_materialized` retains the v0.1 logical window-row metric; it does not count transaction copies, filtering allocations or renderer resources. Renderer blur is cached for a fixed demonstration background; it is not OS backdrop capture, Apple's Liquid Glass implementation or a GPU compositor.

`tests/test_live.py` launches real persistent processes with stdin kept open, sends updates after launch, checks atomic batch rollback/recovery, invalid inputs/UTF-8, equivalent-result redraw suppression, integer overflow through the native API, empty-model strict compilation, EOF and explicit quit. Core and original scripted tests remain in `./scripts/check.sh`.

## Native window (v0.3 origin, current input/layout extended)

```sh
sh scripts/build-window.sh
./build/window/window-host
```

Requires an actual graphical desktop, SDL2 and Cairo. The current Linux fallback links installed shared runtimes; `pkg-config` development packages are used when available. No browser, HTML, JS runtime or PNG encode/decode exists in the normal presentation loop.

The default window build now uses `examples/responsive.layout.json`: nested row/column flow, padding/gap, basis/grow and a narrow-window breakpoint. Actual widget geometry and rasterization change on resize. Legacy fixed-canvas `examples/window.layout.json` remains supported. Both are validated against the typed model before `scene.h` emission. See [layout contract](docs/declarative-layout.md).

Click +/−, focus the search field and type, use Backspace to remove a UTF-8 codepoint or Ctrl/Command+Backspace to clear, and scroll the list with the mouse wheel. Tab/Shift+Tab moves focus; Enter/Space activates focused buttons; Escape clears focus. Close the OS window to exit. The current single-line editor adds caret movement, selection, clipboard shortcuts and bounded preedit handling. Pango supplies shaped geometry. Actual system IME and grapheme navigation remain separate incomplete acceptance gates; see [text input](docs/text-input.md) and [shaping](docs/shaped-text.md).

The normal loop blocks in `SDL_WaitEvent`, repainting on visible state/focus changes and presenting the cached frame on expose/resize. Responsive layouts re-rasterize at window size; a complete high-DPI/display-scale policy remains unimplemented. Model candidates share unchanged buffers; changed query filtering still scans/copies collection results.

```sh
SDL_VIDEODRIVER=dummy ./build/window/window-host --smoke build/window/synthetic.png
```

This last command is explicitly a **synthetic SDL-event regression**, not evidence of a visible desktop or physical input. Actual desktop QA is recorded separately in `evidence/v0.3/`.

## Compiled application actions and persistence

Named `actions` stage typed expressions against the pre-action snapshot, then commit via one model transaction. `prepare_action` exposes the staged writes so a host can validate widget-specific policies before commit. `append` and `remove_at` support a small task-list example; see [actions](docs/actions.md).

Optional `persist` declares exactly which state fields may be exported/restored. Unknown, missing, duplicate or wrong-typed fields reject the whole restore. File data is versioned and bounded, never executable. The POSIX writer uses same-directory temporary data plus rename; directory-fsync durability and concurrent-writer conflict protection are not claimed. See [state storage](docs/app-state.md). No default user file is opened: the window requires explicit `--state-file PATH`.

A CMake entry builds the same host against another declaration:

```sh
cmake -S . -B build/tasks -DCMAKE_BUILD_TYPE=Release \
  -DEUI_MODEL="$PWD/examples/task_list.eui.json" \
  -DEUI_LAYOUT="$PWD/examples/task_list.layout.json"
cmake --build build/tasks --target eui-window
./build/tasks/eui-window --state-file /absolute/path/to/task-state.eui
```

The file must already be valid for startup restore if it exists; a missing file starts from declared state. Add task and Remove first are intentionally modest demo actions, not a production task manager. See [portable build preparation](docs/building.md); CMake syntax is not Windows/macOS execution proof.

## v0.5 input, conditions and experimental accessibility

The native host uses Pango grapheme boundaries and strong-caret visual navigation, actual pointer selection, and bounded64-step undo/redo. System IME, extended composition, multiline editing and weak-caret affinity remain incomplete; see [input evidence](docs/text-input.md).

Strict bool and lazy conditions are compiled, not dynamically interpreted. Both branches are typechecked; unselected runtime errors do not execute. Non-scalar selected results may still materialize/copy. Persistent files containing bool use v3; older v1/v2 files remain readable and non-bool saves retain v2.

The Linux accessibility bridge is **off by default** and requires explicit build/runtime opt-in and a coherent fixed ATK/AT-SPI/bridge deployment. It exposes actual semantic nodes and routes requests to the UI thread. The tested cloud route uses pinned official2.62.1 libraries only in the process loader path. An early version guard rejects the known old environment, but does not attest arbitrary mixed/custom libraries. See [build/lifecycle contract](docs/linux-accessibility-spike.md).

Independent AT-SPI clients and native desktop tests passed. **Orca/audible screen-reader acceptance has not passed.** Read [remaining limits](docs/known-limitations.md) before treating this as an application SDK.
