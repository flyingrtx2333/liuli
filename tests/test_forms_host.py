"""Multi-field host transactions through synthetic SDL dummy events (not real IME QA)."""
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


def fixture():
    literal = lambda value: {'literal': value, 'type': 'text'}
    model = {'version': 1, 'states': [
        {'id': 'first', 'type': 'text', 'initial': 'Alpha'},
        {'id': 'second', 'type': 'text', 'initial': 'Beta'},
        {'id': 'offset', 'type': 'int', 'initial': 7},
        {'id': 'other_offset', 'type': 'int', 'initial': 9},
        {'id': 'hidden', 'type': 'int', 'initial': 0},
        {'id': 'transient', 'type': 'int', 'initial': 0},
        {'id': 'rows', 'type': 'texts', 'initial': ['One', 'Two']}],
        'derived': [], 'views': [{'id': 'rows', 'source': 'rows'}],
        'transactions': [], 'persist': ['first', 'second', 'offset', 'other_offset', 'hidden'],
        'actions': [
            {'id': 'hidden_change', 'set': {'hidden': {'op': 'add', 'args': [{'ref': 'hidden'}, {'literal': 1, 'type': 'int'}]}}},
            {'id': 'noop', 'set': {'hidden': {'ref': 'hidden'}}},
            {'id': 'transient_change', 'set': {'transient': {'literal': 1, 'type': 'int'}}},
            {'id': 'reset', 'set': {'first': literal('Reset A'), 'second': literal('Reset B')}},
            {'id': 'invalid', 'set': {'first': literal('Not committed'), 'second': literal('bad\nvalue')}}]}
    widgets = []
    for i, (name, state) in enumerate((('first_field', 'first'), ('second_field', 'second'), ('mirror', 'first'))):
        widgets.append({'id': name, 'kind': 'input', 'state': state,
                        'text': name, 'rect': [30, 30 + i * 70, 400, 50]})
    widgets += [
        {'id': 'hidden_change', 'kind': 'button', 'action': 'hidden_change', 'text': 'Hidden change', 'rect': [30, 410, 180, 50]},
        {'id': 'noop', 'kind': 'button', 'action': 'noop', 'text': 'No-op', 'rect': [230, 410, 180, 50]},
        {'id': 'transient_change', 'kind': 'button', 'action': 'transient_change', 'text': 'Transient', 'rect': [430, 410, 180, 50]},
        {'id': 'resetting_field', 'kind': 'input', 'state': 'second', 'text': 'Explicit reset',
         'reset_states': ['offset'], 'rect': [30, 330, 400, 50]},
        {'id': 'reset', 'kind': 'button', 'action': 'reset', 'text': 'Reset', 'rect': [30, 250, 180, 50]},
        {'id': 'invalid', 'kind': 'button', 'action': 'invalid', 'text': 'Invalid', 'rect': [230, 250, 180, 50]},
        {'id': 'list', 'kind': 'list', 'binding': 'rows', 'state': 'offset', 'delta': 1, 'rect': [470, 30, 400, 300]}]
    return model, {'version': 1, 'width': 1120, 'height': 840, 'widgets': widgets}


