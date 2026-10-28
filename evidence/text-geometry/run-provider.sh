#!/bin/sh
cd /workspace/scratch/1d8b07060f6d/explicit-ui-accessible-text || exit
export LD_LIBRARY_PATH=/workspace/scratch/1d8b07060f6d/explicit-ui/build/a11y-runtime/usr/lib/x86_64-linux-gnu
G_DEBUG=fatal-warnings EUI_ACCESSIBILITY=1 /workspace/scratch/1d8b07060f6d/explicit-ui/scripts/run-cloud-window.sh "$PWD/build/a11y/eui-window" > evidence/text-geometry/provider.log 2>&1
echo $? > evidence/text-geometry/provider.exit
