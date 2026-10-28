"""Rejected native edits preserve the full editor history, not only visible text.

Synthetic SDL dummy events; not real platform IME acceptance.
"""
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


@unittest.skipIf(os.environ.get('EUI_CONSOLE_ONLY') == '1', 'graphics backend disabled')
class InputRejectionTests(unittest.TestCase):
    def test_existing_history_survives_ui_model_action_and_restore_rejection(self):
        ref = lambda name: {'ref': name}
        lit = lambda value, kind: {'literal': value, 'type': kind}
        op = lambda name, *args: {'op': name, 'args': list(args)}
        invalid = op('or', op('eq', ref('first'), lit('BAD', 'text')),
                     op('and', op('eq', ref('gate'), lit(1, 'int')),
                        op('eq', ref('first'), lit('A', 'text'))))
        model = {'version': 1, 'states': [
            {'id': 'first', 'type': 'text', 'initial': 'A'},
            {'id': 'second', 'type': 'text', 'initial': 'Peer'},
            {'id': 'gate', 'type': 'int', 'initial': 0}],
            'derived': [{'id': 'checked', 'type': 'int', 'expr': op('if', invalid,
                op('add', lit(9223372036854775807, 'int'), lit(1, 'int')), lit(0, 'int'))}],
            'views': [{'id': 'checked', 'source': 'checked'}], 'transactions': [],
            'persist': ['first', 'second', 'gate'],
            'actions': [{'id': 'bad', 'set': {'first': lit('BAD', 'text'), 'second': lit('Changed', 'text')}}]}
        layout = {'version': 1, 'width': 1120, 'height': 840, 'widgets': [
            {'id': 'first', 'kind': 'input', 'state': 'first', 'text': 'First', 'rect': [20, 20, 400, 50]},
            {'id': 'mirror', 'kind': 'input', 'state': 'first', 'text': 'Mirror', 'rect': [20, 90, 400, 50]},
            {'id': 'second', 'kind': 'input', 'state': 'second', 'text': 'Second', 'rect': [20, 160, 400, 50]},
            {'id': 'bad', 'kind': 'button', 'action': 'bad', 'text': 'Bad', 'rect': [20, 230, 120, 50]}]}
        source = r'''
#define main eui_original_main
#include "window_host.cpp"
#undef main
#include <cassert>
#include <tuple>
auto current(const TextEdit& e){return std::make_tuple(e.text(),e.caret(),e.anchor(),e.preedit(),e.preedit_selection_start(),e.preedit_selection_end(),e.undo_size(),e.redo_size());}
auto history(TextEdit e){
    std::vector<decltype(current(e))> result{current(e)};
    auto backward=e;while(backward.undo_size()){backward.undo();result.push_back(current(backward));}
    auto forward=e;while(forward.redo_size()){forward.redo();result.push_back(current(forward));}
    return result;
}
void key(App& app,Sint32 value,Uint16 mods=0){SDL_Event e{};e.type=SDL_KEYDOWN;e.key.keysym.sym=value;e.key.keysym.mod=mods;app.event(e);}
void text(App& app,const char* value){SDL_Event e{};e.type=SDL_TEXTINPUT;std::strcpy(e.text.text,value);app.event(e);}
template<class F> void rejects_unchanged(App& app,F operation,const char* expected){
    const auto states=app_state::capture(app.model);const auto stats=app.model.stats();
    const auto revision=app.model.persistence_revision(),generation=app.generation;
    const auto status=app.status,focused=app.focused;
    std::vector<decltype(history(TextEdit{}))> histories;
    for(const auto& entry:app.editors)histories.push_back(history(entry.second));
    bool rejected=false;try{operation();}catch(const std::exception& error){rejected=std::string(error.what()).find(expected)!=std::string::npos;}
    assert(rejected&&app_state::capture(app.model)==states);
    assert(app.model.persistence_revision()==revision&&app.generation==generation);
    assert(app.model.stats().patches==stats.patches&&app.model.stats().rows_materialized==stats.rows_materialized&&app.model.stats().evaluations==stats.evaluations);
    assert(app.status==status&&app.focused==focused);
    std::size_t i=0;for(const auto& entry:app.editors)assert(history(entry.second)==histories.at(i++));
}
int main(){
    Sdl sdl;App app;app.drain();const auto& first=*app.find("first");
    app.focus(&first);text(app,"B");key(app,'z',KMOD_CTRL);
    assert(app.edit(first).text()=="A"&&app.edit(first).redo_size()==1);
    // Existing redo remains intact after oversize paste, single-line rejection,
    // malformed UTF-8 and derived model failure (unlike Qt's native history).
    assert(SDL_SetClipboardText(std::string(257,'x').c_str())==0);
    rejects_unchanged(app,[&]{key(app,'v',KMOD_CTRL);},"clipboard exceeds query limit");
    rejects_unchanged(app,[&]{text(app,"\n");},"rejects control characters");
    rejects_unchanged(app,[&]{auto c=app.edit(first);c.insert(std::string("\xff",1));app.apply_editor(first,std::move(c));},"invalid UTF-8 text");
    rejects_unchanged(app,[&]{auto c=app.edit(first);c.select_all();c.insert("BAD");app.apply_editor(first,std::move(c));},"integer overflow");
    key(app,'y',KMOD_CTRL);assert(app.edit(first).text()=="AB");
    // Another state can make an old undo value invalid. Native history and the
    // model both remain AB; relaxing the condition makes the same undo usable.
    app.apply_model({{"gate",std::int64_t{1}}});
    rejects_unchanged(app,[&]{key(app,'z',KMOD_CTRL);},"integer overflow");
    app.apply_model({{"gate",std::int64_t{0}}});key(app,'z',KMOD_CTRL);
    assert(app.edit(first).text()=="A"&&app.edit(first).redo_size()==1);
    // Rejected model commit must preserve transient composition and selection,
    // as well as all mirror and unrelated field histories.
    key(app,'a',KMOD_CTRL);SDL_Event composition{};composition.type=SDL_TEXTEDITING;
    std::strcpy(composition.edit.text,"中");composition.edit.start=0;composition.edit.length=1;app.event(composition);
    rejects_unchanged(app,[&]{text(app,"BAD");},"integer overflow");
    rejects_unchanged(app,[&]{auto bad=composition;bad.edit.start=9;app.event(bad);},"composition range exceeds text");
    rejects_unchanged(app,[&]{app.activate(*app.find("bad"));},"integer overflow");
    auto values=app_state::capture(app.model);values["first"]=std::string("BAD");values["second"]=std::string("Changed");
    rejects_unchanged(app,[&]{app.restore_values(values);},"integer overflow");
    values["first"]=std::string("Valid");values["second"]=std::string(257,'x');
    rejects_unchanged(app,[&]{app.restore_values(values);},"single-line text exceeds byte limit");
    // Explicit focus loss intentionally cancels composition, without altering
    // committed state or history; pending composition is not restored by undo.
    auto before=app.edit(first);before.cancel_preedit();app.focus(nullptr);
    assert(history(app.edit(first))==history(before)&&app.model.state_text("first")=="A");
    app.focus(&first);key(app,'y',KMOD_CTRL);assert(app.edit(first).text()=="AB");
    std::cout<<"input_rejection_history_passed: synthetic SDL dummy, no system IME claim\n";
}
'''
        with tempfile.TemporaryDirectory() as directory:
            p = Path(directory)
            (p / 'model.h').write_text(compile_text(json.dumps(model), model=True))
            (p / 'scene.h').write_text(compile_layout(json.dumps(layout), json.dumps(model)))
            (p / 'test.cpp').write_text(source)
            command = [os.environ.get('CXX', 'c++'), '-std=c++17', '-O0', '-pthread',
                       '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I', str(p), '-I', str(ROOT / 'native'),
                       str(p / 'test.cpp'), str(ROOT / 'native/glass_renderer.cpp'), str(ROOT / 'native/shaped_text.cpp')]
            command += ['-Wl,-l:' + name for name in ('libSDL2-2.0.so.0', 'libcairo.so.2',
                'libpangocairo-1.0.so.0', 'libpango-1.0.so.0', 'libgobject-2.0.so.0', 'libglib-2.0.so.0')]
            command += ['-o', str(p / 'test')]
            result = subprocess.run(command, text=True, capture_output=True, timeout=120)
            self.assertEqual(result.returncode, 0, result.stderr)
            env = dict(os.environ, SDL_VIDEODRIVER='dummy');env.pop('EUI_ACCESSIBILITY', None)
            result = subprocess.run([str(p / 'test')], text=True, capture_output=True, timeout=30, env=env)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('input_rejection_history_passed', result.stdout)

if __name__ == '__main__':
    unittest.main()
