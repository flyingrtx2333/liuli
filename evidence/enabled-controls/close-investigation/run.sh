#!/bin/sh
ROOT=/workspace/scratch/1d8b07060f6d/explicit-ui-controls
OUT="$ROOT/evidence/enabled-controls/close-investigation"
name=$1
binary=${2:-$ROOT/build/enabled-controls/eui-window}
printf '%s\n' "$binary" > "$OUT/$name.binary"
SDL_VIDEODRIVER=x11 LD_LIBRARY_PATH=/workspace/scratch/1d8b07060f6d/explicit-ui/build/qa/sdl-noshm-build "$binary" > "$OUT/$name.stdout" 2> "$OUT/$name.stderr" &
pid=$!
printf '%s\n' "$pid" > "$OUT/$name.pid"
wait "$pid"
printf '%s\n' "$?" > "$OUT/$name.exit"
