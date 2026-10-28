"""Real Pango/Cairo shaping and UTF-8 geometry, no browser or fake text metrics."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
@unittest.skipIf(os.environ.get('EUI_CONSOLE_ONLY')=='1','graphics backend disabled')
class ShapedTextTests(unittest.TestCase):
    def test_shapes_and_maps_utf8_with_checked_geometry(self):
        source=r'''
#include "shaped_text.h"
#include <cassert>
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
using namespace explicit_ui;
template<class F> void invalid(F f){bool failed=false;try{f();}catch(const std::invalid_argument&){failed=true;}assert(failed);}
int main(int argc,char**argv){
 assert(argc==2);
 std::unique_ptr<cairo_surface_t,decltype(&cairo_surface_destroy)> s(cairo_image_surface_create(CAIRO_FORMAT_ARGB32,1000,320),cairo_surface_destroy);
 std::unique_ptr<cairo_t,decltype(&cairo_destroy)> c(cairo_create(s.get()),cairo_destroy);
 cairo_set_source_rgb(c.get(),1,1,1);cairo_paint(c.get());cairo_set_source_rgb(c.get(),.08,.12,.2);
 ShapedText latin(c.get(),"office ffi",26);assert(latin.width()>0&&latin.height()>0);assert(latin.hit_test(-100)==0);assert(latin.hit_test(10000)==10);latin.draw(c.get(),20,20);
 ShapedText chinese(c.get(),u8"中文液态玻璃",30);assert(chinese.unknown_glyphs()==0);assert(chinese.caret_x(3)>chinese.caret_x(0));assert(chinese.caret_x(6)>chinese.caret_x(3));
 const auto middle=(chinese.caret_x(0)+chinese.caret_x(3))*.25;assert(chinese.hit_test(middle)==0);
 const auto chosen=chinese.selection_rects(3,9);assert(!chosen.empty());for(auto r:chosen){assert(r.width>=0&&r.height>0);cairo_set_source_rgb(c.get(),.65,.8,1);cairo_rectangle(c.get(),20+r.x,75+r.y,r.width,r.height);cairo_fill(c.get());}cairo_set_source_rgb(c.get(),.08,.12,.2);chinese.draw(c.get(),20,75);
 invalid([&]{chinese.caret(1);});invalid([&]{chinese.selection_rects(9,3);});invalid([&]{chinese.hit_test(std::numeric_limits<double>::quiet_NaN());});
 ShapedText bidi(c.get(),u8"abc אבג العربية xyz",28);assert(bidi.unknown_glyphs()==0);auto rs=bidi.selection_rects(1,11);assert(!rs.empty());for(auto r:rs)assert(r.width>=0);bidi.draw(c.get(),20,135);
 ShapedText cluster(c.get(),u8"é 👩‍💻",28);for(int x=-20;x<250;++x){auto at=cluster.hit_test(x);assert(at<=std::string(u8"é 👩‍💻").size());cluster.caret(at);}cluster.draw(c.get(),20,195);
 const auto boundaries=cluster.grapheme_boundaries();assert((boundaries==std::vector<std::size_t>{0,3,4,15}));
 assert(cluster.previous_grapheme(15)==4&&cluster.next_grapheme(4)==15&&cluster.move_visual(4,1)==15&&cluster.move_visual(15,-1)==4);
 for(int x=-20;x<250;++x){auto at=cluster.hit_test(x);assert(std::find(boundaries.begin(),boundaries.end(),at)!=boundaries.end());}
 auto er=cluster.character_rect(0),ar=cluster.character_rect(1);assert(er.width>=0&&ar.width>=0);
 assert(cluster.character_at_x(er.x+er.width/2)==0);
 assert((ShapedText(c.get(),"one  two! three",16).word_starts()==std::vector<int>{0,5,10,15}));
 auto first=bidi.character_rect(4);assert(first.width>=0&&first.height>0);
 ShapedText flags(c.get(),u8"🇨🇳👍🏽",28);assert((flags.grapheme_boundaries()==std::vector<std::size_t>{0,8,16}));
 ShapedText rtl(c.get(),u8"אבג",28);assert(rtl.move_visual(0,-1)==2&&rtl.move_visual(2,1)==0);
 ShapedText empty(c.get(),"",20);assert(empty.width()==0&&empty.hit_test(10)==0&&empty.selection_rects(0,0).empty());
 invalid([&]{ShapedText t(c.get(),std::string("\xff"),20);});invalid([&]{ShapedText t(c.get(),"a\nb",20);});invalid([&]{ShapedText t(c.get(),std::string(65537,'a'),20);});invalid([&]{ShapedText t(c.get(),"x",0);});
 for(int n=0;n<200;++n){ShapedText t(c.get(),u8"循环 عربي",14);assert(t.width()>0);t.selection_rects(0,6);}
 assert(cairo_surface_write_to_png(s.get(),argv[1])==CAIRO_STATUS_SUCCESS);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);(p/'test.cpp').write_text(source)
            flags=['-Wl,-l:libpangocairo-1.0.so.0','-Wl,-l:libpango-1.0.so.0','-Wl,-l:libgobject-2.0.so.0','-Wl,-l:libglib-2.0.so.0','-Wl,-l:libcairo.so.2']
            subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-O2','-Wall','-Wextra','-Wpedantic','-Werror','-I',str(ROOT/'native'),str(p/'test.cpp'),str(ROOT/'native/shaped_text.cpp'),*flags,'-o',str(p/'test')],check=True,capture_output=True)
            output=ROOT/'evidence/v0.4/shaped-text.png';output.parent.mkdir(parents=True,exist_ok=True)
            result=subprocess.run([str(p/'test'),str(output)],capture_output=True,text=True,timeout=20,env=dict(os.environ,XDG_CACHE_HOME=str(p/'cache')))
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertEqual(output.read_bytes()[:8],b'\x89PNG\r\n\x1a\n')
if __name__=='__main__':unittest.main()
