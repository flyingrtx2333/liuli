"""Initial single-line UI state obeys the same policy as edits and restores."""
import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from compiler import CompileError, compile_text
from layout_compiler import compile_layout


class InitialInputTests(unittest.TestCase):
    def fixtures(self):
        model = {'version': 1, 'states': [
            {'id': 'query', 'type': 'text', 'initial': ''}],
            'derived': [], 'views': [{'id': 'query', 'source': 'query'}],
            'transactions': []}
        fixed = {'version': 1, 'width': 1120, 'height': 840,
                 'widgets': [{'id': 'input', 'kind': 'search',
                              'rect': [10, 10, 300, 50], 'text': 'Search', 'state': 'query'}]}
        flow = {'version': 2, 'width': 640, 'height': 480, 'root': {
            'id': 'input', 'kind': 'search', 'basis': [0, 50], 'grow': 1,
            'text': 'Search', 'state': 'query'}}
        return model, fixed, flow

    def test_compile_time_initial_field_policy(self):
        model, fixed, flow = self.fixtures()
        for layout in (fixed, flow):
            for value in ('', '中文🙂', 'é' * 128):
                model['states'][0]['initial'] = value
                compile_layout(json.dumps(layout), json.dumps(model))
            for value in ('\0', '\n', '\r', '\t', '\x7f', '\u2028', '\u2029',
                          'é' * 129, '中' * 86):
                model['states'][0]['initial'] = value
                with self.subTest(version=layout['version'], value=value), self.assertRaises(CompileError):
                    compile_layout(json.dumps(layout), json.dumps(model))
        # General model text still supports controls; this restriction belongs to the input widget.
        model['states'][0]['initial'] = 'one\ntwo'
        compile_text(json.dumps(model), model=True)

    @unittest.skipIf(os.environ.get('EUI_CONSOLE_ONLY') == '1', 'graphics backend disabled')
    def test_host_rejects_invalid_initial_text_even_with_cached_scene(self):
        model, fixed, _ = self.fixtures()
        scene = compile_layout(json.dumps(fixed), json.dumps(model))
        invalid = copy.deepcopy(model)
        invalid['states'][0]['initial'] = 'one\ntwo'
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / 'scene.h').write_text(scene)
            (path / 'model.h').write_text(compile_text(json.dumps(invalid), model=True))
            (path / 'test.cpp').write_text('''
#define main eui_original_main
#include "window_host.cpp"
#undef main
#include <cassert>
int main(){Sdl sdl;bool rejected=false;try{App app;}
catch(const std::invalid_argument& e){rejected=std::string(e.what()).find("single-line")!=std::string::npos;}
assert(rejected);}
''')
            command = [os.environ.get('CXX', 'c++'), '-std=c++17', '-O0', '-pthread',
                       '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                       '-I', str(path), '-I', str(ROOT / 'native'), str(path / 'test.cpp'),
                       str(ROOT / 'native/glass_renderer.cpp'), str(ROOT / 'native/shaped_text.cpp')]
            command += ['-Wl,-l:' + name for name in (
                'libSDL2-2.0.so.0', 'libcairo.so.2', 'libpangocairo-1.0.so.0',
                'libpango-1.0.so.0', 'libgobject-2.0.so.0', 'libglib-2.0.so.0')]
            command += ['-o', str(path / 'test')]
            result = subprocess.run(command, text=True, capture_output=True, timeout=120)
            self.assertEqual(result.returncode, 0, result.stderr)
            subprocess.run([str(path / 'test')], check=True, timeout=10,
                           env=dict(os.environ, SDL_VIDEODRIVER='dummy'))


if __name__ == '__main__':
    unittest.main()
