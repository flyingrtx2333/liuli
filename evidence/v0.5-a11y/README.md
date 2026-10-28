# Opt-in Linux ATK spike evidence — 2026-10-27

Not Orca acceptance or a claim of Electron parity.

## Final actual cloud desktop run

- `provider.log`: actual `video_driver=x11`, independent client changed counter
  and search state, invalid newline rejected, normal window close, **exit 0**.
  This run used `G_DEBUG=fatal-warnings` (warnings were not ignored).
- `client.log`: separate process linked to system libatspi; discovered all roles,
  invoked `+1`, focused entry, edited `00007` (one matching row) then `中文🙂`
  (3 Unicode characters, caret 3), observed focus/text/caret/children events,
  rejected newline without mutation, queried retained removed row as defunct;
  **exit 0**. No model/provider APIs were linked into the client.
- `actual-atspi-committed.png`: actual desktop screenshot after those operations.
- `cua-atspi-tree.txt`: independent desktop tooling switched from generic X11
  fallback to the real `at_spi` tree and reported the committed entry value.
- `idle.json`: 10-second post-client idle sample, unchanged process CPU ticks and
  RSS pages; 6 threads with fixed runtime. This is one observation, not a general
  battery/energy benchmark. Sleep occurs only in the measurement script.
- `session-lifecycle.log`: 3 same-process init/teardown cycles, duplicate-provider
  rejection and malformed-parent rejection, with fatal warnings enabled, exit 0.

Provider runtime: official Debian ATK/AT-SPI/bridge 2.62.1 trio extracted beneath
`build/a11y-runtime`; process-local loader path only. Separate client retained the
system libatspi to test the real interprocess protocol. Source/package hashes and
fix provenance are in `runtime-manifest.txt` and the helper scripts. The cloud's
preexisting task-local no-SHM SDL build was selected in the same process loader
path. No system library, accessibility toggle, X settings or user machine changed.

## Regression / diagnostic runs

- `python-tests.log`: all 39 existing Python tests pass after shared renderer text
  sanitization extraction and optional provider integration.
- `ctest.log`: scripted smoke, real-ATK in-process semantic-tree unit test, synthetic
  dummy-SDL host test. These are explicitly not actual desktop evidence.
- `tree-sanitizers.log`: synchronous real ATK tree lifecycle under ASan+UBSan,
  including full subtree removal with retained refs, order, malformed snapshot,
  duplicate singleton lease, bounded external edit and queued action checks.
  Leak detection is disabled for this check; no leak-freedom claim is made.
- `repeated-1..3.log`: earlier default-context provider ran three consecutive
  ASan/UBSan bridge-enabled synthetic-event smoke cycles on real X11, exit 0.
  These runs used the old system bridge and **no independent cached client**;
  they did not detect the upstream client-cache cleanup bug below.
- `upstream-cleanup-failure.log`: preserved failing system-library trace and exact
  `g_object_weak_unref(NULL, ...)` observation. Official upstream commit c4ddb104
  fixes cache cleanup's values-versus-keys bug. Final fixed-library run above
  passes the identical client interactions and actual close without warnings.

See `docs/linux-accessibility-spike.md` for supported interfaces, threading
contract, build commands, async request semantics and explicit remaining gates.

## Follow-up startup guard

`runtime-guard.log` was collected without DISPLAY or session-bus variables:
known-old coherent system ATK 2.56.2 is rejected by both the pure check and the
provider constructor; a new-ATK/old-AT-SPI inspection is rejected; the fixed
2.62.1 API versions pass. No bridge was initialized by the pure checks.
`ctest-runtime-guard.log` includes strict numeric version-parser regressions and
the existing default-off host smoke. This is not attestation of an arbitrary
mixed bridge: coherent fixed packages remain a documented deployment requirement.
