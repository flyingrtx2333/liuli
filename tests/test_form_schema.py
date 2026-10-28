"""Bounded input schema and explicit edit reset policy, both layout versions."""
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from compiler import CompileError
from layout_compiler import compile_layout

class FormSchemaTests(unittest.TestCase):
    def fixtures(self, version):
        model = json.loads((ROOT/'examples/counter_list.eui.json').read_text())
        layout = json.loads((ROOT/'examples/window.layout.json').read_text())
        leaves = [dict(layout['widgets'][6], id='legacy'),
                  dict(layout['widgets'][6], id='plain', kind='input'),
                  dict(layout['widgets'][6], id='explicit', kind='input', reset_states=['offset']),
                  dict(layout['widgets'][6], id='disabled_reset', reset_states=[]),
                  layout['widgets'][7]]
        if version == 1:
            layout['widgets'] = leaves
        else:
            for w in leaves:
                del w['rect']; w.update(basis=[0,52],grow=0)
            layout = dict(version=2,width=640,height=480,root=dict(kind='column',basis=[0,0],grow=1,padding=10,gap=4,stack_below=0,children=leaves))
        return model,layout,leaves
    def compile(self, model, layout):
        return compile_layout(json.dumps(layout),json.dumps(model))
    def test_reset_metadata_and_alias_reach_native_layout(self):
        for version in (1,2):
            model,layout,_ = self.fixtures(version)
            with tempfile.TemporaryDirectory() as directory:
                p=Path(directory);(p/'scene.h').write_text(self.compile(model,layout))
                (p/'test.cpp').write_text('''#include "scene.h"
#include <cassert>
int main(){auto l=explicit_ui::make_layout();auto& w=l.widgets;
assert(w[0].reset_states==std::vector<std::string>{"offset"});
assert(w[1].reset_states.empty());assert(w[1].kind==explicit_ui::WidgetKind::search);
assert(w[2].reset_states==std::vector<std::string>{"offset"});assert(w[3].reset_states.empty());}
''')
                subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-I',str(ROOT/'native'),str(p/'test.cpp'),'-o',str(p/'test')],check=True,capture_output=True)
                subprocess.run([str(p/'test')],check=True)
    def test_limits_and_initial_policy(self):
        for version in (1,2):
            model,layout,leaves=self.fixtures(version)
            template=copy.deepcopy(leaves[1]);leaves[:]=[dict(template,id='input'+str(i)) for i in range(16)]
            self.compile(model,layout)
            leaves.append(dict(template,id='overflow'))
            with self.assertRaises(CompileError):self.compile(model,layout)
            leaves.pop()
            for bad in ['x\ny','x\u2028y','中'*86]:
                m=copy.deepcopy(model)
                next(s for s in m['states'] if s['id']=='query')['initial']=bad
                with self.assertRaises(CompileError):self.compile(m,layout)
    def test_reset_rejections(self):
        for version in (1,2):
            for bad in ('offset', ['offset','offset'], ['query'], ['missing'], [True], [None]):
                model,layout,leaves=self.fixtures(version);leaves[1]['reset_states']=bad
                with self.subTest(version=version,bad=bad),self.assertRaises(CompileError):self.compile(model,layout)
            model,layout,leaves=self.fixtures(version);leaves[-1]['reset_states']=[]
            with self.assertRaises(CompileError):self.compile(model,layout)
    def test_example_compiles(self):
        compile_layout((ROOT/'examples/form.layout.json').read_text(),(ROOT/'examples/form.eui.json').read_text())

if __name__=='__main__':unittest.main()
