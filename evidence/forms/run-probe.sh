#!/bin/sh
cd /workspace/scratch/1d8b07060f6d/explicit-ui-forms
LD_LIBRARY_PATH=/workspace/scratch/1d8b07060f6d/explicit-ui/build/a11y-runtime/usr/lib/x86_64-linux-gnu ./build/forms/forms-atspi-probe --edit >evidence/forms/atspi-probe.log 2>&1
printf '%s\n' "$?" >evidence/forms/atspi-probe.exit
