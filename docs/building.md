# Building Explicit UI

## CMake (portable build entry, Linux verified)

Requires CMake 3.20+, Python 3.9+, a C++17 compiler, and SDL2/Cairo development dependencies for the window target. The generated application does not run Python; Python is a build-time dependency.

```sh
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake --config Release --parallel 2
ctest --test-dir build/cmake -C Release --output-on-failure
```

Targets:

- `eui-scripted`: original finite compiler workload.
- `eui-live`: persistent standard-input host, console-only in this CMake target.
- `eui-window`: SDL2/Cairo native window, generated responsive layout by default.

On single-configuration generators, binaries are inside `build/cmake/`. Multi-configuration generators commonly place them in `build/cmake/Release/`. Run `eui-window` (or `eui-window.exe`) from a graphical desktop. The CTest window case deliberately uses `SDL_VIDEODRIVER=dummy`; it proves only synthetic event handling, not visible-window behavior.

To build the dependency-free C++ core without graphics libraries:

```sh
cmake -S . -B build/cmake-core -DEUI_BUILD_WINDOW=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake-core --config Release --parallel 2
ctest --test-dir build/cmake-core -C Release --output-on-failure
```

The existing `scripts/check.sh` retains the broader Python regression suite; CTest currently runs smaller executable smoke checks, not that entire suite. Existing shell build commands remain supported on Linux.

## Dependency resolution

The window build first tries the `SDL2::SDL2` CMake package target, then SDL2 pkg-config. Cairo currently uses its pkg-config metadata. These dependencies must be installed for the actual target architecture by the developer/build environment; this project does not silently download or vendor them. Include paths and link flags come from the dependency metadata.

For this Linux cloud environment only, which has shared runtimes but no development headers, the build can find `libSDL2-2.0.so.0` and `libcairo.so.2` and use the existing small ABI declaration shims. Disable that fallback to require development packages:

```sh
cmake -S . -B build/cmake-strict -DEUI_LINUX_RUNTIME_FALLBACK=OFF
```

A missing dependency is a configure error, not a silent switch to a browser renderer or dummy application. The Linux runtime fallback is never selected by CMake for macOS or Windows.

Alternate declarations can be selected at configuration time:

```sh
cmake -S . -B build/custom \
  -DEUI_MODEL=/absolute/path/app.eui.json \
  -DEUI_LAYOUT=/absolute/path/app.layout.json
```

The `--smoke` mode is still a test fixture for the bundled counter/list model, not a generic test for arbitrary applications. Custom apps need their own behavioral checks.

## Actual verification and platform limits

The current assistant cloud has Linux `c++` and SDL2/Cairo runtime libraries. CMake was installed from its package-registry distribution into a temporary private directory solely for build verification; it is not copied into the source package and no system package/settings were changed. Logs are under `evidence/build-portability/`.

No macOS SDK/Xcode, Windows compiler or Windows runtime was available during this work. This CMake entry is preparation for portable builds, **not a claim that macOS or Windows build, launch, IME, accessibility, signing or packaging have passed**. SDL's own multi-platform availability does not establish this application's compatibility.

The smallest honest next platform validation is to provision/select a target-platform build machine with SDK and SDL2/Cairo development dependencies, compile this exact source, then run the same visible-window and application tests. Cross-compilation alone cannot close interaction/system integration gates. Paid infrastructure, credentials, code-signing identities and publication remain separate actions requiring appropriate authorization.

The bundled `window_synthetic` CTest assumes the counter model and responsive
layout. It is registered only for those exact bundled declarations. Custom
models/layouts retain generic core/accessibility checks but must supply their own
interaction proof; skipping a mismatched counter fixture is not a custom-app pass.
