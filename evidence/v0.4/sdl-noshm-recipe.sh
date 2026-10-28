#!/bin/sh
# Rebuild the scoped Linux x86_64 X11 diagnostic library. No system install.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$ROOT"
CMAKE=${CMAKE:-cmake}
BASE="$ROOT/build/qa/deps"
SDK="$BASE/x11-sdk/usr"
BUILD="$ROOT/build/qa/sdl-noshm-build"
mkdir -p "$BASE"
# Download exact inputs; cached files must also pass the pinned hashes.
while IFS= read -r url; do
    name=${url##*/}
    if [ ! -f "$BASE/$name" ]; then
        curl --fail --location --retry 2 "$url" -o "$BASE/$name.tmp"
        mv "$BASE/$name.tmp" "$BASE/$name"
    fi
done < "$ROOT/evidence/v0.4/sdl-noshm-sources.txt"
sha256sum --check "$ROOT/evidence/v0.4/sdl-noshm-inputs.sha256"
tar -xzf "$BASE/SDL2-2.32.4.tar.gz" -C "$BASE"
mkdir -p "$BASE/x11-sdk"
for name in libx11-dev_1.8.12-1_amd64.deb libxext-dev_1.3.4-1+b3_amd64.deb x11proto-dev_2024.1-1_all.deb; do
    dpkg-deb -x "$BASE/$name" "$BASE/x11-sdk"
done
"$CMAKE" -S "$BASE/SDL2-2.32.4" -B "$BUILD" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="-DNO_SHARED_MEMORY -I$SDK/include" \
  -DCMAKE_INCLUDE_PATH="$SDK/include" -DX_INCLUDEDIR="$SDK/include" \
  -DX11_LIB=/lib/x86_64-linux-gnu/libX11.so.6 -DXEXT_LIB=/lib/x86_64-linux-gnu/libXext.so.6 \
  -DSDL_X11=ON -DSDL_X11_SHARED=ON \
  -DSDL_X11_XCURSOR=OFF -DSDL_X11_XDBE=OFF -DSDL_X11_XINPUT=OFF \
  -DSDL_X11_XFIXES=OFF -DSDL_X11_XRANDR=OFF -DSDL_X11_XSCRNSAVER=OFF -DSDL_X11_XSHAPE=OFF \
  -DSDL_WAYLAND=OFF -DSDL_KMSDRM=OFF -DSDL_OPENGL=OFF -DSDL_OPENGLES=OFF -DSDL_VULKAN=OFF \
  -DSDL_AUDIO=OFF -DSDL_JOYSTICK=OFF -DSDL_HAPTIC=OFF -DSDL_SENSOR=OFF \
  -DSDL_TEST=OFF -DSDL_STATIC=OFF -DSDL_SHARED=ON
"$CMAKE" --build "$BUILD" -j4
grep '#define SDL_VIDEO_DRIVER_X11 1' "$BUILD/include-config-release/SDL2/SDL_config.h"
grep NO_SHARED_MEMORY "$BUILD/CMakeFiles/SDL2.dir/flags.make"
test -f "$BUILD/libSDL2-2.0.so.0"
grep '#define SDL_VIDEO_DRIVER_X11_DYNAMIC_XEXT "libXext.so.6"' "$BUILD/include-config-release/SDL2/SDL_config.h"
printf '%s\n' "Built $BUILD/libSDL2-2.0.so.0; use scripts/run-cloud-window.sh on an actual X11 desktop."
