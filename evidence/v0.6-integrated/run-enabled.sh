#!/bin/sh
cd /workspace/scratch/1d8b07060f6d/explicit-ui || exit 1
export LD_LIBRARY_PATH="$PWD/build/a11y-runtime/usr/lib/x86_64-linux-gnu"
EUI_ACCESSIBILITY=1 G_DEBUG=fatal-warnings scripts/run-cloud-window.sh build/v06-enabled/eui-window > evidence/v0.6-integrated/enabled-desktop.log 2>&1
printf '%s\n' "$?" > evidence/v0.6-integrated/enabled-desktop.exit
