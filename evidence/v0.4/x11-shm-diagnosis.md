# X11 presentation diagnosis

## Controlled observations

The original responsive app failed with X11 MIT-SHM `X_ShmPutImage BadValue` on live resize; a later clean startup also failed. A bounded-surface update candidate did not fix startup. These are retained failures, not passed tests.

A separate native SDL2.32.4 program with only a window, FillRect and surface update, no Cairo, model, layout or min/max setters, reproduced the same startup error. It logged matching1120×840 window and destination clip. Enabling SDL framebuffer acceleration on the same program also failed. Therefore neither responsive layout nor min/max calls are necessary to trigger the observed failure in this desktop environment.

A separate Xlib-only `XPutImage` program, with no shared-memory transport, presented successfully through614 synchronized updates, actual narrowing to519 pixels and expansion, then cleanly exited. This isolates a viable non-SHM path. It does not determine whether the underlying server failure is segment sizing, namespace handling or another MIT-SHM implementation issue.

## Source inspection and corrective path

- Official SDL2 X11 framebuffer source has a compile-time `NO_SHARED_MEMORY` guard around shared-memory allocation, attach and upload. No runtime XSHM-disable hint was established.
- Xserver's SHM handler can report `BadValue` with the image width for insufficient attached segment size as well as rectangle-bound errors. The numeric width alone did not prove a resize race.
- A task-local unmodified official SDL2.32.4 source build with `-DNO_SHARED_MEMORY` uses the existing XPutImage fallback. CMake output and generated config explicitly verify the X11 backend is enabled; this is not a dummy-only successful build.
- `sdl-noshm-recipe.sh`, source URLs, input hashes and library hash record the build. Debian development headers were extracted locally, not installed system-wide. The executable selects this library only through its process-local `LD_LIBRARY_PATH`.
- Audio/joystick/haptics/sensors and unused GPU/Wayland backends were disabled in this diagnostic build. This scoped library is not a declaration that those capabilities are supported by the framework or a general SDK distribution.

Actual native validation of the scoped SDL library is recorded separately in `probe-sdl-noshm-fixed.log` and the final desktop QA report. Dummy smoke alone is not sufficient proof.

## Primary references

- https://github.com/libsdl-org/SDL/blob/release-2.32.4/src/video/x11/SDL_x11framebuffer.c
- https://github.com/libsdl-org/SDL/blob/release-2.32.4/src/video/SDL_video.c
- https://github.com/mirror/xserver/blob/master/Xext/shm.c

No X11 error handler was suppressed, no global server extension disabled, no IPC/security setting altered, and no system SDL library replaced.

## Verified scoped-library result

The first scoped build lacked dynamic Xext discovery and failed X11 initialization; those logs are preserved. Correcting CMake's case-sensitive `XEXT_LIB` produced dynamic names `libX11.so.6` and `libXext.so.6`.

With `SDL_VIDEODRIVER=x11` and the task-local library, the same native SDL probe displayed successfully, survived actual border resizing1180→519→1119 pixels, and closed through Alt+F4 without an X error. See `probe-sdl-noshm-fixed.log`. This establishes a working scoped SDL non-SHM presentation path, not a claim that the system-installed SDL path is fixed. Full application QA is owned by the separate final integration report.
