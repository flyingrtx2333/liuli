"""Real persistent child process tests, not replaying compile-time transactions."""
import json
import os
from pathlib import Path
import selectors
import subprocess
import sys
import tempfile
import time
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from compiler import compile_text
ROOT = Path(__file__).resolve().parents[1]

class LiveTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.directory = Path(cls.temp.name)
        cls.build((ROOT / 'examples/counter_list.eui.json').read_text(), 'host', ROOT / 'native/live_host.cpp')

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    @classmethod
    def build(cls, source, name, cpp):
        (cls.directory / 'model.h').write_text(compile_text(source, model=True))
        subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', '-O0', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I', str(cls.directory), '-I', str(ROOT / 'native'), str(cpp), '-o', str(cls.directory / name)], check=True, capture_output=True)

    def setUp(self):
        self.process = subprocess.Popen([str(self.directory / 'host')], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.initial = self.read()

    def tearDown(self):
        if self.process.poll() is None:
            self.process.kill()
        self.process.communicate(timeout=5)
        for pipe in (self.process.stdin, self.process.stdout, self.process.stderr):
            if pipe is not None:
                pipe.close()

    def read(self):
        with selectors.DefaultSelector() as selector:
            selector.register(self.process.stdout, selectors.EVENT_READ)
            self.assertTrue(selector.select(5), 'host response timed out')
        return json.loads(self.process.stdout.readline())

    def command(self, line):
        self.process.stdin.write(line.encode('utf-8') + b'\n')
        self.process.stdin.flush()
        return self.read()

    def test_persists_external_updates_batch_and_render_suppression(self):
        time.sleep(.05)
        self.assertIsNone(self.process.poll(), 'must stay alive with stdin open')
        with selectors.DefaultSelector() as selector:
            selector.register(self.process.stdout, selectors.EVENT_READ)
            self.assertFalse(selector.select(.05), 'idle host must not emit frames')
        unchanged = self.command('set-int count 0')
        unrelated = self.command('set-int unrelated 4')
        for result in [unchanged, unrelated]:
            self.assertEqual(result['renders'], 1)
            self.assertEqual(result['evaluations'], self.initial['evaluations'])
        updated = self.command('set-int count 7')
        self.assertEqual(updated['views']['counter'], 'Count: 7')
        self.assertEqual(updated['renders'], 2)
        self.command('begin')
        self.command('set-text query "99"')
        self.command('set-int offset 2')
        self.assertEqual(self.command('snapshot')['views'], updated['views'])
        batch = self.command('commit')
        self.assertEqual(batch['renders'], 3)
        self.assertEqual(batch['evaluations'], dict(title=2, filtered=2, visible=2))
        expected = [f'Item {i:05d}' for i in range(10000) if '99' in f'Item {i:05d}'][2:14]
        self.assertEqual(batch['views']['list'], expected)
        self.command('set-int offset 0')
        one = self.command('set-text query "Item 00099"')
        equivalent = self.command('set-text query "00099"')
        self.assertEqual(equivalent['renders'], one['renders'])
        self.assertEqual(equivalent['evaluations']['visible'], one['evaluations']['visible'])
        self.assertEqual(self.command('quit'), {'status': 'bye'})
        self.assertEqual(self.process.wait(timeout=5), 0)

    def test_error_rollback_unknown_types_and_commands(self):
        for command in ['set-int missing 1', 'set-text count "bad"', 'set-int count 9223372036854775808', 'set-int count 1 trailing', 'set-text query unquoted', 'set-text query "unfinished', 'bad', '', 'commit', 'quit trailing']:
            with self.subTest(command=command):
                self.assertIn('error', self.command(command))
                snapshot = self.command('snapshot')
                self.assertEqual(snapshot['views'], self.initial['views'])
                self.assertEqual(snapshot['evaluations'], self.initial['evaluations'])
                self.assertEqual(snapshot['renders'], 1)
        self.command('begin')
        self.command('set-int count 99')
        self.command('set-int offset -1')
        self.assertIn('invalid window', self.command('commit')['error'])
        snapshot = self.command('snapshot')
        self.assertEqual(snapshot['views'], self.initial['views'])
        self.assertEqual(snapshot['evaluations'], self.initial['evaluations'])
        self.assertEqual(snapshot['renders'], 1)
        self.assertIn('error', self.command('commit'))
        self.assertEqual(self.command('set-int count 1')['views']['counter'], 'Count: 1')

    def test_batch_abort_last_write_eof_and_invalid_utf8(self):
        self.command('begin'); self.command('set-int count 1'); self.command('set-int count 0')
        self.assertEqual(self.command('commit')['renders'], 1)
        self.command('begin'); self.command('set-int count 2')
        self.assertIn('error', self.command('bad'))
        self.assertIn('error', self.command('commit'))
        self.process.stdin.write(b'set-text query "\xff"\n'); self.process.stdin.flush()
        self.assertIn('UTF-8', self.read()['error'])
        self.command('begin'); self.command('set-int count 2'); self.command('abort')
        self.assertEqual(self.command('snapshot')['renders'], 1)
        self.command('begin'); self.command('set-int count 3')
        self.process.stdin.close()
        self.assertEqual(self.read(), {'status': 'eof'})
        self.assertEqual(self.process.wait(timeout=5), 0)
        # communicate cannot flush a deliberately closed stdin.
        self.process.stdin = None

    def test_oversized_line_recovery_and_final_line_without_newline(self):
        self.command('begin'); self.command('set-int count 8')
        self.assertIn('size limit', self.command('x' * (1024 * 1024 + 100))['error'])
        self.assertIn('error', self.command('commit'))
        self.assertEqual(self.command('snapshot')['views'], self.initial['views'])
        self.process.stdin.write(b'set-int count 9'); self.process.stdin.flush()
        self.process.stdin.close()
        # Read both final outputs together: buffered readline can hide the second
        # line from select when the child exits immediately after it writes EOF.
        self.assertEqual(self.process.wait(timeout=5), 0)
        output = self.process.stdout.read().splitlines()
        self.assertEqual(json.loads(output[0])['views']['counter'], 'Count: 9')
        self.assertEqual(json.loads(output[1]), {'status': 'eof'})
        self.assertEqual(self.process.wait(timeout=5), 0)
        self.process.stdin = None

    def test_empty_and_typed_api_headers_build(self):
        empty = dict(version=1, states=[], derived=[], views=[], transactions=[])
        cpp = self.directory / 'api.cpp'
        cpp.write_text('#include "model.h"\nint main(){explicit_ui::Model m; return m.apply({}) || m.stats().patches != 0;}')
        self.build(json.dumps(empty), 'empty', cpp)
        subprocess.run([str(self.directory / 'empty')], check=True)
        typed = dict(version=1, states=[dict(id='n', type='int', initial=0), dict(id='rows', type='texts', initial=[])], derived=[dict(id='plus', type='int', expr=dict(op='add', args=[dict(ref='n'), dict(literal=1, type='int')]))], views=[dict(id='number', source='plus'), dict(id='list', source='rows')], transactions=[])
        cpp.write_text('''#include "model.h"
#include <cassert>
int main(){explicit_ui::Model m;
assert(m.apply({{"rows", std::vector<std::string>{"hello", "世界"}}}));
assert(m.view_texts("list").size()==2);
try { m.apply({{"n", INT64_MAX}}); assert(false); } catch(const std::overflow_error&) {}
assert(m.view_int("number")==1);
assert(!m.apply({}));
}''')
        self.build(json.dumps(typed), 'typed', cpp)
        subprocess.run([str(self.directory / 'typed')], check=True)

if __name__ == '__main__': unittest.main()
