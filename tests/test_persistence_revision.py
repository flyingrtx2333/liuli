"""Persistence revision tracks final persisted values, not affected view repaint flags."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from compiler import compile_text


class PersistenceRevisionTests(unittest.TestCase):
    def test_revision_tracks_atomic_final_persistent_values(self):
        model = {'version': 1, 'states': [
            {'id': 'hidden', 'type': 'int', 'initial': 0},
            {'id': 'other', 'type': 'text', 'initial': 'a'},
            {'id': 'transient', 'type': 'int', 'initial': 0}],
            'derived': [], 'views': [], 'transactions': [], 'persist': ['hidden', 'other']}
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            (directory / 'model.h').write_text(compile_text(json.dumps(model), model=True))
            (directory / 'test.cpp').write_text(r'''
#include "model.h"
#include <cassert>
int main(){using namespace explicit_ui;Model m;assert(m.persistence_revision()==0);
assert(!m.apply({{"hidden",std::int64_t{1}}}));assert(m.persistence_revision()==1);
m.apply({{"hidden",std::int64_t{1}}});assert(m.persistence_revision()==1);
m.apply({{"transient",std::int64_t{2}}});assert(m.persistence_revision()==1);
m.apply({{"hidden",std::int64_t{2}},{"other",std::string("b")}});assert(m.persistence_revision()==2);
m.apply({{"hidden",std::int64_t{3}},{"hidden",std::int64_t{2}}});assert(m.persistence_revision()==2);
bool rejected=false;try{m.apply({{"hidden",std::int64_t{4}},{"other",std::int64_t{7}}});}catch(const std::invalid_argument&){rejected=true;}
assert(rejected&&m.persistence_revision()==2&&m.state_int("hidden")==2&&m.state_text("other")=="b");
m.apply({{"hidden",std::int64_t{0}},{"other",std::string("a")}});assert(m.persistence_revision()==3);
Model unpersisted;assert(unpersisted.persistence_revision()==0);
}
''')
            command = [os.environ.get('CXX', 'c++'), '-std=c++17', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                       '-I', str(directory), '-I', str(ROOT / 'native'), str(directory / 'test.cpp'), '-o', str(directory / 'test')]
            result = subprocess.run(command, capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stderr)
            subprocess.run([str(directory / 'test')], check=True, timeout=10)


if __name__ == '__main__':
    unittest.main()
