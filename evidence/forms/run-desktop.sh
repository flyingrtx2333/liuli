#!/bin/sh
cd /workspace/scratch/1d8b07060f6d/explicit-ui-forms
export EUI_ACCESSIBILITY=1 G_DEBUG=fatal-warnings
export LD_LIBRARY_PATH=/workspace/scratch/1d8b07060f6d/explicit-ui/build/a11y-runtime/usr/lib/x86_64-linux-gnu
/workspace/scratch/1d8b07060f6d/explicit-ui/scripts/run-cloud-window.sh "$PWD/build/forms/eui-window" --state-file "$PWD/build/forms/desktop.state" > "$PWD/evidence/forms/desktop.log" 2>&1
printf '%s\n' "$?" > "$PWD/evidence/forms/desktop.exit"
