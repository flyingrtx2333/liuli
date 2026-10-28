#!/bin/sh
cd /workspace/scratch/1d8b07060f6d/explicit-ui-accessible-text || exit
export LD_LIBRARY_PATH=/workspace/scratch/1d8b07060f6d/explicit-ui/build/a11y-runtime/usr/lib/x86_64-linux-gnu
xwininfo -name 'Explicit UI / native interaction' > evidence/text-geometry/xwininfo.txt
export EUI_EXPECT_CLIENT_X=$(sed -n 's/.*Absolute upper-left X: *//p' evidence/text-geometry/xwininfo.txt)
export EUI_EXPECT_CLIENT_Y=$(sed -n 's/.*Absolute upper-left Y: *//p' evidence/text-geometry/xwininfo.txt)
G_DEBUG=fatal-warnings build/a11y/eui-atspi-probe > evidence/text-geometry/probe.log 2>&1
echo $? > evidence/text-geometry/probe.exit
