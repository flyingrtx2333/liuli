"""Strict typed conditionals, short circuiting and atomic boolean persistence."""
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
def lit(v, t='int'): return {'literal':v,'type':t}
def ref(v): return {'ref':v}
def op(n,*a): return {'op':n,'args':list(a)}
def document():
    overflow=op('add',lit(2**63-1),lit(1))
    d={'version':1,'states':[{'id':'enabled','type':'bool','initial':False},{'id':'count','type':'int','initial':0}],
       'derived':[{'id':'safe','type':'int','expr':op('if',ref('enabled'),overflow,ref('count'))},
                  {'id':'both','type':'bool','expr':op('and',ref('enabled'),op('eq',overflow,lit(0)))},
                  {'id':'either','type':'bool','expr':op('or',op('not',ref('enabled')),op('eq',overflow,lit(0)))},
                  {'id':'label','type':'text','expr':op('if',ref('enabled'),lit('on','text'),lit('off','text'))},
                  {'id':'items','type':'texts','expr':op('if',ref('enabled'),op('remove_at',lit([],'texts'),lit(0)),lit(['safe'],'texts'))}],
       'views':[{'id':x,'source':x} for x in ['enabled','safe','both','either','label','items']],
       'transactions':[],'persist':['enabled','count'],
       'actions':[{'id':'fail','set':{'count':lit(9),'enabled':lit(True,'bool')}},
                  {'id':'increment','set':{'count':op('add',ref('count'),lit(1))}}]}
    return d
