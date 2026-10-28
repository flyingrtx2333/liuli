"""Real host/model boundary rejects invalid action and reload values atomically."""
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
from layout_compiler import compile_layout

@unittest.skipIf(os.environ.get('EUI_CONSOLE_ONLY') == '1', 'native host disabled')
class HostTransactionTests(unittest.TestCase):
    def test_actions_and_restore_respect_editor_transaction(self):
        doc = json.loads((ROOT/'examples/counter_list.eui.json').read_text())
        doc['actions'] = [
            {'id':'too_long','set':{'count':{'literal':99,'type':'int'},'query':{'literal':'x'*257,'type':'text'}}},
            {'id':'valid','set':{'count':{'literal':7,'type':'int'},'query':{'literal':'00007','type':'text'}}},
            {'id':'bad_offset','set':{'query':{'literal':'changed','type':'text'},'offset':{'literal':100001,'type':'int'}}}]
        model = json.dumps(doc)
        with tempfile.TemporaryDirectory() as directory:
            p = Path(directory)
            (p/'model.h').write_text(compile_text(model, model=True))
            (p/'scene.h').write_text(compile_layout((ROOT/'examples/responsive.layout.json').read_text(), model))
            (p/'test.cpp').write_text(r'''
#define main explicit_ui_window_program_main
#include "window_host.cpp"
#undef main
#include <cassert>
int main(int argc,char**argv){
 assert(argc==2);Sdl sdl;App app(argv[1]);
 auto before=app.model.stats();const auto paints=app.paints;
 Widget action{};action.width=1;action.height=1;action.kind=WidgetKind::button;action.action="too_long";
 bool rejected=false;try{app.activate(action);}catch(const std::invalid_argument&){rejected=true;}
 assert(rejected&&app.model.state_int("count")==0&&app.query().empty()&&app.edit(*app.search()).text().empty());
 assert(app.model.stats().patches==before.patches&&app.paints==paints);
 action.action="valid";app.activate(action);
 assert(app.model.state_int("count")==7&&app.query()=="00007"&&app.edit(*app.search()).text()=="00007");
 action.action="bad_offset";rejected=false;try{app.activate(action);}catch(const std::invalid_argument&){rejected=true;}
 assert(rejected&&app.query()=="00007"&&app.edit(*app.search()).text()=="00007");
 auto saved=app_state::capture(app.model);saved["query"]=std::string("\xe2\x80\xa8");saved["count"]=std::int64_t{99};
 state_store::save(app.state_path,saved);before=app.model.stats();const auto errors=app.errors;app.load_state();app.wait_state_for_test();
 assert(app.errors==errors+1&&app.model.state_int("count")==7&&app.query()=="00007"&&app.edit(*app.search()).text()=="00007"&&app.model.stats().patches==before.patches);
 saved=app_state::capture(app.model);saved["query"]=std::string("99");saved["count"]=std::int64_t{8};state_store::save(app.state_path,saved);app.load_state();app.wait_state_for_test();
 assert(app.model.state_int("count")==8&&app.query()=="99"&&app.edit(*app.search()).text()=="99");
 app.save_state();app.wait_state_for_test();assert(app.status=="Saved");
 app.load_state();app.set_query(*app.search(),"newer");app.wait_state_for_test();assert(app.query()=="newer"&&app.status=="Load skipped: newer edits");
 app.load_state();app.cancel_state();app.wait_state_for_test();assert(app.query()=="newer"&&app.status=="Cancelled");
 for(const auto* q:{"one","two","three"}){app.set_query(*app.search(),q);app.save_state();}
 app.wait_state_for_test();assert(app.status=="Saved");assert(std::get<std::string>(state_store::load(app.state_path).at("query"))=="three");
 app.save_state();app.set_query(*app.search(),"later");app.wait_state_for_test();assert(app.status=="Saved snapshot; newer edits pending");
 // Native event adapter: Pango clusters, clipboard, undo and mouse selection.
 app.focus(app.search());app.set_query(*app.search(),u8"中é👩‍💻");app.key(SDLK_BACKSPACE);assert(app.query()==u8"中é");app.key(SDLK_BACKSPACE);assert(app.query()==u8"中");app.key('z',KMOD_CTRL);assert(app.query()==u8"中é");app.key('z',KMOD_CTRL|1);assert(app.query()==u8"中");
 assert(SDL_SetClipboardText(u8"中文👩‍💻🇨🇳")==0);app.key('a',KMOD_CTRL);app.key('v',KMOD_CTRL);assert(app.query()==u8"中文👩‍💻🇨🇳");
 app.key(SDLK_LEFT,1);assert(app.edit(*app.search()).selected_text()==u8"🇨🇳");app.key('x',KMOD_CTRL);assert(app.query()==u8"中文👩‍💻");app.key('z',KMOD_CTRL);assert(app.query()==u8"中文👩‍💻🇨🇳");
 app.set_query(*app.search(),u8"中文👩‍💻");auto widget=*app.search();SDL_Event mouse{};mouse.type=SDL_MOUSEBUTTONDOWN;mouse.button.button=SDL_BUTTON_LEFT;mouse.button.x=widget.x+18;mouse.button.y=widget.y+widget.height/2;app.dispatch(mouse);assert(app.edit(*app.search()).caret()==0);
 mouse={};mouse.type=SDL_MOUSEMOTION;mouse.motion.state=1;mouse.motion.x=widget.x+widget.width-18;mouse.motion.y=widget.y+widget.height/2;app.dispatch(mouse);assert(app.edit(*app.search()).selected_text()==u8"中文👩‍💻");
 mouse={};mouse.type=SDL_MOUSEBUTTONUP;mouse.button.button=SDL_BUTTON_LEFT;app.dispatch(mouse);assert(!app.selecting_text);
 // An edit may join a following combining mark; publishing snaps forward.
 app.set_query(*app.search(),u8"́");app.key(SDLK_HOME);app.text("e");assert(app.query()==u8"é"&&app.edit(*app.search()).caret()==3);app.key(SDLK_BACKSPACE);assert(app.query().empty());
 // Same hit origin as rendering for horizontally scrolled long text.
 app.set_query(*app.search(),std::string(240,'W'));widget=*app.search();const auto at=editor_hit_test(widget,app.edit(*app.search()),widget.x+widget.width-18);assert(at>=239);
 // A rejected startup file must not be overwritten by a later Save.
 const std::string bad_path=app.state_path+".bad";{std::ofstream f(bad_path);f<<"broken original";}
 {App broken(bad_path);assert(broken.save_blocked);broken.save_state();broken.wait_state_for_test();std::ifstream f(bad_path);std::string bytes((std::istreambuf_iterator<char>(f)),{});assert(bytes=="broken original");}
 // Normal Close after an explicit Save defers exit until that save commits.
 app.set_query(*app.search(),"close-safe");app.save_state();SDL_Event quit{};quit.type=SDL_QUIT;app.dispatch(quit);app.wait_state_for_test();
 assert(!app.running&&std::get<std::string>(state_store::load(app.state_path).at("query"))=="close-safe");
 // Outstanding work is joined while SDL is still alive; queued app events are flushed.
 {App closing(app.state_path);closing.save_state();}

}
''')
            flags = ['-Wl,-l:libSDL2-2.0.so.0','-Wl,-l:libcairo.so.2','-Wl,-l:libpangocairo-1.0.so.0','-Wl,-l:libpango-1.0.so.0','-Wl,-l:libgobject-2.0.so.0','-Wl,-l:libglib-2.0.so.0']
            subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-O0','-pthread','-Wall','-Wextra','-Wpedantic','-Werror','-I',str(p),'-I',str(ROOT/'native'),str(p/'test.cpp'),str(ROOT/'native/glass_renderer.cpp'),str(ROOT/'native/shaped_text.cpp'),*flags,'-o',str(p/'test')],check=True,capture_output=True,timeout=60)
            subprocess.run([str(p/'test'),str(p/'state')],check=True,timeout=30,env=dict(os.environ,SDL_VIDEODRIVER='dummy',XDG_CACHE_HOME='/tmp/explicit-ui-font-cache'))

if __name__ == '__main__':unittest.main()
