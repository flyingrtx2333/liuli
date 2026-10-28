cd /workspace/scratch/1d8b07060f6d/explicit-ui
scripts/run-cloud-window.sh build/window/window-host --state-file evidence/v0.4/qa-async-responsive-state.eui > evidence/v0.4/actual-async-responsive.log 2>&1 &
p=$!
sleep 2
python3 evidence/v0.3/measure-desktop.py "$p" > evidence/v0.4/actual-async-responsive-idle.json
wait "$p"
echo EXIT=$? >> evidence/v0.4/actual-async-responsive.log
