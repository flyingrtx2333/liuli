#!/bin/sh
cd /workspace/scratch/1d8b07060f6d/explicit-ui || exit 1
LD_LIBRARY_PATH="$PWD/build/a11y-runtime/usr/lib/x86_64-linux-gnu" G_DEBUG=fatal-warnings build/v06-forms/forms-atspi-probe --edit > evidence/v0.6-integrated/forms-atspi.log 2>&1
printf '%s\n' "$?" > evidence/v0.6-integrated/forms-atspi.exit
