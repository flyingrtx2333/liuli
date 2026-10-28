#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
out="$root/build/preview"
mkdir -p "$out/font-cache"
export XDG_CACHE_HOME="$out/font-cache"
python3 "$root/compiler.py" "$root/examples/counter_list.eui.json" -o "$out/runtime.cpp"
g++ -std=c++17 -O2 "$out/runtime.cpp" -o "$out/runtime"
"$out/runtime" > "$out/snapshots.jsonl"
# Build-time bridge only: JSON snapshots become C++ string literals, not a UI runtime.
python3 - "$out" <<'PY'
import json, pathlib, sys
out = pathlib.Path(sys.argv[1])
snapshots = [x for line in (out/'snapshots.jsonl').read_text().splitlines() if 'views' in (x := json.loads(line))]
by_label = {x['label']: x for x in snapshots}
selected = [by_label['initial'], by_label['batch_filter_scroll']]
def cpp(s):
    # Octal byte escapes avoid C++ Unicode UCN/control-character pitfalls.
    return '"' + ''.join('\\%03o' % b for b in s.encode('utf-8')) + '"'
rows = []
for s in selected:
    v = s['views']
    assert isinstance(v['counter'], str) and all(isinstance(x, str) for x in v['list'])
    rows.append('{%s,%s,{%s},%d,%d,%d}' % (cpp(s['label']),cpp(v['counter']),','.join(map(cpp,v['list'])),s['patches'],s['rows_materialized'],sum(s['evaluations'].values())))
(out/'preview_data.inc').write_text('const std::vector<Snapshot> preview_snapshots = {\n' + ',\n'.join(rows) + '\n};\n')
(out/'selected-snapshots.json').write_text(json.dumps(selected, indent=2)+'\n')
assert selected[0]['views'] != selected[1]['views'], 'before/after views must differ'
PY
g++ -std=c++17 -O2 -Wall -Wextra -pedantic -I "$out" "$root/native/cairo_preview.cpp" -Wl,-l:libcairo.so.2 -o "$out/cairo-preview"
"$out/cairo-preview" "$out"
python3 - "$out" <<'PY'
import pathlib, struct, sys
out=pathlib.Path(sys.argv[1])
images=[]
for name in ['before.png','after.png']:
    data=(out/name).read_bytes()
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    assert struct.unpack('>II',data[16:24]) == (1200,900)
    images.append(data)
    print(f'{out/name}: PNG 1200x900, {len(data)} bytes')
assert images[0] != images[1], 'state updates must change rendered output'
PY
