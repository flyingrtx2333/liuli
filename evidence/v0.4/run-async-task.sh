cd /workspace/scratch/1d8b07060f6d/explicit-ui
scripts/run-cloud-window.sh build/task-list/eui-window --state-file evidence/v0.4/qa-async-tasks.eui >> evidence/v0.4/actual-async-task.log 2>&1 &
p=$!
sleep 2
python3 evidence/v0.3/measure-desktop.py "$p" > evidence/v0.4/actual-async-task-idle.json
wait "$p"
echo EXIT=$? >> evidence/v0.4/actual-async-task.log
