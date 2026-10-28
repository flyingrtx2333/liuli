"""Compile and exercise the bounded codec and real POSIX replacement path."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]

class StateStoreTests(unittest.TestCase):
    def test_codec_and_atomic_storage(self):
        source=r'''
#include "state_store.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <limits>
#include <random>
namespace ss=explicit_ui::state_store;
template<class F> void rejected(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}assert(caught);}
int main(int argc,char** argv){
 assert(argc==2); const std::string directory=argv[1], path=directory+"/app.state";
 ss::State original{{"count",std::int64_t{7}},{"query",std::string("中文\n\\\"\t\0end",15)},{"empty",std::string{}},{"min",std::numeric_limits<std::int64_t>::min()},{"max",std::numeric_limits<std::int64_t>::max()}};
 // Embedded NUL/newline are length-prefixed, not delimiter escapes.
 original["query"]=std::string("中文\n\\\"\t")+std::string(1,'\0')+"end";

 original["tasks"]=std::vector<std::string>{"", "任务 😀", std::string("a\0b",3), "line\nnext"};
 assert(ss::encode(original).rfind("EUISTATE 2\n",0)==0);
 assert(ss::decode("EUISTATE 1\nI 5 1\ncount7\nT 5 0\nquery\nEND\n")==ss::State({{"count",std::int64_t{7}},{"query",std::string{}}}));
 assert(ss::decode("EUISTATE 1\nEND\n").empty());
 ss::State array_state{{"items",std::vector<std::string>{}}};assert(ss::decode(ss::encode(array_state))==array_state);
 array_state["items"]=std::vector<std::string>(ss::max_array_items,"");assert(ss::decode(ss::encode(array_state))==array_state);
 array_state["items"]=std::vector<std::string>(ss::max_array_items+1,"");rejected([&]{ss::encode(array_state);});
 array_state["items"]=std::vector<std::string>{std::string(4097,'x')};rejected([&]{ss::encode(array_state);});
 array_state["items"]=std::vector<std::string>{std::string("\xff")};rejected([&]{ss::encode(array_state);});
 array_state["items"]=std::vector<std::string>(17,std::string(4096,'x'));rejected([&]{ss::encode(array_state);});
 auto array_record=[](const std::string& payload){return "EUISTATE 2\nA 1 "+std::to_string(payload.size())+"\na"+payload+"\nEND\n";};
 for(const auto& payload:std::vector<std::string>{"", "1\n", "0\nx", "1001\n", "1\n4097\nx\n", "1\n3\nx\n", "1\n1\nxX", "1\n1\n\xff\n", "1\n01\nx\n", "1\n99999999999999999999999\nx\n"})rejected([&]{ss::decode(array_record(payload));});
 rejected([&]{ss::decode("EUISTATE 1\nA 1 2\na0\n\nEND\n");});
 assert(ss::decode(ss::encode(original))==original);
 assert(ss::decode(ss::encode({})).empty());
 ss::State exact;for(int i=0;i<15;++i)exact["key"+std::to_string(i)]=std::string(4096,'x');
 exact["padding"]=std::string(3800,'x');
 const auto used=ss::encode(exact).size();assert(used<ss::max_file_bytes);
 exact["padding"]=std::string(3800+ss::max_file_bytes-used,'x');
 assert(ss::encode(exact).size()==ss::max_file_bytes);assert(ss::decode(ss::encode(exact))==exact);
 std::get<std::string>(exact["padding"])+='x';rejected([&]{ss::encode(exact);});
 auto maximal=original;maximal["query"]=std::string(ss::max_text_bytes,'x');assert(ss::decode(ss::encode(maximal))==maximal);
 auto bad=maximal;bad["query"]=std::string(ss::max_text_bytes+1,'x');rejected([&]{ss::encode(bad);});
 bad=original;bad["query"]=std::string("\xff");rejected([&]{ss::encode(bad);});
 bad=original;bad["bad key"]=std::int64_t{1};rejected([&]{ss::encode(bad);});
 bad.clear();for(int i=0;i<65;++i)bad["key"+std::to_string(i)]=std::int64_t{i};rejected([&]{ss::encode(bad);});
 bad.clear();for(int i=0;i<17;++i)bad["key"+std::to_string(i)]=std::string(4096,'x');rejected([&]{ss::encode(bad);});
 const std::string invalid[]={"", "EUISTATE 4\nEND\n", "EUISTATE 1\nEND\ntrailing", "EUISTATE 1\nI 1 1\na1\nI 1 1\na2\nEND\n", "EUISTATE 1\nI 1 20\na9223372036854775808\nEND\n", "EUISTATE 1\nI 1 2\na01\nEND\n", "EUISTATE 1\nT 1 1\na\xff\nEND\n", "EUISTATE 1\nT 99999999999999999999999 1\na1\nEND\n", "EUISTATE 1\nT 1 99999\na\nEND\n", "EUISTATE 1\nT 1 1\na1XEND\n"};
 for(const auto& bytes:invalid)rejected([&]{ss::decode(bytes);});
 auto encoded=ss::encode(original);for(std::size_t n=0;n<encoded.size();++n)rejected([&]{ss::decode(encoded.substr(0,n));});
 std::mt19937 random(7);for(int i=0;i<20000;++i){auto mutated=encoded;for(unsigned n=0;n<1+random()%5;++n)mutated[random()%mutated.size()]=static_cast<char>(random()%256);if(i%3==0)mutated.resize(random()%mutated.size());try{ss::decode(mutated);}catch(const std::exception&){} }
 rejected([&]{ss::decode(std::string(ss::max_file_bytes+1,'x'));});
 #ifndef _WIN32
 ss::save(path,original);assert(ss::load(path)==original);
 auto next=original;next["count"]=std::int64_t{9};ss::save(path,next);assert(ss::load(path)==next);
 bad=next;bad["query"]=std::string("\xff");rejected([&]{ss::save(path,bad);});assert(ss::load(path)==next);
 rejected([&]{ss::save(directory+"/missing/app.state",next);});assert(ss::load(path)==next);
 // Rename failure after creating/writing a temp leaves destination directory unchanged.
 std::filesystem::create_directory(directory+"/is-directory");rejected([&]{ss::save(directory+"/is-directory",next);});assert(std::filesystem::is_directory(directory+"/is-directory"));
 for(const auto& entry:std::filesystem::directory_iterator(directory))assert(entry.path().filename().string().find(".tmp.")==std::string::npos);
 rejected([&]{ss::load(directory+"/is-directory");});
 {std::ofstream f(directory+"/malformed");f<<"EUISTATE 1\nI 1 1\na1\nT 1 3\nbx";}
 auto loaded=original;rejected([&]{loaded=ss::load(directory+"/malformed");});assert(loaded==original);
 {std::ofstream f(directory+"/large");f<<std::string(ss::max_file_bytes+1,'x');}rejected([&]{ss::load(directory+"/large");});
 #ifndef _WIN32
 std::filesystem::create_symlink(path,directory+"/link");rejected([&]{ss::load(directory+"/link");});
 assert(::mkfifo((directory+"/pipe").c_str(),0600)==0);rejected([&]{ss::load(directory+"/pipe");});
 #endif
 rejected([&]{ss::save(std::string("a\0b",3),next);});
 #else
 rejected([&]{ss::save(path,original);});
 #endif
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);(p/'test.cpp').write_text(source)
            result=subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-O2','-Wall','-Wextra','-Wpedantic','-Werror','-I',str(ROOT/'native'),str(p/'test.cpp'),'-o',str(p/'test')],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            subprocess.run([str(p/'test'),tmp],check=True,timeout=10)

    def test_generated_schema_restoration_is_atomic(self):
        import sys
        sys.path.insert(0,str(ROOT))
        from compiler import compile_text
        source=r'''
#include "app_state.h"
#include <cassert>
namespace ss=explicit_ui::state_store;
namespace app=explicit_ui::app_state;
int main(){
 explicit_ui::Model model;
 const auto original=app::capture(model);
 auto state=original;state["count"]=std::int64_t{9};state["query"]=std::string("00007");state["offset"]=std::int64_t{0};
 assert(app::restore(model,state));assert(model.view_text("counter")=="Count: 9");assert(model.view_texts("list")==std::vector<std::string>{"Item 00007"});
 assert(app::capture(model)==state);assert(!app::restore(model,state));
 auto reject=[&](ss::State bad){bool failed=false;try{app::restore(model,bad);}catch(const std::exception&){failed=true;}assert(failed);assert(app::capture(model)==state);};
 auto bad=state;bad.erase("query");reject(bad);
 bad=state;bad["extra"]=std::int64_t{1};reject(bad);
 bad=state;bad["count"]=std::string("7");reject(bad);
 bad=state;bad["query"]=std::vector<std::string>{"x"};reject(bad);
 bad=state;bad["query"]=std::string("\xff");reject(bad);
 bad=state;bad["offset"]=std::int64_t{-1};reject(bad);
 bad=state;bad["count"]=std::int64_t{123};bad["offset"]=std::int64_t{-1};reject(bad);
 assert(app::restore(model,original));assert(app::capture(model)==original);
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);(p/'model.h').write_text(compile_text((ROOT/'examples/counter_list.eui.json').read_text(),model=True));(p/'test.cpp').write_text(source)
            result=subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-O0','-Wall','-Wextra','-Wpedantic','-Werror','-I',str(p),'-I',str(ROOT/'native'),str(p/'test.cpp'),'-o',str(p/'test')],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            subprocess.run([str(p/'test')],check=True,timeout=10)

if __name__=='__main__':unittest.main()
