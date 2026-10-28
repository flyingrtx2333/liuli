#!/bin/sh
cd /workspace/scratch/1d8b07060f6d/explicit-ui || exit
export LD_LIBRARY_PATH="$PWD/build/a11y-runtime/usr/lib/x86_64-linux-gnu"
G_DEBUG=fatal-warnings build/a11y-integrated/eui-atspi-probe > evidence/v0.5-integrated/probe.log 2>&1
echo $? > evidence/v0.5-integrated/probe.exit
