"""Compile and run the UTF-8 text editor core, independent of SDL or a display."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class TextEditTests(unittest.TestCase):
    def test_editor_unicode_selection_and_atomic_rejection(self):
        source = r'''
#include "text_edit.h"
#include <cassert>
#include <functional>
#include <limits>
using explicit_ui::TextEdit;
void rejected(TextEdit& e,const std::function<void()>& operation){
 const auto text=e.text(),preedit=e.preedit();const auto caret=e.caret(),anchor=e.anchor();
 const auto ps=e.preedit_selection_start(),pe=e.preedit_selection_end();
 const auto undo=e.undo_size(),redo=e.redo_size();
 bool threw=false;try{operation();}catch(const std::invalid_argument&){threw=true;}
 assert(threw&&e.text()==text&&e.caret()==caret&&e.anchor()==anchor);
 assert(e.preedit()==preedit&&e.preedit_selection_start()==ps&&e.preedit_selection_end()==pe);
 assert(e.undo_size()==undo&&e.redo_size()==redo);
}
int main(){
 TextEdit e("A中😀Z",16);assert(e.caret()==9&&e.text().size()==9);
 e.move_left();assert(e.caret()==8);e.move_left();assert(e.caret()==4);
 e.move_left(true);assert(e.caret()==1&&e.anchor()==4&&e.selected_text()=="中");
 e.insert("文");assert(e.text()=="A文😀Z"&&e.caret()==4&&!e.has_selection());
 e.erase_forward();assert(e.text()=="A文Z");e.backspace();assert(e.text()=="AZ"&&e.caret()==1);
 e.move_home();e.backspace();assert(e.text()=="AZ");e.erase_forward();assert(e.text()=="Z");
 e.move_end();e.erase_forward();assert(e.text()=="Z");
 e.reset("中😀x");e.select(0,7);e.move_right();assert(e.caret()==7&&!e.has_selection());
 e.select(7,0);e.move_left();assert(e.caret()==0&&!e.has_selection());
 e.move_end(true);assert(e.selected_text()=="中😀x");e.move_home(true);assert(!e.has_selection());
 e.select_all();e.insert("");assert(e.text().empty()&&e.caret()==0);
 e.move_left(true);e.move_right(true);e.backspace();e.erase_forward();assert(e.text().empty());
 e.reset("中😀x");e.select(0,3);e.set_preedit("你好",0,3);
 assert(e.text()=="中😀x"&&e.preedit()=="你好");
 rejected(e,[&]{e.insert(std::string("\xff",1));});
 rejected(e,[&]{e.insert(std::string(17,'x'));});
 rejected(e,[&]{e.select(1,3);});rejected(e,[&]{e.select(0,999);});
 rejected(e,[&]{e.select(std::numeric_limits<std::size_t>::max(),0);});
 rejected(e,[&]{e.reset(std::string("\xc0\x80",2));});
 rejected(e,[&]{e.set_preedit("中",1,3);});rejected(e,[&]{e.set_preedit("中",3,0);});
 rejected(e,[&]{e.set_preedit(std::string(17,'a'),0,0);});
 e.commit_preedit("你好");assert(e.text()=="你好😀x"&&e.caret()==6&&e.preedit().empty());
 e.set_preedit("abc",1,2);e.move_left();assert(e.preedit().empty());
 e.select_all();e.insert(std::string(16,'a'));assert(e.text().size()==16);
 e.set_preedit("x",0,1);rejected(e,[&]{e.commit_preedit("x");});
 e.select_all();e.insert("😀");assert(e.text()=="😀");
 e.insert(e.text());assert(e.text()=="😀😀"); // Aliased input is staged safely.
 e.reset(e.text());assert(e.text()=="😀😀");
 TextEdit history("中",256);history.select_all();history.insert("👩‍💻");history.undo();assert(history.text()=="中"&&history.has_selection());history.redo();assert(history.text()=="👩‍💻");
 history.undo();history.insert("文");assert(history.redo_size()==0);history.redo();assert(history.text()=="文");
 history.reset("");for(int i=0;i<100;++i)history.insert("x");assert(history.undo_size()==64);for(int i=0;i<100;++i)history.undo();assert(history.text().size()==36&&history.redo_size()==64);for(int i=0;i<100;++i)history.redo();assert(history.text().size()==100);
 history.reset("中");history.set_preedit("你好",0,6);assert(history.undo_size()==0);history.commit_preedit("你好");assert(history.undo_size()==1);history.undo();assert(history.text()=="中"&&history.preedit().empty());
 // Rejected insertion must not destroy an existing redo branch or composition.
 TextEdit branch("A",2);branch.insert("B");branch.undo();branch.set_preedit("x",0,1);
 rejected(branch,[&]{branch.insert("XYZ");});
 rejected(branch,[&]{branch.insert(std::string("\xff",1));});
 assert(branch.redo_size()==1);branch.redo();assert(branch.text()=="AB"&&branch.preedit().empty());
 branch.undo();assert(branch.text()=="A");branch.redo();assert(branch.text()=="AB");
 TextEdit zero(0);zero.insert("");rejected(zero,[&]{zero.insert("a");});
 TextEdit combining("a\xcc\x81",8);combining.backspace();assert(combining.text()=="a"); // Explicit codepoint semantics.
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "test.cpp").write_text(source)
            subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-O2", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-I", str(ROOT / "native"), str(path / "test.cpp"), "-o", str(path / "test")], check=True, capture_output=True, timeout=30)
            subprocess.run([str(path / "test")], check=True, timeout=10)

if __name__ == "__main__":
    unittest.main()
