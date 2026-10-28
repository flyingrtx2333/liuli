"""Optional bool-enabled buttons: compiler contract and real host event guards."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from compiler import compile_text, CompileError
from layout_compiler import compile_layout

class EnabledControlsTests(unittest.TestCase):
    def test_binding_validation(self):
        model=(ROOT/'examples/enabled_controls.eui.json').read_text()
        layout=json.loads((ROOT/'examples/enabled_controls.layout.json').read_text())
        compile_layout(json.dumps(layout),model)
        for value in ('missing','counter','',True,1):
            layout['widgets'][1]['enabled_view']=value
            with self.assertRaises(CompileError):compile_layout(json.dumps(layout),model)
        del layout['widgets'][1]['enabled_view']
        layout['widgets'][2]['enabled_view']='enabled'
        with self.assertRaises(CompileError):compile_layout(json.dumps(layout),model)
        # Flow layouts accept the same optional field, including legacy delta actions.
        button={'id':'increment','kind':'button','basis':[200,60],'grow':0,'text':'Increment','state':'count','delta':1,'enabled_view':'enabled'}
        flow={'version':2,'width':640,'height':480,'root':button}
        compile_layout(json.dumps(flow),model)
        for value in ('missing','counter',False):
            button['enabled_view']=value
            with self.assertRaises(CompileError):compile_layout(json.dumps(flow),model)

    @unittest.skipIf(os.environ.get('EUI_CONSOLE_ONLY')=='1','native host disabled')
    def test_host_events(self):
        model=(ROOT/'examples/enabled_controls.eui.json').read_text()
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory)
            (p/'model.h').write_text(compile_text(model,model=True))
            (p/'scene.h').write_text(compile_layout((ROOT/'examples/enabled_controls.layout.json').read_text(),model))
            (p/'test.cpp').write_text(r"""
