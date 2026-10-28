# Cloud X11 presentation: scoped non-SHM SDL

## What failed, and what was established

On the tested cloud X11 desktop, system SDL 2.32.4 intermittently failed at startup or live resize with MIT-SHM `X_ShmPutImage BadValue`. This is **not established as a responsive-layout bug**: a minimal SDL-only fill/update probe reproduced it without Cairo, layout, model code or min/max setters, with matching window and surface dimensions. Bounded updates and a framebuffer-acceleration toggle did not solve it.

A separate native Xlib `XPutImage` probe passed 614 synchronized presentations, narrowing and expansion. An unmodified official SDL 2.32.4 build with `-DNO_SHARED_MEMORY` then passed the same real SDL probe with explicit `x11`, actual border resizing 1180 → 519 → 1119, and clean Alt+F4 closure. The integrated application subsequently passed native desktop QA with this scoped library. These results establish a viable non-SHM route, **not** a fix to system SDL or proof of the underlying server/segment failure mechanism.

The first local build failed X11 initialization because dynamic Xext discovery was missing; its offscreen fallback was rejected as a test result. The corrected build explicitly includes `libX11.so.6` and `libXext.so.6`. No X errors were suppressed, server settings changed, or system libraries replaced.

## Rebuild from a source-only package

Target: Linux x86_64, X11; tested Debian-compatible runtime paths. Requires a C/C++ toolchain, Make, CMake, curl, tar, dpkg-deb and sha256sum, plus existing `/lib/x86_64-linux-gnu/libX11.so.6` and `libXext.so.6`. No sudo or system package installation is performed. A missing prerequisite is an explicit setup failure, not permission to modify the system.

From the project root:

```sh
# Downloads pinned inputs, verifies hashes, extracts headers locally, builds SDL.
sh evidence/v0.4/sdl-noshm-recipe.sh
# If cmake is not on PATH: CMAKE=/absolute/path/to/cmake sh ...
sh scripts/build-window.sh
# Run from an actual cloud desktop terminal with DISPLAY set:
sh scripts/run-cloud-window.sh
# Or choose a binary and pass arguments (relative paths use caller's cwd):
sh scripts/run-cloud-window.sh ./build/window/window-host
```

The recipe derives the project root from its own location; all downloaded headers, source and outputs stay in `build/qa/`. It is sufficient to recreate dependencies omitted by source packages. The launcher selects only this process's library via `LD_LIBRARY_PATH`, forces `SDL_VIDEODRIVER=x11`, rejects absent DISPLAY/library, and preserves the executable's exit status. It never silently chooses dummy/offscreen. Do not export its library path globally.

This is a limited **cloud diagnostic/presentation build**, not the general distribution SDK: audio, joystick/haptics/sensors, OpenGL/ES/Vulkan, Wayland and optional X11 extensions are disabled. Other platforms and normal native distributions need separate backend qualification. The observed binary hash is provenance, not an expectation of byte-identical builds across toolchains.

## Pinned inputs and evidence

Exact download URLs are in [`sdl-noshm-sources.txt`](../evidence/v0.4/sdl-noshm-sources.txt), verified by [`sdl-noshm-inputs.sha256`](../evidence/v0.4/sdl-noshm-inputs.sha256):

| Input | SHA-256 |
|---|---|
| SDL2-2.32.4.tar.gz | `f15b478253e1ff6dac62257ded225ff4e7d0c5230204ac3450f1144ee806f934` |
| libx11-dev_1.8.12-1_amd64.deb | `530de60c9c6dadd997ae7d74eca2a41b25e849373a5ad5dcdb619aa05713d6e0` |
| libxext-dev_1.3.4-1+b3_amd64.deb | `e6bd898976d762a6955ec465870e7f12a3b149f4c5b231b0687e293ed5726532` |
| x11proto-dev_2024.1-1_all.deb | `6a6764500eb8171d3cd6d793c09902974f8ea87bd20c68d9eac5f1eeaf3fc407` |

See [`x11-shm-diagnosis.md`](../evidence/v0.4/x11-shm-diagnosis.md), retained probe sources in `evidence/v0.4/probes/`, `probe-no-limits.log`, `probe-accelerated.log`, `probe-xputimage.log`, and final `probe-sdl-noshm-fixed.log`. Build configuration, input/library hashes and reproduction log are retained alongside them. A dummy smoke test alone does not validate X11 presentation.

Primary implementation references: [official SDL X11 framebuffer](https://github.com/libsdl-org/SDL/blob/release-2.32.4/src/video/x11/SDL_x11framebuffer.c) contains the real `NO_SHARED_MEMORY` compile guard and XPutImage fallback; [Xserver SHM request handling](https://github.com/mirror/xserver/blob/master/Xext/shm.c) shows why numeric width errors alone do not establish a layout race. No runtime XSHM-disable hint was assumed.
