cd /workspace/scratch/1d8b07060f6d/explicit-ui
SDL_VIDEODRIVER=x11 LD_LIBRARY_PATH="$PWD/build/qa/sdl-noshm-build" ./build/task-list/eui-window --state-file evidence/v0.4/qa-tasks.eui >> evidence/v0.4/actual-task-list.log 2>&1
echo EXIT=$? >> evidence/v0.4/actual-task-list.log