class ConditionTests(unittest.TestCase):
    def build(self,p,code):
        (p/'test.cpp').write_text(code)
        subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-Wall','-Wextra','-Wpedantic','-Werror','-I',str(p),'-I',str(ROOT/'native'),str(p/'test.cpp'),'-o',str(p/'test')],check=True,capture_output=True)
        return subprocess.run([str(p/'test')],cwd=p,check=True,capture_output=True,text=True).stdout
    def test_live_rollback_and_codec(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);(p/'model.h').write_text(compile_text(json.dumps(document()),model=True))
            out=self.build(p,r'''#include "app_state.h"
#include <cassert>
using namespace explicit_ui;
int main(){Model m;assert(!m.state_bool("enabled")&&!m.view_bool("both")&&m.view_bool("either"));
assert(m.view_text("label")=="off"&&m.view_texts("items")==std::vector<std::string>{"safe"});
auto stats=m.stats();
for(int i=0;i<5;++i){bool bad=false;try{
if(i==0)m.invoke_action("fail");
if(i==1)m.apply({{"count",int64_t{5}},{"enabled",int64_t{1}}});
if(i==2)m.apply({{"count",true}});
if(i==3)app_state::restore(m,{{"count",int64_t{9}},{"enabled",true}});
if(i==4)app_state::restore(m,{{"count",int64_t{9}},{"enabled",int64_t{0}}});
}catch(const std::exception&){bad=true;}assert(bad&&!m.state_bool("enabled")&&m.state_int("count")==0&&m.stats().patches==stats.patches&&m.stats().evaluations==stats.evaluations);}
assert(!m.apply({{"enabled",true},{"enabled",false}}));
assert(m.invoke_action("increment")&&m.view_int("safe")==1);
auto state=app_state::capture(m);auto encoded=state_store::encode(state);assert(encoded.find("EUISTATE 3\n")==0);
assert(state_store::decode(encoded)==state);Model n;app_state::restore(n,state_store::decode(encoded));assert(n.state_int("count")==1&&!n.state_bool("enabled"));
assert(std::get<int64_t>(state_store::decode("EUISTATE 1\nI 1 1\nx1\nEND\n").at("x"))==1);
assert(std::get<int64_t>(state_store::decode("EUISTATE 2\nI 1 1\nx1\nEND\n").at("x"))==1);
for(auto bytes:{"EUISTATE 3\nB 1 1\nx1\nEND\n","EUISTATE 2\nB 1 4\nxtrue\nEND\n","EUISTATE 1\nB 1 5\nxfalse\nEND\n","EUISTATE 3\nB 1 4\nxTrue\nEND\n"}){bool bad=false;try{state_store::decode(bytes);}catch(const std::invalid_argument&){bad=true;}assert(bad);}
m.snapshot("checked",0);
}''')
            views=json.loads(out)['views'];self.assertIs(views['enabled'],False);self.assertIs(views['either'],True)
    def test_scripted_ordering_and_equality(self):
        d=document();d['derived'].append({'id':'true_branch','type':'bool','expr':op('if',lit(True,'bool'),lit(True,'bool'),op('eq',op('add',lit(2**63-1),lit(1)),lit(0)))});d['views'].append({'id':'true_branch','source':'true_branch'});d['transactions']=[{'label':'one','set':{'count':lit(1)}}]
        for name,expr in [('lt',op('lt',lit(-1),lit(0))),('le',op('le',lit(0),lit(0))),('gt',op('gt',lit(1),lit(0))),('ge',op('ge',lit(0),lit(0))),('text',op('eq',lit('x','text'),lit('x','text'))),('texts',op('eq',lit(['a'],'texts'),lit(['a'],'texts'))),('bool',op('eq',lit(True,'bool'),lit(True,'bool')))]:
            d['derived'].append({'id':'check_'+name,'type':'bool','expr':expr});d['views'].append({'id':name,'source':'check_'+name})
        with tempfile.TemporaryDirectory() as tmp:
            out=self.build(Path(tmp),compile_text(json.dumps(d)))
            views=json.loads(out.splitlines()[1])['views']
            for name in ['lt','le','gt','ge','text','texts','bool','true_branch']:self.assertIs(views[name],True)
            self.assertEqual(views['safe'],1)
    def test_successful_boolean_actions_and_persistence(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);(p/'model.h').write_text(compile_text((ROOT/'examples/conditions.eui.json').read_text(),model=True))
            self.build(p,r'''#include "app_state.h"
#include <cassert>
using namespace explicit_ui;
int main(){Model m;m.apply({{"count",int64_t{1}}});assert(m.invoke_action("toggle"));
assert(m.state_bool("enabled")&&m.view_bool("ready")&&m.view_text("status")=="Ready");
auto bytes=state_store::encode(app_state::capture(m));Model n;app_state::restore(n,state_store::decode(bytes));assert(n.state_bool("enabled")&&n.view_bool("ready"));
state_store::save("state",app_state::capture(m));assert(std::get<bool>(state_store::load("state").at("enabled")));
auto oversized=app_state::capture(m);oversized["bad"]=std::string(5000,'x');bool rejected=false;try{state_store::save("state",oversized);}catch(const std::invalid_argument&){rejected=true;}assert(rejected&&state_store::load("state")==app_state::capture(m));
assert(m.invoke_action("toggle")&&!m.state_bool("enabled")&&!m.view_bool("ready"));
}''')
    def test_compile_rejections(self):
        bad=[lit(1,'bool'),lit(True,'int'),op('not',lit(0)),op('and',lit(True,'bool'),lit(1)),op('or',lit(True,'bool')),op('eq',lit(True,'bool'),lit(1)),op('lt',lit(False,'bool'),lit(True,'bool')),op('if',lit(1),lit(1),lit(2)),op('if',lit(True,'bool'),lit(1),lit('x','text')),op('if',lit(False,'bool'),op('unknown'),lit(1)),op('to_text',lit(True,'bool'))]
        for expr in bad:
            d=document();d['derived']=[{'id':'bad','type':'bool','expr':expr}];d['views']=[]
            for model in (False,True):
                with self.subTest(expr=expr,model=model),self.assertRaises(CompileError):compile_text(json.dumps(d),model=model)
if __name__=='__main__':unittest.main()
