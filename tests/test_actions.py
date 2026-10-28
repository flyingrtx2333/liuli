"""Compiled named actions use the same atomic model transaction path."""
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
from layout_compiler import compile_layout
ROOT = Path(__file__).resolve().parents[1]
def ref(s): return {'ref':s}
def lit(x,t='int'): return {'literal':x,'type':t}
def op(n,*args): return {'op':n,'args':list(args)}
def document():
    return {'version':1,'states':[
        {'id':'items','type':'texts','initial':['first']},
        {'id':'draft','type':'text','initial':'中文😀'},
        {'id':'selected','type':'int','initial':0},
        {'id':'a','type':'int','initial':1}, {'id':'b','type':'int','initial':2}],
        'derived':[],'views':[{'id':'list','source':'items'},{'id':'draft','source':'draft'},{'id':'a','source':'a'},{'id':'b','source':'b'}],
        'transactions':[], 'actions':[
            {'id':'add_task','set':{'items':op('append',ref('items'),ref('draft')),'draft':lit('','text')}},
            {'id':'remove_task','set':{'items':op('remove_at',ref('items'),ref('selected'))}},
            {'id':'swap','set':{'a':ref('b'),'b':ref('a')}},
            {'id':'noop','set':{}},
            {'id':'same','set':{'a':ref('a')}},
            {'id':'overflow','set':{'draft':lit('not committed','text'),'a':op('add',lit(2**63-1),lit(1))}},
            {'id':'bad_remove','set':{'draft':lit('not committed','text'),'items':op('remove_at',ref('items'),lit(-1))}}]}
class ActionTests(unittest.TestCase):
    def test_native_actions_snapshot_rollback_and_bounds(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);(p/'model.h').write_text(compile_text(json.dumps(document()),model=True))
            (p/'main.cpp').write_text(r'''#include "model.h"
#include <cassert>
#include <functional>
using namespace explicit_ui;
int main(){Model m;
assert(m.state_texts("items")==std::vector<std::string>({"first"}));
bool wrong=false;try{m.state_texts("draft");}catch(const std::invalid_argument&){wrong=true;}assert(wrong);
assert(!m.invoke_action("noop"));assert(!m.invoke_action("same"));
assert(m.invoke_action("add_task"));assert(m.view_texts("list")==std::vector<std::string>({"first",u8"中文😀"}));assert(m.state_text("draft").empty());
assert(m.invoke_action("swap"));assert(m.state_int("a")==2&&m.state_int("b")==1);
auto old=m.view_texts("list");auto stats=m.stats();
for(auto id:{"unknown","overflow","bad_remove"}){bool rejected=false;try{m.invoke_action(id);}catch(const std::exception&){rejected=true;}assert(rejected);assert(m.view_texts("list")==old&&m.state_text("draft").empty());assert(m.stats().patches==stats.patches);}
assert(m.invoke_action("remove_task"));assert(m.view_texts("list")==std::vector<std::string>({u8"中文😀"}));
assert(m.invoke_action("remove_task"));assert(m.view_texts("list").empty());
bool bad=false;try{m.invoke_action("remove_task");}catch(const std::out_of_range&){bad=true;}assert(bad&&m.view_texts("list").empty());
m.apply({{"selected",INT64_MAX}});bad=false;try{m.invoke_action("remove_task");}catch(const std::out_of_range&){bad=true;}assert(bad);
m.apply({{"items",std::vector<std::string>(100000,"x")},{"draft",std::string("still here")}});stats=m.stats();bad=false;try{m.invoke_action("add_task");}catch(const std::length_error&){bad=true;}assert(bad&&m.state_text("draft")=="still here"&&m.view_texts("list").size()==100000&&m.stats().patches==stats.patches);
}
''')
            subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-O0','-Wall','-Wextra','-Wpedantic','-Werror','-I',str(ROOT/'native'),str(p/'main.cpp'),'-o',str(p/'test')],check=True,capture_output=True)
            subprocess.run([str(p/'test')],check=True,timeout=10)
    def test_rejects_malformed_actions_and_types(self):
        cases=[]
        d=document();d['actions']='bad';cases.append(d)
        d=document();d['actions'].append(copy.deepcopy(d['actions'][0]));cases.append(d)
        d=document();d['actions'][0]['id']='bad id';cases.append(d)
        d=document();d['actions'][0]['extra']=0;cases.append(d)
        for update in [{'unknown':lit(0)}, {'draft':lit(0)}, {'items':op('append',ref('items'),lit(0))}, {'items':op('remove_at',ref('items'),lit('x','text'))}, {'items':op('append',ref('missing'),ref('draft'))}]:
            d=document();d['actions'][0]['set']=update;cases.append(d)
        d=document();d['derived']=[{'id':'derived_value','type':'int','expr':lit(2)}];d['actions'][0]['set']={'derived_value':lit(1)};cases.append(d)
        for d in cases:
            for mode in [False,True]:
                with self.subTest(mode=mode,doc=d),self.assertRaises(CompileError):compile_text(json.dumps(d),model=mode)
        with self.assertRaises(CompileError):compile_text(json.dumps(document()).replace('"add_task"','"noop"',1))
    def test_declarative_action_button_binding(self):
        model=(ROOT/'examples/task_list.eui.json').read_text()
        layout=json.loads((ROOT/'examples/task_list.layout.json').read_text())
        output=compile_layout(json.dumps(layout),model)
        self.assertIn('WidgetKind::button',output)
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);(p/'scene.h').write_text(output)
            (p/'test.cpp').write_text('#include "scene.h"\n#include <cassert>\nint main(){auto l=explicit_ui::make_layout();bool found=false;for(auto&w:l.widgets)if(w.id=="add"){assert(w.action=="add_task"&&w.state.empty()&&w.delta==0);found=true;}assert(found);}')
            subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-Wall','-Wextra','-Werror','-I',str(ROOT/'native'),str(p/'test.cpp'),'-o',str(p/'test')],check=True,capture_output=True)
            subprocess.run([str(p/'test')],check=True)
        for changes in [{'action':'missing'}, {'action':''}, {'action':1}, {'state':'selected'}, {'delta':1}]:
            d=copy.deepcopy(layout);d['root']['children'][3]['children'][0].update(changes)
            with self.subTest(changes=changes),self.assertRaises(CompileError):compile_layout(json.dumps(d),model)
        legacy={'version':1,'width':1120,'height':840,'widgets':[{'id':'add','kind':'button','rect':[0,0,100,50],'text':'Add','action':'add_task'}]}
        self.assertIn('WidgetKind::button',compile_layout(json.dumps(legacy),model))

    def test_scripted_operators_and_legacy_optional_field(self):
        d=document();del d['actions'];d['transactions']=[{'label':'append','set':{'items':op('append',ref('items'),ref('draft'))}},{'label':'remove','set':{'items':op('remove_at',ref('items'),lit(0))}}]
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);(p/'script.cpp').write_text(compile_text(json.dumps(d)))
            subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-Wall','-Wextra','-Werror',str(p/'script.cpp'),'-o',str(p/'script')],check=True,capture_output=True)
            result=subprocess.run([str(p/'script')],check=True,capture_output=True,text=True)
            self.assertEqual(json.loads(result.stdout.splitlines()[2])['views']['list'],['中文😀'])
        self.assertIn('invoke_action',compile_text(json.dumps(d),model=True))
if __name__=='__main__':unittest.main()
