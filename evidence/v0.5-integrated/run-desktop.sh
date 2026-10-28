#!/bin/sh
cd /workspace/scratch/1d8b07060f6d/explicit-ui || exit
export LD_LIBRARY_PATH="$PWD/build/a11y-runtime/usr/lib/x86_64-linux-gnu"
G_DEBUG=fatal-warnings build/a11y-integrated/eui-accessibility-session-test > evidence/v0.5-integrated/lifecycle.log 2>&1
echo $? > evidence/v0.5-integrated/lifecycle.exit
(EUI_ACCESSIBILITY=1 G_DEBUG=fatal-warnings scripts/run-cloud-window.sh build/a11y-integrated/eui-window --state-file "$PWD/evidence/v0.5-integrated/qa.state" > evidence/v0.5-integrated/provider.log 2>&1; echo $? > evidence/v0.5-integrated/provider.exit) &
