#!/bin/sh
# Explicit task-local library selection. Never fall back to offscreen/dummy.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
LIB="$ROOT/build/qa/sdl-noshm-build"
if [ ! -r "$LIB/libSDL2-2.0.so.0" ]; then
    echo "Missing scoped SDL library; run evidence/v0.4/sdl-noshm-recipe.sh first." >&2
    exit 1
fi
if [ -z "${DISPLAY:-}" ]; then
    echo "DISPLAY is unset; launch from the actual cloud X11 desktop terminal." >&2
    exit 1
fi
if [ "$#" -eq 0 ]; then
    set -- "$ROOT/build/window/window-host"
fi
export SDL_VIDEODRIVER=x11
export LD_LIBRARY_PATH="$LIB${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$@"
