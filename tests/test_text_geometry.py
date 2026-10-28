"""Real Pango capture, independent of display/session bus and ATK worker."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
@unittest.skipIf(os.environ.get('EUI_CONSOLE_ONLY')=='1','graphics backend disabled')
class TextGeometryTests(unittest.TestCase):
    def test_bounded_renderer_geometry(self):
        source=r'''
#include "glass_renderer.h"
#include <cassert>
#include <stdexcept>
using namespace explicit_ui;
int hit(const TextGeometry& g,int x){for(auto r:g.hits)if(x>=r.begin&&x<r.end)return r.offset;return -1;}
int main(){
 Widget w;w.x=80;w.y=100;w.width=140;w.height=48;
 TextEdit e("abc",256);auto g=editor_text_geometry(w,e,true);
 assert(g.available&&g.characters.size()==3&&g.characters[0].x==18);
 {auto collapsed=w;collapsed.width=0;auto hidden=editor_text_geometry(collapsed,e,true);
 assert(!hidden.available&&hidden.characters.empty()&&hidden.hits.empty());
 collapsed=w;collapsed.height=0;assert(!editor_text_geometry(collapsed,e,false).available);
 for(int height:{-1,4097}){collapsed.height=height;bool rejected=false;
 try{editor_text_geometry(collapsed,e,true);}catch(const std::length_error&){rejected=true;}assert(rejected);}}
 for(int i=0;i<3;++i){auto r=g.characters[i];assert(hit(g,r.x+r.width/2)==i);}
 e.reset("אבג");g=editor_text_geometry(w,e,true);
 assert(g.characters[0].x>g.characters[1].x&&g.characters[1].x>g.characters[2].x);
 for(int i=0;i<3;++i){auto r=g.characters[i];assert(r.width>0&&hit(g,r.x+r.width/2)==i);}
 e.reset("é 👩‍💻");g=editor_text_geometry(w,e,true);assert(g.characters.size()==6);
 assert(hit(g,g.characters[0].x+1)==0);
 auto emoji=g.characters[3];assert(hit(g,emoji.x+emoji.width/2)==3);
 e.reset(std::string(200,'W'));g=editor_text_geometry(w,e,true);
 assert(g.characters.front().x<0&&g.hits.size()<=4096&&g.characters.size()==200);
 auto unfocused=editor_text_geometry(w,e,false);assert(unfocused.characters.front().x==18&&unfocused.hit_bottom<=w.height/2+13);
 e.move_home();auto home=editor_text_geometry(w,e,true);assert(home.characters.front().x==18);
 e.set_preedit("x",0,1);g=editor_text_geometry(w,e,true);assert(!g.available&&g.characters.empty()&&g.hits.empty());
 e.reset("");g=editor_text_geometry(w,e,true);assert(g.available&&g.characters.empty()&&g.hits.empty()&&g.word_starts==std::vector<int>{0});
 w.width=4097;bool bad=false;try{editor_text_geometry(w,e,true);}catch(const std::length_error&){bad=true;}assert(bad);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);(p/'test.cpp').write_text(source)
            flags=['-Wl,-l:libpangocairo-1.0.so.0','-Wl,-l:libpango-1.0.so.0','-Wl,-l:libgobject-2.0.so.0','-Wl,-l:libglib-2.0.so.0','-Wl,-l:libcairo.so.2']
            subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-O2','-Wall','-Wextra','-Wpedantic','-Werror','-I',str(ROOT/'native'),str(p/'test.cpp'),str(ROOT/'native/shaped_text.cpp'),str(ROOT/'native/glass_renderer.cpp'),*flags,'-o',str(p/'test')],check=True,capture_output=True)
            subprocess.run([str(p/'test')],check=True,capture_output=True,timeout=20,env=dict(os.environ,XDG_CACHE_HOME=str(p/'cache')))
