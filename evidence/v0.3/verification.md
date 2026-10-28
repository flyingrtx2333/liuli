# v0.3 verification scope

2026-10-27. Assistant-owned Linux cloud environment; no user-computer changes or remote repository publish.

## Automated verification

- `tests-final.log`: 20 passing unittest cases, 36.670 seconds, after final input-error, wheel-bound and framebuffer-default changes.
- `core-check-final.log`: parent-run aggregate validation, including unit tests, strict C++17 scripted build, native live-host build and stdin smoke. Passed with exit status 0.
- `synthetic-window.log`: SDL dummy-video events exercise click, keyboard activation, search, UTF-8 backspace, clear, resize hit testing, focus loss, Tab, scroll limits and quit. Exactly two intentional input errors test rejection without mutation. This is not a visible desktop test.
- Layout compiler tests cover typed bindings, unknown fields, invalid rectangles, duplicates, unsupported search count, list step bounds, NUL IDs and declared positions/actions reaching compiled C++.

## Actual desktop and resource evidence

The QA report and native app screenshot in this folder document actual X11 window interaction, separately from synthetic tests. Linux X11 is the only tested desktop backend. A virtual cloud desktop is not an iPhone or a physical battery-powered device.

The before/after SDL-hint observations use the same executable, with `SDL_FRAMEBUFFER_ACCELERATION=0` as the changed environment setting; identity is in `ab-binary.sha256`.

- Default prior path: RSS 113976 KiB, peak 113976 KiB.
- Explicit CPU framebuffer: RSS 26548 KiB, peak 37960 KiB.
- Settled RSS reduction: about 76.7%; these are separate process samples, not repeated statistical trials.
- Two-second idle intervals: zero additional CPU ticks at 100 Hz, not zero power or an energy measurement.
- Final source defaults to CPU framebuffer; explicit environment overrides are preserved for experiments.

No claims of complete Electron parity, tested macOS/Windows/iOS, responsive layout, GPU rendering, accessibility integration, production-grade text input or superior battery life follow from these tests.
