import copy
import json
import subprocess
import tempfile
import unittest
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from layout_compiler import compile_layout
from compiler import CompileError

class LayoutTests(unittest.TestCase):
    def setUp(self):
        self.doc=json.loads((ROOT/'examples/window.layout.json').read_text())
        self.model=(ROOT/'examples/counter_list.eui.json').read_text()
    def compile(self,doc):
        return compile_layout(json.dumps(doc),self.model)
    def test_generated_layout_and_half_open_hit_test(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)
            (p/'scene.h').write_text(self.compile(self.doc))
            (p/'test.cpp').write_text('''#include "scene.h"
#include <cassert>
int main(){auto l=explicit_ui::make_layout();
assert(explicit_ui::hit_test(l,108,365)->id=="decrement");
assert(explicit_ui::hit_test(l,215,416)->id=="decrement");
assert(!explicit_ui::hit_test(l,216,417));
assert(explicit_ui::hit_test(l,390,270)->state=="query");
assert(!explicit_ui::hit_test(l,1028,270));}
''')
            subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-I',str(ROOT/'native'),str(p/'test.cpp'),'-o',str(p/'test')],check=True,capture_output=True)
            subprocess.run([str(p/'test')],check=True)
    def test_rejects_wrong_types_unknown_fields_and_bounds(self):
        cases=[]
        d=copy.deepcopy(self.doc);d['widgets'][3]['state']='query';cases.append(d)
        d=copy.deepcopy(self.doc);d['widgets'][5]['binding']='list';cases.append(d)
        d=copy.deepcopy(self.doc);d['widgets'][7]['binding']='counter';cases.append(d)
        d=copy.deepcopy(self.doc);d['widgets'][6]['state']='count';cases.append(d)
        d=copy.deepcopy(self.doc);d['widgets'][0]['rect']=[0,0,1121,40];cases.append(d)
        d=copy.deepcopy(self.doc);d['widgets'][3]['delta']=True;cases.append(d)
        d=copy.deepcopy(self.doc);d['widgets'][0]['extra']='x';cases.append(d)
        d=copy.deepcopy(self.doc);d['widgets'][0]['id']='search';cases.append(d)
        d=copy.deepcopy(self.doc);d['widgets'].extend(dict(d['widgets'][6],id='search'+str(i)) for i in range(16));cases.append(d)
        d=copy.deepcopy(self.doc);d['widgets'][0]['id']='bad\0id';cases.append(d)
        d=copy.deepcopy(self.doc);d['widgets'][7]['delta']=-1;cases.append(d)
        d=copy.deepcopy(self.doc);d['widgets'][7]['delta']=101;cases.append(d)
        for d in cases:
            with self.subTest(doc=d),self.assertRaises(CompileError):self.compile(d)
    def test_duplicate_json_keys_rejected(self):
        with self.assertRaises(CompileError):compile_layout('{"version":1,"version":1}',self.model)
    def test_declared_position_text_and_actions_reach_generated_cpp(self):
        d=copy.deepcopy(self.doc)
        d['widgets'][3].update(rect=[100,300,110,55],delta=-2,text='two')
        result=self.compile(d)
        self.assertIn('WidgetKind::button,100,300,110,55',result)
        self.assertIn(',-2}',result)

if __name__=='__main__':unittest.main()
