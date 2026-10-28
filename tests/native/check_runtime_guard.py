#!/usr/bin/env python3
"""Opt-in developer check; no display, session bus, or bridge initialization.
Usage: check_runtime_guard.py <runtime-check> <session-test> <fixed-runtime-dir>
Requires the known-old system runtime plus the staged fixed trio for this test.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

check, session, fixed = (Path(arg).resolve() for arg in sys.argv[1:])
base = os.environ.copy()
for name in ("DISPLAY", "WAYLAND_DISPLAY", "DBUS_SESSION_BUS_ADDRESS", "AT_SPI_BUS_ADDRESS", "LD_LIBRARY_PATH"):
    base.pop(name, None)

def run(binary, env, expected, fragment):
    result = subprocess.run([str(binary)], env=env, capture_output=True, text=True, timeout=10)
    output = result.stdout + result.stderr
    assert result.returncode == expected, (result.returncode, output)
    assert fragment in output, output
    print(output.strip())
    print(f"exit={result.returncode}")

run(check, base, 1, "runtime rejected: ATK reports")
run(session, base, 1, "runtime rejected: ATK reports")
with tempfile.TemporaryDirectory(prefix="eui-atk-only-") as temp:
    # Deliberate mismatch is inspected only by the pure guard, never initialized.
    (Path(temp) / "libatk-1.0.so.0").symlink_to(fixed / "libatk-1.0.so.0")
    run(check, dict(base, LD_LIBRARY_PATH=temp), 1, "runtime rejected: AT-SPI reports")
run(check, dict(base, LD_LIBRARY_PATH=str(fixed)), 0, "PASS supported ATK/AT-SPI versions")
print("PASS old coherent runtime and old AT-SPI rejected; constructor rejects before bus; fixed API versions pass")
