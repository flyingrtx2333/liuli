cd /workspace/scratch/1d8b07060f6d/explicit-ui
scripts/run-cloud-window.sh build/task-list/eui-window --state-file evidence/v0.4/qa-corrupt-state.eui > evidence/v0.4/actual-corrupt-task.log 2>&1
echo EXIT=$? >> evidence/v0.4/actual-corrupt-task.log