#define main explicit_ui_window_program_main
#include "window_host.cpp"
#undef main
#include <cassert>
int main(){
 Sdl sdl;App app;app.resize();
 const auto* toggle=app.find("toggle");const auto* increment=app.find("increment");
 assert(toggle&&increment&&toggle->enabled&&!increment->enabled);
 assert(!hit_test(app.layout,increment->x+10,increment->y+10));
 auto paints=app.paints;
 app.activate(*increment);app.focus(increment);assert(app.focused.empty()&&app.paints==paints);
 SDL_Event click{};click.type=SDL_MOUSEBUTTONDOWN;click.button.button=SDL_BUTTON_LEFT;click.button.x=increment->x+10;click.button.y=increment->y+10;app.dispatch(click);
 assert(app.model.state_int("count")==0&&app.paints==paints);
 app.tab(false);assert(app.focused=="toggle");paints=app.paints;app.tab(false);app.tab(true);assert(app.focused=="toggle"&&app.paints==paints);
 auto disabled_frame=app.frame;
 app.activate(*toggle);assert(increment->enabled);
 bool distinct=false;
 for(int y=increment->y;y<increment->y+increment->height;++y)
   for(int x=increment->x;x<increment->x+increment->width;++x)
     if(disabled_frame.pixels[y*disabled_frame.stride+x*4]!=app.frame.pixels[y*app.frame.stride+x*4])distinct=true;
 assert(distinct);app.tab(false);assert(app.focused=="increment");
 app.key(SDLK_RETURN);assert(app.model.state_int("count")==1);
 // Programmatic activation of the other button must clear newly disabled focus.
 app.activate(*toggle);assert(!increment->enabled&&app.focused.empty());
 paints=app.paints;app.key(SDLK_RETURN);app.key(' ');app.activate(*increment);
 assert(app.model.state_int("count")==1&&app.paints==paints);
 // Unchanged view values do not force a repaint. Model actions remain callable.
 assert(!app.apply_model({{"enabled",false}}));assert(app.paints==paints);
 app.model.invoke_action("increment");assert(app.model.state_int("count")==2);app.paint();
#ifdef EUI_LINUX_ACCESSIBILITY
 app.accessible_queue=std::make_shared<App::AccessibleQueue>();
 app.accessible_queue->requests.push_back({AccessibilityRequest::Kind::activate,"increment",{}});
 app.accessible_queue->requests.push_back({AccessibilityRequest::Kind::focus,"increment",{}});
 paints=app.paints;app.accessibility_event();assert(app.model.state_int("count")==2&&app.focused.empty()&&app.paints==paints);
 // Both requests were queued when enabled; Toggle disables before stale activation.
 app.activate(*toggle);
 app.accessible_queue->requests.push_back({AccessibilityRequest::Kind::activate,"toggle",{}});
 app.accessible_queue->requests.push_back({AccessibilityRequest::Kind::activate,"increment",{}});
 app.accessibility_event();assert(!increment->enabled&&app.model.state_int("count")==2);
#endif
 // A constrained flow may round a small child to zero pixels. Layout
 // publication uses the same paint path as resize, clearing its old focus.
 {
   App collapsed;collapsed.activate(*collapsed.find("toggle"));
   collapsed.focus(collapsed.find("increment"));assert(collapsed.focused=="increment");
   Flow narrow{true,*collapsed.find("increment"),Axis::row,1,60,0,0,0,0,{}};
   Flow wide{true,*collapsed.find("toggle"),Axis::row,4096,60,0,0,0,0,{}};
   Flow root{false,{},Axis::row,0,0,0,0,0,0,{narrow,wide}};
   collapsed.layout=resolve_layout(root,320,320);collapsed.paint();
   const auto* tiny=collapsed.find("increment");
   assert(tiny&&tiny->width==0&&tiny->enabled&&collapsed.model.view_bool("enabled"));
   assert(collapsed.focused.empty());auto before=collapsed.paints;
   collapsed.focus(tiny);collapsed.activate(*tiny);collapsed.key(SDLK_RETURN);
   assert(collapsed.focused.empty()&&collapsed.model.state_int("count")==0&&collapsed.paints==before);
   collapsed.tab(false);assert(collapsed.focused=="toggle");
   before=collapsed.paints;collapsed.tab(true);assert(collapsed.focused=="toggle"&&collapsed.paints==before);
#ifdef EUI_LINUX_ACCESSIBILITY
   collapsed.accessible_queue=std::make_shared<App::AccessibleQueue>();
   collapsed.accessible_queue->requests.push_back({AccessibilityRequest::Kind::activate,"increment",{}});
   collapsed.accessible_queue->requests.push_back({AccessibilityRequest::Kind::focus,"increment",{}});
   collapsed.accessibility_event();assert(collapsed.focused=="toggle"&&collapsed.model.state_int("count")==0&&collapsed.paints==before);
#endif
   // Height and negative synthetic rectangles obey the same admission rule.
   for(auto& w:collapsed.layout.widgets)if(w.id=="increment"){w.width=200;w.height=0;}
   collapsed.focus(tiny);collapsed.activate(*tiny);assert(collapsed.focused=="toggle"&&collapsed.model.state_int("count")==0);
   for(auto& w:collapsed.layout.widgets)if(w.id=="increment"){w.width=-1;w.height=60;}
   collapsed.focus(tiny);collapsed.activate(*tiny);assert(collapsed.focused=="toggle"&&collapsed.model.state_int("count")==0);
 }
 // A disabled top button is a blocker, not pointer-events:none.
 // The text widget is last/topmost and intentionally remains pass-through.
 {
   App overlap;
   for(auto& w:overlap.layout.widgets){w.x=40;w.y=40;w.width=200;w.height=60;}
   overlap.paint();const auto before=overlap.paints;
   SDL_Event mouse{};mouse.type=SDL_MOUSEBUTTONDOWN;mouse.button.button=SDL_BUTTON_LEFT;mouse.button.x=60;mouse.button.y=60;
   overlap.dispatch(mouse);
   assert(!overlap.model.state_bool("enabled")); // underlying Toggle did not run
   assert(overlap.model.state_int("count")==0&&overlap.focused.empty()&&overlap.paints==before);
   overlap.activate(*overlap.find("toggle"));
   overlap.dispatch(mouse);
   assert(overlap.model.state_bool("enabled")&&overlap.model.state_int("count")==1);
   assert(overlap.focused=="increment"); // enabled top button through text overlay
 }
}
""")
            flags=['-Wl,-l:libSDL2-2.0.so.0','-Wl,-l:libcairo.so.2','-Wl,-l:libpangocairo-1.0.so.0','-Wl,-l:libpango-1.0.so.0','-Wl,-l:libgobject-2.0.so.0','-Wl,-l:libglib-2.0.so.0']
            extras=[]
            sdk=os.environ.get('EUI_A11Y_SDK')
            if sdk:
                extras=['-DEUI_LINUX_ACCESSIBILITY=1',str(ROOT/'native/linux_accessibility.cpp'),'-ldl','-Wl,-l:libatk-1.0.so.0','-Wl,-l:libatk-bridge-2.0.so.0']
                for suffix in ('usr/include/atk-1.0','usr/include/at-spi2-atk/2.0','usr/include/glib-2.0','usr/lib/x86_64-linux-gnu/glib-2.0/include'):
                    extras+=['-isystem',str(Path(sdk)/suffix)]
            subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-O0','-pthread','-Wall','-Wextra','-Wpedantic','-Werror','-I',str(p),'-I',str(ROOT/'native'),str(p/'test.cpp'),str(ROOT/'native/glass_renderer.cpp'),str(ROOT/'native/shaped_text.cpp'),*extras,*flags,'-o',str(p/'test')],check=True,timeout=90)
            env=dict(os.environ,SDL_VIDEODRIVER='dummy',XDG_CACHE_HOME='/tmp/explicit-ui-font-cache')
            env.pop('EUI_ACCESSIBILITY',None) # Queue guards need no provider or bus.
            subprocess.run([str(p/'test')],check=True,timeout=30,env=env)
