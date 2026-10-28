# Immutable model candidate storage

The generated persistent `Model::Data` now owns text and text-array values through `shared_ptr<const T>`. A transaction still copies its scalar fields, counters and ownership handles into a private candidate, but no longer copies unchanged collection/string buffers. Updates validate their input and allocate a new immutable value only if needed; derived expressions allocate their own result. Direct reference-derived nodes share immutable ownership. Commit remains a nonthrowing `unique_ptr` swap after all evaluation succeeds. No mutation is performed through a shared value.

Public `Model`, `Value`, `Update`, view/state getters and reference-validity rules are unchanged. Equal final staged state values return early after input validation, without committing; last-write-wins behavior remains. Unknown/type-invalid/UTF-8-invalid intermediate updates are still rejected even if a subsequent update would restore the original value.

## Correctness checks

`compiler-storage-tests.log` records the original 8 compiler tests, 5 persistent-model tests and new immutable-storage test passing. The new compiled C++ check covers:

- Direct/transitive immutable aliases share ownership without exposing writable buffers.
- Caller-owned input changes after apply do not affect committed values.
- Empty, same-value and change-then-restore transactions leave values/counters unchanged.
- Invalid windows and arithmetic overflow after staged array replacement preserve committed state, references and counters.
- Invalid UTF-8 is not skipped on a final no-op update.
- `bad_alloc` injected at every allocation point until the transaction succeeds preserves committed data/counters on every failure.

## Matched allocation probe

`model-allocation-probe.cpp` is compiled with `-std=c++17 -O2`, once against the pre-change generated header and once against the optimized header, using the same 10,000-row `counter_list.eui.json` model. Model construction and caller-owned Update construction are excluded. Global `operator new` counts requests made while `apply` executes. Requested bytes are cumulative allocation traffic, **not peak memory, retained memory, RSS, or energy**. Timings cover the whole update loop, not rendering or a full application.

Five consecutive process runs of each build are preserved in the JSONL files; `model-allocation-summary.json` reports median elapsed time and deterministic allocation counts.

| Case | Iterations | Before requested bytes | After requested bytes | Before/after allocations | Median ms before/after |
|---|---:|---:|---:|---:|---:|
| Changed counter | 1000 | 640584000 | 192000 | 4000 / 2000 | 115.366 / 0.052548 |
| Unchanged counter | 1000 | 640584000 | 144000 | 4000 / 1000 | 110.172 / 0.020440 |
| Alternating query `99` / empty | 100 | 102669600 | 54190400 | 2150 / 2150 | 18.0879 / 18.5469 |

Counter updates avoid copying unrelated large collections. Query changes still scan/filter/copy the collection and therefore do not show a timing improvement in these samples; requested allocation bytes decrease but allocation count does not. The especially short optimized counter timings are microbenchmark observations, not a promised app-speed multiplier. This is not an Electron comparison.

## Reproduction and limits

`model-storage.patch` records the change against the retained pre-change compiler; `model-header-hashes.txt` identifies both generated headers. The old compiler/header/binary remain in ignored `build/memory-baseline/`, current counterparts in `build/memory-optimized/`. To reproduce from packaged source, copy it to a separate directory and reverse the patch to obtain the old compiler; generate headers from the same example, compile the probe against each header, then run each binary five times.

Dependency scheduling still scans graph guards. Candidate creation still allocates a Data object and copies scalar fields/shared ownership handles. Equal supplied arrays still require validation/comparison. Filter/window expressions still materialize new arrays, and initial source/filter storage is not deduplicated unless expressed as a direct reference. Shared ownership adds per-value allocations/reference-count costs; no general zero-copy, zero-allocation or thread-safe API claim is made.
