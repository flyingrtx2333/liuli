#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
python3 -m unittest discover -s tests -v
python3 compiler.py examples/counter_list.eui.json -o build/demo.cpp
"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror build/demo.cpp -o build/demo
./build/demo > build/metrics.jsonl
cat build/metrics.jsonl
./scripts/build-live.sh
printf 'set-int count 7\nquit\n' | ./build/live/live-host > build/live/smoke.jsonl
python3 - <<'PY'
import json
from pathlib import Path
lines = [json.loads(line) for line in Path('build/live/smoke.jsonl').read_text().splitlines()]
assert lines[0]['renders'] == 1
assert lines[1]['views']['counter'] == 'Count: 7' and lines[1]['renders'] == 2
assert lines[2] == {'status': 'bye'}
PY
