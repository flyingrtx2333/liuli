# v0.2 verification and resource observation

Observed in the assistant Linux cloud workspace, 2026-10-27 UTC.

## Native process

The same generated model accepts runtime stdin commands, commits valid batches and directly calls the Cairo renderer. One material cache is built; same-value assignments do not produce extra frames. The recorded live-session shows counter 7 and Item 00007 after a filter commit, three successful PNGs and one cache build. This is an offscreen process, not a desktop window.

## Measured memory change

Both probes use 1120 x 840 output, five counter updates, the same model and PNG export path. The optimization stores two float-RGB work buffers instead of three double-RGB buffers while retaining double calculations/accumulators; work buffers are released before final cache composition.

- Original peak RSS: 86476 KiB (84.45 MiB).
- Optimized peak RSS: 33832 KiB (33.04 MiB).
- Reduction in this observation: 60.9%.
- Optimized resident RSS after events: 16672 KiB (16.28 MiB).
- Same linked binary without PNG rendering, peak RSS: 4996 KiB.
- Source pixel comparison: five of 940800 pixels differ per frame, maximum one 8-bit channel step. Detailed comparison is in ../glass-qa/float-buffer-pixel-deltas.json.

## Timing and idle

- Optimized five-event median command response: 66.995 ms, including full CPU raster output and PNG encoding/writing, plus protocol/JSON work. This is NOT a display frame budget or input-to-screen latency.
- Console-only twenty-event median command response: 0.161 ms. This is NOT a GUI benchmark.
- Both idle observations lasted about two seconds with stdin open and showed zero additional CPU ticks (100 ticks/second resolution). They do not prove zero energy usage, zero wakeups in a real desktop integration, or battery savings.
- Renderer after optimized probe: {'cache_builds': 1, 'render_frames': 6}.

## Limits

These are small single-run Linux process observations, not a controlled comparison with Electron, Flutter, Slint or Qt. RSS is process memory, not total system cost or GPU memory. The model still copies all data per transaction and scans graph guards. No desktop window, OS input, IME, accessibility, GPU compositor or platform portability has been validated. Docker recipe is unbuilt. Tests and build logs accompany this report; no production-readiness claim is made.
