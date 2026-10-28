import copy
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from layout_compiler import compile_layout
from compiler import CompileError

class ResponsiveLayoutTests(unittest.TestCase):
    def setUp(self):
        self.doc=json.loads((ROOT/'examples/responsive.layout.json').read_text())
        self.model=(ROOT/'examples/counter_list.eui.json').read_text()
    def compile(self,doc):return compile_layout(json.dumps(doc),self.model)
    def test_flow_resize_bounds_hit_testing_and_breakpoint(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);(p/'scene.h').write_text(self.compile(self.doc))
            (p/'check.cpp').write_text('''#include "scene.h"
#include <cassert>
#include <algorithm>
using namespace explicit_ui;
const Widget& find(const Layout& l,const char* id){return *std::find_if(l.widgets.begin(),l.widgets.end(),[&](const Widget&w){return w.id==id;});}
int main(){
for(const auto& size:std::vector<std::pair<int,int>>{{1,1},{20,30},{320,320},{520,740},{800,600},{1120,840},{1920,1080},{4096,4096}}){
 auto l=make_layout(size.first,size.second);assert(l.width==size.first&&l.height==size.second);
 for(const auto&w:l.widgets){assert(w.x>=0&&w.y>=0&&w.width>=0&&w.height>=0);assert(w.x+w.width<=l.width&&w.y+w.height<=l.height);}
}
auto wide=make_layout(1120,840);auto narrow=make_layout(520,740);
assert(find(wide,"search").x>find(wide,"increment").x);
assert(find(narrow,"search").y>find(narrow,"increment").y);
assert(find(narrow,"search").height==52); // Not proportional whole-image scaling.
for(const auto* l:{&wide,&narrow}){
 const auto& b=find(*l,"increment");assert(hit_test(*l,b.x+b.width/2,b.y+b.height/2)->id==b.id);
 assert(hit_test(*l,b.x+b.width,b.y+b.height)==nullptr);
}
bool failed=false;try{make_layout(0,10);}catch(const std::invalid_argument&){failed=true;}assert(failed);
failed=false;try{make_layout(4097,10);}catch(const std::invalid_argument&){failed=true;}assert(failed);
}
''')
            subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-I',str(ROOT/'native'),str(p/'check.cpp'),'-o',str(p/'check')],check=True,capture_output=True)
            subprocess.run([str(p/'check')],check=True)
    def test_invalid_flow_constraints(self):
        cases=[]
        for key,value in [('grow',True),('grow',-1),('basis',[0,-1]),('gap',-1),('stack_below',True),('children',[]),('padding',4097),('extra',0)]:
            d=copy.deepcopy(self.doc);d['root'][key]=value;cases.append(d)
        d=copy.deepcopy(self.doc);d['root']['stack_below']=400;cases.append(d)
        d=copy.deepcopy(self.doc);d['root']['children'][0]['binding']='list';cases.append(d)
        d=copy.deepcopy(self.doc);d['root']['children'][0]['id']='search';cases.append(d)
        d=copy.deepcopy(self.doc);d['root']['children'][0]['id']='bad\0id';cases.append(d)
        d=copy.deepcopy(self.doc);d['width']=0;cases.append(d)
        for d in cases:
            with self.subTest(doc=d),self.assertRaises(CompileError):self.compile(d)
    def test_depth_bound_and_determinism(self):
        self.assertEqual(self.compile(self.doc),self.compile(self.doc))
        d=copy.deepcopy(self.doc)
        for _ in range(18):d['root']=dict(kind='column',basis=[0,0],grow=1,padding=0,gap=0,stack_below=0,children=[d['root']])
        with self.assertRaises(CompileError):self.compile(d)

if __name__=='__main__':unittest.main()