@unittest.skipIf(os.environ.get('EUI_CONSOLE_ONLY') == '1', 'graphics backend disabled')
class FormsHostTests(unittest.TestCase):
    def test_multifield_transactions_and_persistence(self):
        self.run_host()

    def test_accessibility_field_routing(self):
        sdk = Path(os.environ.get('EUI_A11Y_SDK', str(ROOT / 'build/a11y-sdk/root')))
        if not (sdk / 'usr/include/atk-1.0/atk/atk.h').exists():
            self.skipTest('optional ATK SDK unavailable; set EUI_A11Y_SDK')
        self.run_host(sdk)

    def run_host(self, sdk=None):
        model, layout = fixture()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / 'model.h').write_text(compile_text(json.dumps(model), model=True))
            (path / 'scene.h').write_text(compile_layout(json.dumps(layout), json.dumps(model)))
            (path / 'test.cpp').write_text(r'''
#define main eui_original_main
#include "window_host.cpp"
#undef main
#include <cassert>
#include <tuple>
auto snapshot(const TextEdit& e){return std::make_tuple(e.text(),e.caret(),e.anchor(),e.preedit(),e.undo_size(),e.redo_size());}
void key_event(App& app,Sint32 key,Uint16 mod=0){SDL_Event e{};e.type=SDL_KEYDOWN;e.key.keysym.sym=key;e.key.keysym.mod=mod;app.event(e);}
void text_event(App& app,const char* text){SDL_Event e{};e.type=SDL_TEXTINPUT;std::strcpy(e.text.text,text);app.event(e);}
int main(int argc,char** argv){
    assert(argc==2);Sdl sdl;
    state_store::State saved;
    {
        App app(argv[1]);app.drain();
        const auto& first=*app.find("first_field");const auto& second=*app.find("second_field");const auto& mirror=*app.find("mirror");
        assert(app.editors.size()==4);
        app.focus(&first);key_event(app,SDLK_HOME);key_event(app,SDLK_RIGHT,1);
        const auto first_selection=snapshot(app.edit(first));
        app.focus(&second);text_event(app,"!");
        assert(app.model.state_text("second")=="Beta!");assert(snapshot(app.edit(first))==first_selection);
        assert(app.model.state_int("offset")==7); // Input is not an implicit global list reset.
        const auto second_edited=snapshot(app.edit(second));
        app.focus(&first);text_event(app,"X");assert(app.model.state_text("first")=="Xlpha");
        assert(app.edit(mirror).text()=="Xlpha"&&app.edit(mirror).undo_size()==0);
        assert(snapshot(app.edit(second))==second_edited);
        key_event(app,'z',KMOD_CTRL);assert(app.model.state_text("first")=="Alpha"&&app.edit(mirror).text()=="Alpha");
        assert(app.model.state_text("second")=="Beta!");
        app.focus(&second);key_event(app,'z',KMOD_CTRL);assert(app.model.state_text("second")=="Beta");
        // Mirrored edits invalidate only the stale peer history, never another state.
        app.focus(&mirror);text_event(app,"M");assert(app.edit(first).text()=="AlphaM");assert(app.edit(first).undo_size()==0);
        app.focus(&first);key_event(app,SDLK_TAB);assert(app.focused=="second_field");key_event(app,SDLK_TAB,1);assert(app.focused=="first_field");
        SDL_Event click{};click.type=SDL_MOUSEBUTTONDOWN;click.button.button=SDL_BUTTON_LEFT;click.button.x=second.x+20;click.button.y=second.y+20;app.event(click);assert(app.focused=="second_field");
        // A bad later field rejects the entire prepared action and editor publication.
        const auto before=app_state::capture(app.model);const auto a=snapshot(app.edit(first)),b=snapshot(app.edit(second)),m=snapshot(app.edit(mirror));const auto generation=app.generation;
        bool rejected=false;try{app.activate(*app.find("invalid"));}catch(const std::invalid_argument&){rejected=true;}
        assert(rejected&&app_state::capture(app.model)==before&&app.generation==generation);
        assert(snapshot(app.edit(first))==a&&snapshot(app.edit(second))==b&&snapshot(app.edit(mirror))==m);
        auto bad=before;bad["first"]=std::string("Changed");bad["second"]=std::string(257,'x');
        rejected=false;try{app.restore_values(bad);}catch(const std::invalid_argument&){rejected=true;}
        assert(rejected&&app_state::capture(app.model)==before&&app.generation==generation);
        assert(snapshot(app.edit(first))==a&&snapshot(app.edit(second))==b&&snapshot(app.edit(mirror))==m);
        bad=before;bad.erase("offset");bad["first"]=std::string("Changed");
        rejected=false;try{app.restore_values(bad);}catch(const std::invalid_argument&){rejected=true;}
        assert(rejected&&app_state::capture(app.model)==before&&snapshot(app.edit(first))==a);
        app.set_query(*app.find("resetting_field"),"Reset offset only");assert(app.model.state_int("offset")==0&&app.model.state_int("other_offset")==9);
        app.activate(*app.find("reset"));assert(app.model.state_text("first")=="Reset A"&&app.model.state_text("second")=="Reset B");assert(app.edit(mirror).text()=="Reset A");
        // Uncommitted synthetic composition is not silently called Saved.
        app.focus(&second);SDL_Event composition{};composition.type=SDL_TEXTEDITING;std::strcpy(composition.edit.text,"ime");composition.edit.start=1;app.event(composition);
        const auto errors=app.errors;app.save_state();assert(app.pending_request==0&&app.errors==errors+1&&app.status.find("composition")!=std::string::npos);
        assert(app.model.state_text("second")=="Reset B"&&app.edit(second).preedit()=="ime");
        text_event(app,"!");assert(app.edit(second).preedit().empty());
        saved=app_state::capture(app.model);app.save_state();app.wait_state_for_test();assert(app.status=="Saved");
        // Canceling uncommitted composition restores a truthful clean status, including after a rejected save.
        const auto clean_revision=app.model.persistence_revision();
        app.focus(&second);app.event(composition);app.save_state();assert(app.status=="Save rejected: finish text composition");
        SDL_Event empty_composition{};empty_composition.type=SDL_TEXTEDITING;app.event(empty_composition);
        assert(app.status=="Saved"&&app.model.persistence_revision()==clean_revision&&app.edit(second).preedit().empty());
        app.event(composition);app.save_state();assert(app.status=="Save rejected: finish text composition");
        app.focus(nullptr);assert(app.status=="Saved"&&app.model.persistence_revision()==clean_revision&&app.edit(second).preedit().empty());
        app.focus(&second);app.event(composition);
        for(auto& w:app.layout.widgets)if(w.id=="second_field")w.width=0;
        app.paint();assert(app.status=="Saved"&&app.model.persistence_revision()==clean_revision&&app.edit(second).preedit().empty()&&app.focused.empty());
        for(auto& w:app.layout.widgets)if(w.id=="second_field")w.width=400;
        app.focus(&second);
        const auto saved_revision=app.model.persistence_revision();
        app.activate(*app.find("noop"));assert(app.status=="Saved"&&app.model.persistence_revision()==saved_revision);
        app.activate(*app.find("transient_change"));assert(app.status=="Saved"&&app.model.persistence_revision()==saved_revision);
        app.activate(*app.find("hidden_change"));assert(app.model.persistence_revision()==saved_revision+1&&app.status=="Unsaved changes");
        saved=app_state::capture(app.model);app.save_state();app.wait_state_for_test();assert(app.status=="Saved");
        const auto failure_revision=app.model.persistence_revision();rejected=false;try{app.activate(*app.find("invalid"));}catch(const std::invalid_argument&){rejected=true;}
        assert(rejected&&app.model.persistence_revision()==failure_revision&&app.status=="Saved");
        // Save captures committed state; later composition is unsaved even without model mutation.
        app.save_state();composition.edit.start=1;app.event(composition);app.wait_state_for_test();
        assert(app.status=="Saved snapshot; text composition pending"&&app.model.persistence_revision()==failure_revision);
        auto clear=app.edit(second);clear.cancel_preedit();app.apply_editor(second,std::move(clear));
        app.save_state();app.wait_state_for_test();assert(app.status=="Saved");
        app.focus(&second);text_event(app,"x");key_event(app,'z',KMOD_CTRL);
        assert(app_state::capture(app.model)==saved&&app.status=="Unsaved changes"&&app.model.persistence_revision()==failure_revision+2);
        app.save_state();app.wait_state_for_test();assert(app.status=="Saved");

        // Async load cannot overwrite a subsequent edit in any field.
        app.set_query(first,"dirty");assert(app.status=="Unsaved changes");app.load_state();app.set_query(first,"newer");app.wait_state_for_test();assert(app.model.state_text("first")=="newer"&&app.status=="Load skipped: newer edits");
        app.load_state();app.wait_state_for_test();assert(app_state::capture(app.model)==saved);
        assert(app.edit(first).text()==app.model.state_text("first")&&app.edit(second).text()==app.model.state_text("second")&&app.edit(mirror).text()==app.model.state_text("first"));
#ifdef EUI_LINUX_ACCESSIBILITY
        // Exercise UI-side AT request routing without creating a bridge, bus, or worker.
        app.accessible_queue=std::make_shared<App::AccessibleQueue>();app.accessible_queue->type=SDL_RegisterEvents(1);
        const auto send=[&](AccessibilityRequest request){app.accessible_queue->requests.push_back(std::move(request));app.accessibility_event();};
        send({AccessibilityRequest::Kind::replace,"first_field","中🙂A",0,0});
        assert(app.edit(first).text()=="中🙂A"&&app.edit(mirror).text()=="中🙂A");
        const auto first_at=snapshot(app.edit(first));
        send({AccessibilityRequest::Kind::replace,"second_field","Second AT",0,0});
        send({AccessibilityRequest::Kind::select,"second_field","",1,3});
        assert(app.edit(second).anchor()==1&&app.edit(second).caret()==3&&snapshot(app.edit(first))==first_at);
        send({AccessibilityRequest::Kind::select,"first_field","",1,2});
        assert(app.edit(first).anchor()==3&&app.edit(first).caret()==7); // Unicode scalar indexes -> UTF-8 bytes.
        send({AccessibilityRequest::Kind::focus,"mirror","",0,0});assert(app.focused=="mirror");
        const auto before_collapsed=app_state::capture(app.model);const auto second_at=snapshot(app.edit(second));const auto first_selected=snapshot(app.edit(first));
        for(auto& w:app.layout.widgets)if(w.id=="second_field")w.width=0;
        send({AccessibilityRequest::Kind::replace,"second_field","blocked",0,0});
        send({AccessibilityRequest::Kind::select,"second_field","",0,1});
        send({AccessibilityRequest::Kind::focus,"second_field","",0,0});
        assert(app_state::capture(app.model)==before_collapsed&&snapshot(app.edit(second))==second_at&&snapshot(app.edit(first))==first_selected&&app.focused=="mirror");
        for(auto& w:app.layout.widgets)if(w.id=="second_field")w.width=400;
        app.close_after_save=true;send({AccessibilityRequest::Kind::replace,"first_field","frozen",0,0});
        assert(app_state::capture(app.model)==before_collapsed);app.close_after_save=false;
        app.restore_values(saved);
        std::cout<<"forms_accessibility_route_passed: no live ATK initialization\n";
#endif
        app.save_state();app.request_close();assert(app.close_after_save&&app.running);app.wait_state_for_test();assert(!app.running);
    }
    {App restarted(argv[1]);assert(restarted.status=="Loaded"&&app_state::capture(restarted.model)==saved);
     for(const auto& w:restarted.layout.widgets)if(w.kind==WidgetKind::search)assert(restarted.edit(w).text()==restarted.model.state_text(w.state));}
    std::cout<<"forms_host_passed: synthetic SDL dummy; no real IME claim\n";
}
''')
            command = [os.environ.get('CXX', 'c++'), '-std=c++17', '-O0', '-pthread',
                       '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I', str(path),
                       '-I', str(ROOT / 'native'), str(path / 'test.cpp'),
                       str(ROOT / 'native/glass_renderer.cpp'), str(ROOT / 'native/shaped_text.cpp')]
            if sdk is not None:
                command += ['-DEUI_LINUX_ACCESSIBILITY', str(ROOT / 'native/linux_accessibility.cpp'), '-ldl',
                            '-Wl,-l:libatk-1.0.so.0', '-Wl,-l:libatk-bridge-2.0.so.0']
                for include in ('usr/include/atk-1.0', 'usr/include/at-spi2-atk/2.0',
                                'usr/include/glib-2.0', 'usr/lib/x86_64-linux-gnu/glib-2.0/include'):
                    command += ['-isystem', str(sdk / include)]
            command += ['-Wl,-l:' + name for name in ('libSDL2-2.0.so.0', 'libcairo.so.2',
                        'libpangocairo-1.0.so.0', 'libpango-1.0.so.0', 'libgobject-2.0.so.0', 'libglib-2.0.so.0')]
            command += ['-o', str(path / 'test')]
            result = subprocess.run(command, text=True, capture_output=True, timeout=120)
            self.assertEqual(result.returncode, 0, result.stderr)
            environment = dict(os.environ, SDL_VIDEODRIVER='dummy')
            environment.pop('EUI_ACCESSIBILITY', None)
            result = subprocess.run([str(path / 'test'), str(path / 'state.eui')], text=True,
                                    capture_output=True, timeout=30,
                                    env=environment)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('forms_host_passed', result.stdout)
            if sdk is not None:
                self.assertIn('forms_accessibility_route_passed', result.stdout)


if __name__ == '__main__':
    unittest.main()
