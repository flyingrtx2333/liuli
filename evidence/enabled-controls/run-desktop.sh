#!/bin/sh
cd /workspace/scratch/1d8b07060f6d/explicit-ui-controls
SDL_VIDEODRIVER=x11 LD_LIBRARY_PATH=/workspace/scratch/1d8b07060f6d/explicit-ui/build/qa/sdl-noshm-build ./build/enabled-controls/eui-window > evidence/enabled-controls/desktop.log 2>&1
printf '%s\n' "$?" > evidence/enabled-controls/desktop-exit.txt
