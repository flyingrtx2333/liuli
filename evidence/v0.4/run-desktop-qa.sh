#!/bin/sh
cd /workspace/scratch/1d8b07060f6d/explicit-ui
./build/qa/combined-intermediate --state-file evidence/v0.4/qa-state.eui > evidence/v0.4/actual-combined.log 2>&1 &
p=$!
sleep 2
python3 evidence/v0.3/measure-desktop.py "$p" > evidence/v0.4/actual-combined-idle.json
wait "$p"
echo EXIT=$? >> evidence/v0.4/actual-combined.log
