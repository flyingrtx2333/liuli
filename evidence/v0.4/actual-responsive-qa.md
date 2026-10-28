# Native desktop QA in progress

Assistant cloud desktop, SDL2 2.32.4, actual X11 input (not dummy).

## Responsive pre-integration baseline

Binary hash in responsive-before-fix.sha256, retained locally under build/qa/responsive-before-fix.

- Initial1120×840 window rendered row layout with counter left and search/list right.
- Two actual button clicks changed count0→2; Tab and typing00007 filtered to Item00007.
- App-only screenshot actual-responsive-wide.jpg records this state.
- Native Xfwm resize using Alt+F8 and pointer movement toward~520×740 crashed with MIT-SHM X_ShmPutImage BadValue, exit1. Raw log actual-responsive.log.
- A subsequent fresh launch also failed before window-ready, same X11 request with value1120. Raw log actual-responsive-retry.log.
- No narrow responsive pass is claimed for this baseline. Implementers notified; bounded-surface update patch is pending actual validation.

## Required final combined checks

1. Startup and wide/narrow resize without X11 fatal error; actual reflow and click/search hit alignment.
2. Caret navigation, middle insertion, Shift-selection, replace, copy/paste/cut with demo-only text.
3. Opt-in state-file save, change, restore and restart; failed/malformed load leaves prior model untouched.
4. Focus loss, repeated interaction and clean OS close.
5. Save final native app-only screenshots and binary identity; distinguish synthetic IME events from a real OS IME test.

## Deeper transport isolation

- Bounded `SDL_UpdateWindowSurfaceRects` candidate `103ff253...` still failed before window-ready; log actual-combined.log. Its idle measurement is invalid because the process exited.
- Minimal SDL-only `FillRect` probe with no Cairo/model/layout or min/max setters failed same XShmPutImage BadValue1120. Destination clip was exactly1120×840; log probe-no-limits.log.
- Same minimal probe with `SDL_FRAMEBUFFER_ACCELERATION=1` also failed, BadValue1216; log probe-accelerated.log. Thus reversing the CPU-framebuffer choice alone is not an observed fix.
- No Xserver/system settings or user machine were changed. Actual desktop test remains blocked until a corrected presentation path runs.

## Non-SHM control

A minimal direct Xlib XPutImage probe succeeded on the same desktop: gradient visible, right-border resize1180→519 pixels and expansion survived, then clean exit via Escape. `probe-xputimage.log` records successful synchronized draws. This establishes a viable non-SHM transport; it does not yet validate the full framework on that path. The probe intentionally redraws heavily while observing events, so its frame count is not a framework idle metric.

## Scoped SDL transport fix and intermediate actual acceptance (10:23 UTC)

The first scoped SDL build inadvertently lacked Xext discovery and selected the offscreen driver; `probe-sdl-noshm.log` is NOT a native pass. Forced x11 failed (`probe-sdl-noshm-x11.log`). Corrected build uses official SDL 2.32.4 with `NO_SHARED_MEMORY`, X11 enabled and dynamic sonames libX11.so.6/libXext.so.6. No system library/server/security setting was changed. Explicit `SDL_VIDEODRIVER=x11 LD_LIBRARY_PATH="$PWD/build/qa/sdl-noshm-build"` was used for every subsequent launch.

Actual corrected probe displayed and resized 1180→519→1119 pixels using native border drags, all presentations successful (`probe-sdl-noshm-fixed.log`), then closed via WM AltF4.

Intermediate combined host (`final-host.sha256`; filename reflects test script, NOT async final release) then passed:
- Visible real X11 startup, 1180×812; counter click twice to2; Tab→search00007.
- Real resize to519×812 reflowed controls into column; +click at relocated position changed count3.
- Home/Delete removed first query character; End/ShiftLeft/replacement produced0008.
- CtrlA/C/X/V clipboard roundtrip retained0008.
- CtrlS created `qa-final-state.eui` with count3/query0008. Mutate9999 then CtrlO restored0008 visibly.
- Expanded1119×812 and correct row layout; WM close EXIT0 (`actual-final-host.log`).
- App-only screenshots: `actual-combined-wide.jpg`, `actual-combined-narrow.jpg`, `actual-combined-restored-wide.jpg`.
- Actual initial idle sample (`actual-final-idle.json`): RSS31684kB (~30.94MiB), HWM37980kB;0CPU ticks over2seconds. This is one Linux cloud sample, not energy/battery proof.

Intermediate task-list application (`task-list-sync.sha256`) actual workflow passed: typed “Verify native resize and persistence”, clicked Add, CtrlS, closed EXIT0, relaunched same opt-in `qa-tasks.eui`, confirmed3tasks including new item, clicked Remove first, CtrlS, close EXIT0. State file contains2remaining tasks and log `actual-task-list.log` records restored model. App-only screenshots `actual-task-added.jpg`, `actual-task-restored.jpg`, `actual-task-removed.jpg`.

The intermediate checkpoint above preceded async/minmax retesting; the final section below supersedes its pending status. OS IME composition, other operating systems and battery measurements remain unproven.

## Final async source-frozen acceptance (10:34 UTC)

Hashes: `async-final.sha256`. Final binaries launched through `scripts/run-cloud-window.sh`, explicit X11 scoped SDL. All windows closed after tests. No source/build edits during this verification.

- Final responsive app initial1120×840, actual native drag→518×840→1120×840: correct row/column/row reflow, no X error. Count increment hit alignment at both widths, Tab focus/search, ShiftLeft replacement00007→00008. CtrlS Saved title; mutate9999 then CtrlO Loaded title and restored00008/count2. EXIT0. Screenshots `actual-async-responsive-{wide,narrow,restored}.jpg`, log `actual-async-responsive.log`.
- Final responsive initial scene RSS24132kB (23.57MiB), HWM37936kB;0CPU ticks over2seconds (`actual-async-responsive-idle.json`). This is the final same-scene sample, superseding the intermediate footprint; no battery/power or cross-platform claim.
- Final task app initial900×700; cut/paste entered task, Add and CtrlS observed Saved title. Resize to518px succeeded. Added another task, CtrlS immediately followed by WM AltF4 without intervening observation, closed EXIT0; restart same file observed Loaded title and both additions. This real fast-path confirms user workflow, not deterministically forced disk-race duration. Remove first and repeated CtrlS completed. Screenshots `actual-async-task-saved.jpg`, `actual-async-task-restarted.jpg`; log `actual-async-task.log`. Task initial RSS20112kB,0ticks/2s (`actual-async-task-idle.json`).
- Malformed existing disposable state file: title “Load rejected; saving blocked”; CtrlS→“Save blocked: reload a valid state file”. SHA256 before/after identical6be38feeda8f6fa289d1e3599a3624f4cffa8670c744a5cf6a8e8bb92aad5a0b, bytes preserved. EXIT0. `actual-corrupt-task.log`, `qa-corrupt-{before,after}.sha256`, `actual-corrupt-blocked.jpg` (client-only screenshot omits WM title; title directly observed through native AX).
- Saving/Loading transient titles were too brief to capture; only terminal Saved/Loaded and blocked states actually observed. No claim of OS IME, full accessibility, forced stale-load race, disk-full behavior, macOS/Windows, or battery-life acceptance.
