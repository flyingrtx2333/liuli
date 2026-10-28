#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/live
python3 compiler.py examples/counter_list.eui.json -o build/live/scripted.cpp --model-header build/live/model.h
# Set EUI_CONSOLE_ONLY=1 to build without Cairo even when the renderer is present.
if [ "${EUI_CONSOLE_ONLY:-0}" != 1 ] && [ -f native/glass_renderer.cpp ]; then
    "${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror \
        -DEXPLICIT_UI_GLASS -I build/live -I native native/live_host.cpp native/glass_renderer.cpp native/shaped_text.cpp \
        -Wl,-l:libcairo.so.2 -Wl,-l:libpangocairo-1.0.so.0 -Wl,-l:libpango-1.0.so.0 -Wl,-l:libgobject-2.0.so.0 -Wl,-l:libglib-2.0.so.0 -o build/live/live-host
else
    "${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror \
        -I build/live -I native native/live_host.cpp -o build/live/live-host
fi
printf '%s\n' 'Built build/live/live-host (blocking stdin; quit or EOF exits).'
