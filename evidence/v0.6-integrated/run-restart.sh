#!/bin/sh
cd /workspace/scratch/1d8b07060f6d/explicit-ui || exit 1
export LD_LIBRARY_PATH="$PWD/build/a11y-runtime/usr/lib/x86_64-linux-gnu"
G_DEBUG=fatal-warnings build/v06-forms/eui-accessibility-session-test > evidence/v0.6-integrated/lifecycle.log 2>&1
printf '%s\n' "$?" > evidence/v0.6-integrated/lifecycle.exit
EUI_ACCESSIBILITY=1 G_DEBUG=fatal-warnings scripts/run-cloud-window.sh build/v06-forms/eui-window --state-file "$PWD/evidence/v0.6-integrated/qa.state" > evidence/v0.6-integrated/restart.log 2>&1
printf '%s\n' "$?" > evidence/v0.6-integrated/restart.exit
