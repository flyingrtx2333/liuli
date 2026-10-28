# Actual native Linux desktop QA

This is a real SDL X11 window on the assistant cloud desktop, controlled through the supported desktop UI tool. It is not the SDL dummy smoke and not a browser rendering. The command sandbox has no display, so the executable was launched from the cloud desktop terminal against the shared workspace. No user machine was involved.

## First interaction pass

- Initial native window reported `video_driver=x11`, count 0 and the first list rows.
- Two button clicks plus Return changed the count to 3.
- Mouse focus followed by keyboard input `00007` visibly filtered to `Item 00007`.
- Backspace changed query to `0000` and restored multiple matches.
- Ctrl+Backspace cleared query; actual wheel down moved first row to `Item 00003`.
- Clicking the window decoration resize/maximize control changed the client size from 1180x812 to 1120x840. Subsequent increment click mapped correctly (count 4); Tab moved from button to search and typing `00007` restored the single match.
- Native Alt+F4 closed the window; process exit code 0. Final state is recorded in `actual-window-first-pass.log`.
- Two UI issues were found: clipped hint and wrong Escape description. Both were sent to implementers for correction. Final screenshot is captured after rebuild, separately below.

## Matched binary framebuffer experiment

Same executable (see `ab-binary.sha256`), same desktop and initial state, no user input during each observation:

- SDL default surface acceleration: RSS/HWM 113976 kB (111.30 MiB).
- Environment `SDL_FRAMEBUFFER_ACCELERATION=0`: RSS 26548 kB (25.93 MiB), HWM 37960 kB (37.07 MiB).
- Both 2-second observations had zero additional CPU ticks at 100 ticks/second.
- RSS reduction ~76.7% in this one scene/run; not a complete-app benchmark or Electron comparison.
- This confirms a useful CPU-rendering configuration for this SDL/Linux environment, not energy, battery, cross-platform, GPU, or general performance proof.

Raw results are `actual-window-idle-before-sdl-hint.json` and `actual-window-idle-after-sdl-hint.json`. A/B windows both closed with exit 0.

## Final rebuilt executable

- Both reported UI issues are corrected: hint is `Click to count`, footer is `Esc: unfocus`.
- Without any environment override, actual X11 window RSS was 26552 kB (25.93 MiB), HWM 37848 kB (36.96 MiB). Two-second idle CPU ticks stayed19→19. See `actual-window-final-idle.json`.
- Repeated two clicks + Return produced count3. Tab focused search; typing00007 produced exactly Item00007.
- Scrolling down500px on the single-result list retained that row instead of making the list empty.
- Backspace and Ctrl+Backspace restored the full list; wheel down120px started at Item00003.
- Re-entering00007 restored the single match. Escape cleared focus; subsequent printable input did not change query.
- Native Alt+F4 closed successfully, EXIT=0. State proof: `actual-window-final.log`.
- `actual-native-window.jpg` is an app-only screenshot returned by the supported desktop-control tool, after actual mouse/keyboard interactions. The tool supplied JPEG bytes, retained as JPEG; it is not a renderer export. Client size1180x812. No unrelated desktop content is included.
- `final-window-binary.sha256` identifies the final tested executable.
