cd /workspace/scratch/1d8b07060f6d/explicit-ui
SDL_VIDEODRIVER=x11 LD_LIBRARY_PATH="$PWD/build/qa/sdl-noshm-build" ./build/window/window-host --state-file evidence/v0.4/qa-final-state.eui > evidence/v0.4/actual-final-host.log 2>&1 &
p=$!
sleep 2
python3 evidence/v0.3/measure-desktop.py "$p" > evidence/v0.4/actual-final-idle.json
wait "$p"
echo EXIT=$? >> evidence/v0.4/actual-final-host.log
