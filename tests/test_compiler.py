import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from compiler import CompileError, compile_text
ROOT = Path(__file__).resolve().parents[1]

def ref(name): return {'ref': name}
def lit(value, typ='int'): return {'literal': value, 'type': typ}
def op(name, *args): return {'op': name, 'args': list(args)}

class CompilerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.doc = json.loads((ROOT / 'examples/counter_list.eui.json').read_text())
        cls.temp = tempfile.TemporaryDirectory()
        cls.counter = 0

    @classmethod
    def tearDownClass(cls): cls.temp.cleanup()

    def run_native(self, doc):
        type(self).counter += 1
        stem = Path(self.temp.name) / str(self.counter)
        stem.with_suffix('.cpp').write_text(compile_text(json.dumps(doc)))
        subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', '-O0', '-Wall', '-Wextra', '-Werror', str(stem.with_suffix('.cpp')), '-o', str(stem)], check=True, capture_output=True)
        return subprocess.run([str(stem)], text=True, capture_output=True, timeout=5)

    def test_native_dependency_batch_virtualization_lifecycle(self):
        result = self.run_native(self.doc)
        self.assertEqual(result.returncode, 0, result.stderr)
        out = [json.loads(line) for line in result.stdout.splitlines()]
        initial, unchanged, unrelated, increment, batch, single, same, empty, lifecycle = out
        self.assertEqual(initial['evaluations'], dict(title=1, filtered=1, visible=1))
        for snap in [unchanged, unrelated]:
            self.assertEqual(snap['evaluations'], initial['evaluations'])
            self.assertEqual(snap['patches'], 2)
            self.assertEqual(snap['rows_materialized'], 12)
        self.assertEqual(increment['evaluations'], dict(title=2, filtered=1, visible=1))
        self.assertEqual(increment['views']['counter'], 'Count: 1')
        self.assertEqual(batch['evaluations'], dict(title=2, filtered=2, visible=2))
        expected = [f'Item {i:05d}' for i in range(10000) if '99' in f'Item {i:05d}'][2:14]
        self.assertEqual(batch['views']['list'], expected)
        self.assertEqual(batch['rows_materialized'], 24)
        self.assertEqual(single['views']['list'], ['Item 00099'])
        self.assertEqual(same['evaluations'], dict(title=2, filtered=4, visible=3))
        self.assertEqual(same['patches'], single['patches'])
        self.assertEqual(empty['evaluations'], same['evaluations'])
        self.assertEqual(lifecycle, {'lifecycle': {'mounts': 2, 'unmounts': 2}})
        # Process terminates within timeout; generated core has no polling/timer/event loop.

    def minimal(self):
        return {'version': 1, 'states': [{'id': 'a', 'type': 'int', 'initial': 1}, {'id': 'b', 'type': 'int', 'initial': 2}], 'derived': [], 'views': [{'id': 'a', 'source': 'a'}, {'id': 'b', 'source': 'b'}], 'transactions': []}

    def test_simultaneous_snapshot_and_safe_unicode_strings(self):
        doc = self.minimal()
        value = '中文"; system("false"); //\\\n\x00'
        doc['states'].append({'id': 'str', 'type': 'text', 'initial': value})
        doc['views'].append({'id': 'str', 'source': 'str'})
        doc['transactions'] = [{'label': 'swap', 'set': {'a': ref('b'), 'b': ref('a')}}]
        result = self.run_native(doc)
        self.assertEqual(result.returncode, 0, result.stderr)
        out = [json.loads(line) for line in result.stdout.splitlines()]
        self.assertEqual(out[1]['views'], {'a': 2, 'b': 1, 'str': value})

    def test_integer_overflow_fails_defined(self):
        doc = self.minimal()
        doc['transactions'] = [{'label': 'overflow', 'set': {'a': op('add', lit(2**63-1), lit(1))}}]
        result = self.run_native(doc)
        self.assertEqual(result.returncode, 2)
        self.assertIn('integer overflow', result.stderr)
        self.assertEqual(len(result.stdout.splitlines()), 1)

    def test_window_bounds(self):
        doc = self.minimal()
        doc['derived'] = [{'id': 'bad', 'type': 'texts', 'expr': op('window', lit(['a'], 'texts'), lit(-1), lit(1))}]
        result = self.run_native(doc)
        self.assertEqual(result.returncode, 2)
        self.assertIn('invalid window', result.stderr)

    def test_reject_malformed_types_references_and_code(self):
        cases = []
        for value in [True, 1.0, '1', 2**63]:
            doc = self.minimal(); doc['states'][0]['initial'] = value; cases.append(doc)
        for expr in [ref('missing'), op('system', lit('x', 'text')), op('add', lit('x', 'text'), lit(1)), op('add', lit(1)), {'raw_cpp': 'exit(0)'}]:
            doc = self.minimal(); doc['derived'] = [{'id': 'd', 'type': 'int', 'expr': expr}]; cases.append(doc)
        doc = self.minimal(); doc['states'][0]['id'] = 'a;exit(0)'; cases.append(doc)
        doc = self.minimal(); doc['states'][1]['id'] = 'a'; cases.append(doc)
        doc = self.minimal(); doc['extra'] = 1; cases.append(doc)
        doc = self.minimal(); doc['transactions'] = [{'label': 'bad', 'set': {'b': lit('x', 'text')}}]; cases.append(doc)
        doc = self.minimal(); doc['derived'] = [{'id': 'cycle', 'type': 'int', 'expr': ref('cycle')}]; cases.append(doc)
        for doc in cases:
            with self.subTest(doc=doc), self.assertRaises(CompileError): compile_text(json.dumps(doc))
        with self.assertRaises(CompileError): compile_text('{"version":1,"version":1}')

    def test_unused_declarations_build_warning_free(self):
        doc = self.minimal()
        doc['views'] = []
        doc['derived'] = [{'id': 'unused', 'type': 'int', 'expr': lit(3)}]
        self.assertEqual(self.run_native(doc).returncode, 0)

    def test_invalid_unicode_metadata_rejected(self):
        for field in ['view', 'label']:
            doc = self.minimal()
            if field == 'view': doc['views'][0]['id'] = '\ud800'
            else: doc['transactions'] = [{'label': '\ud800', 'set': {}}]
            with self.assertRaises(CompileError): compile_text(json.dumps(doc))

    def test_compiler_deterministic(self):
        source = json.dumps(self.doc)
        self.assertEqual(compile_text(source), compile_text(source))

if __name__ == '__main__': unittest.main(verbosity=2)
