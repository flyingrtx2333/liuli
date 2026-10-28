#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/window
python3 compiler.py examples/counter_list.eui.json -o build/window/scripted.cpp --model-header build/window/model.h
python3 layout_compiler.py examples/responsive.layout.json --model examples/counter_list.eui.json -o build/window/scene.h
if command -v pkg-config >/dev/null && pkg-config --exists sdl2 cairo pangocairo; then
    flags=$(pkg-config --cflags --libs sdl2 cairo pangocairo)
else
    flags='-Wl,-l:libSDL2-2.0.so.0 -Wl,-l:libcairo.so.2 -Wl,-l:libpangocairo-1.0.so.0 -Wl,-l:libpango-1.0.so.0 -Wl,-l:libgobject-2.0.so.0 -Wl,-l:libglib-2.0.so.0'
fi
# flags are intentionally split into compiler arguments.
# shellcheck disable=SC2086
"${CXX:-c++}" -std=c++17 -O2 -pthread -Wall -Wextra -Wpedantic -Werror -I build/window -I native native/window_host.cpp native/glass_renderer.cpp native/shaped_text.cpp $flags -o build/window/window-host
printf '%s\n' 'Built build/window/window-host. Requires a graphical desktop; SDL_VIDEODRIVER=dummy is test-only.'
