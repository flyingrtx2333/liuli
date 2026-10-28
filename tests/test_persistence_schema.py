"""Opt-in generated persistence and action preparation, no UI-specific state names."""
import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from compiler import CompileError,compile_text

class PersistenceSchemaTests(unittest.TestCase):
    def test_rejects_invalid_persistence_declarations(self):
        doc=json.loads((ROOT/'examples/task_list.eui.json').read_text())
        for value in ['tasks',[1],['tasks','tasks'],['missing'],['visible_tasks'],None,{}]:
            bad=copy.deepcopy(doc);bad['persist']=value
            with self.subTest(persist=value),self.assertRaises(CompileError):compile_text(json.dumps(bad),model=True)

    def run_cpp(self,doc,source):
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);(p/'model.h').write_text(compile_text(json.dumps(doc),model=True));(p/'main.cpp').write_text(source)
            result=subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-O0','-Wall','-Wextra','-Wpedantic','-Werror','-I',str(p),'-I',str(ROOT/'native'),str(p/'main.cpp'),'-o',str(p/'test')],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            subprocess.run([str(p/'test')],check=True,timeout=10)

    def test_task_arrays_schema_and_atomic_restore(self):
        doc=json.loads((ROOT/'examples/task_list.eui.json').read_text())
        self.run_cpp(doc,r'''#include "app_state.h"
#include <cassert>
using namespace explicit_ui;
int main(){
 Model model;
 const auto original=app_state::capture(model);
 assert(original.size()==4);
 model.apply({{"draft",std::string(u8"中文😀")}});
 const auto before=model.state_texts("tasks");const auto stats=model.stats();
 const auto staged=model.prepare_action("add_task");
 assert(model.state_texts("tasks")==before&&model.state_text("draft")==u8"中文😀"&&model.stats().patches==stats.patches);
 assert(model.apply(staged));assert(model.state_texts("tasks").back()==u8"中文😀");assert(model.state_text("draft").empty());
 auto saved=app_state::capture(model);
 assert(state_store::decode(state_store::encode(saved))==saved);
 model.invoke_action("remove_task");assert(app_state::restore(model,saved));assert(app_state::capture(model)==saved);
 assert(!app_state::restore(model,saved));
 auto reject=[&](std::vector<Update> updates){auto current=app_state::capture(model);auto prior=model.stats();bool failed=false;try{model.restore_persistent_state(updates);}catch(const std::exception&){failed=true;}assert(failed);assert(app_state::capture(model)==current&&model.stats().patches==prior.patches);};
 auto valid=model.persistent_state();auto bad=valid;bad.pop_back();reject(bad);
 bad=valid;bad.push_back(valid.front());reject(bad);
 bad=valid;bad[1]=bad[0];reject(bad);
 bad=valid;bad[0].id="missing";reject(bad);
 bad=valid;bad[0].value=std::int64_t{3};reject(bad);
 bad=valid;for(auto& u:bad)if(u.id=="draft")u.value=std::string("\xff");reject(bad);
 bad=valid;for(auto& u:bad){if(u.id=="tasks")u.value=std::vector<std::string>{"staged only"};if(u.id=="offset")u.value=std::int64_t{-1};}reject(bad);
 // Caller payloads and captured snapshots cannot mutate the committed model.
 auto exported=app_state::capture(model);std::get<std::vector<std::string>>(exported.at("tasks"))[0]="caller edit";
 assert(app_state::capture(model)==saved);
 bool unknown=false;try{model.prepare_action("missing");}catch(const std::invalid_argument&){unknown=true;}assert(unknown&&app_state::capture(model)==saved);
 assert(app_state::restore(model,original));assert(app_state::capture(model)==original);
}
''')

    def test_unconfigured_persistence_is_empty_and_core_still_compiles(self):
        doc={'version':1,'states':[],'derived':[],'views':[],'transactions':[]}
        self.run_cpp(doc,r'''#include "app_state.h"
#include <cassert>
int main(){explicit_ui::Model model;assert(model.persistent_state().empty());assert(!model.restore_persistent_state({}));assert(explicit_ui::app_state::capture(model).empty());bool failed=false;try{model.restore_persistent_state({{"extra",std::int64_t{0}}});}catch(const std::invalid_argument&){failed=true;}assert(failed);}
''')

if __name__=='__main__':unittest.main()
