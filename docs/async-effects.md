# Bounded asynchronous state effects

`native/async_state_store.h` wraps the existing state codec/filesystem service with one worker thread. No model, renderer, widget or SDL object is accessed by the worker. The caller explicitly provides one path and a notification callback. This is a local state-store adapter, not a general task scheduler or filesystem sandbox.

## API and ownership

```cpp
explicit_ui::AsyncStateStore store(path, notify);
auto id = store.save(captured_state); // captures an independent value snapshot
auto other = store.load();
store.cancel();
auto completion = store.take_completed();
store.close();
```

`save` performs bounded pure codec validation on the calling thread before accepting a request; disk I/O runs on the worker. A caller should capture only its declared persistent fields, not the entire model. Requests carry monotonically increasing 64-bit IDs. A completion contains its ID, operation, loaded state, `exception_ptr`, cancellation flag and `save_completed` flag.

At most one job is running, one latest job is pending, and one completion is retained. Submitting a new request cancels the active request on a best-effort basis, replaces any pending request and discards an unconsumed prior completion. Superseded requests are not guaranteed individual completion notifications. Only the latest request's completion is published. This bounds retained snapshots/results rather than accumulating a queue while the UI is busy.

The notification callback runs on the worker thread after a result has been stored. It must be quick, must not block, must not destroy the store, and must not read or mutate UI/model state. An SDL adapter can push a registered user event and call `take_completed()` on the UI thread when that event arrives. Spurious/stale events are harmless: the mailbox may already be empty. Notification exceptions are contained; `notification_failed()` records that at least one callback failed, and the completion remains available. A failed notification cannot guarantee that the UI will wake, so adapters should avoid throwing and handle event-queue failure explicitly.

## Cancellation, revisions and shutdown

Cancellation checks occur before and after a filesystem operation. Already-running OS calls cannot be interrupted by this adapter. In particular, cancellation **does not undo a save that already reached its atomic rename**. `save_completed=true` means the underlying save returned successfully even if cancellation arrived too late. It does not imply that the saved snapshot equals the current UI state.

The UI must keep its own model revision when submitting a load. If the user edits before that load completes, reject the stale completion instead of overwriting those edits. Request IDs prevent older requests from winning over newer requests; they do not by themselves detect independent model edits. Apply restored state only after complete schema and widget validation, using one model transaction. A save may legitimately persist the snapshot requested while later UI edits remain dirty.

`close()` cancels active work, discards pending work/results, wakes the worker and joins it. Destruction calls `close()`. Close/destruction must be called from the owning thread, not the notification callback, and not concurrently with another close/destruction. The notifier's dependencies must outlive the join: destroy/close this adapter before `SDL_Quit` or destruction of any object it uses. A notifier already running when shutdown starts is allowed to finish; none runs after close returns.

The file service bounds content and rejects non-regular POSIX inputs using nonblocking open plus `fstat`. It does not bound storage latency. A slow or stuck filesystem operation can delay cancellation and shutdown join. This is not guaranteed immediate termination on network filesystem hangs, crash recovery, directory-fsync durability, concurrent-writer conflict detection or a Windows atomic-save backend.

## Verification

`tests/test_async_state_store.py` compiles a strict C++17 `-pthread` test using real temporary files. It checks snapshot ownership, save/load, malformed-file errors, rejected oversized requests, newest-pending wins, cancelled pending saves, bounded completion consumption, repeated requests, notifier exceptions, idempotent shutdown, destructor join and write failures. A controlled notification barrier makes replacement/cancellation deterministic without adding a fake filesystem backend to production code.

## Native SDL host integration

The host keeps startup restoration synchronous before opening its first window. Later Ctrl/Command+S and Ctrl/Command+O use the worker. The worker captures only an immutable-value state snapshot; it never sees Model or TextEdit. A registered SDL user event is coalesced with an atomic queued flag, so notifications do not grow without bound. The UI thread consumes the latest completion and performs all model/editor work. Normal idle operation has no polling timeout; the timeout-based wait helper exists only for synthetic tests.

Every accepted state/editor change advances a host generation. A load remembers its submission generation, and its completion is skipped if newer edits occurred. Request IDs reject superseded operations independently of this generation check. Save completion distinguishes the requested snapshot from newer unsaved edits. Native window-title status exposes Saving, Saved, Loading, Load rejected, stale-load, cancellation and error outcomes. Ctrl/Command+Escape requests cancellation.

A normal Close during an explicit pending Save is deferred until successful completion; failure keeps the window open. Editing and further load shortcuts are ignored while this close-save is pending. Explicit cancellation may proceed with close, but cannot reverse a completed rename. Shutdown joins the worker and then flushes its registered event before SDL teardown; no notifier accesses destroyed UI/model objects. A failed SDL notification remains observable through its flag and mailbox on the next UI event; without a successful wakeup it cannot promise immediate completion feedback.

`tests/test_host_transactions.py` exercises the real host on SDL dummy: late-load rejection after edits, latest save selection, cancellation, saved-old-snapshot status, corrupted-startup overwrite prevention, save-immediate-quit preservation, model/editor rejection atomicity and close with an active worker. These tests are separate from actual desktop QA.
