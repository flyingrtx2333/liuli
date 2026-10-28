#include "shaped_text.h"
#include "model_api.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#if __has_include(<pango/pangocairo.h>)
#include <pango/pangocairo.h>
#else
// Public opaque Pango/GLib ABI subset, not an implementation. Prefer vendor headers.
// Signatures: https://docs.gtk.org/Pango/ and https://docs.gtk.org/PangoCairo/ .
extern "C" {
struct _PangoLayout; using PangoLayout=_PangoLayout;
struct _PangoLayoutLine; using PangoLayoutLine=_PangoLayoutLine;
struct _PangoFontDescription; using PangoFontDescription=_PangoFontDescription;
struct PangoRectangle {int x,y,width,height;};
// Public bitfield layout: https://docs.gtk.org/Pango/struct.LogAttr.html
struct PangoLogAttr {unsigned is_line_break:1,is_mandatory_break:1,is_char_break:1,is_white:1,is_cursor_position:1,is_word_start:1,is_word_end:1,is_sentence_boundary:1,is_sentence_start:1,is_sentence_end:1,backspace_deletes_character:1,is_expandable_space:1,is_word_boundary:1,break_inserts_hyphen:1,break_removes_preceding:1,reserved:17;};
const PangoLogAttr* pango_layout_get_log_attrs_readonly(PangoLayout*,int*);
void pango_layout_move_cursor_visually(PangoLayout*,int,int,int,int,int*,int*);
enum PangoWeight {PANGO_WEIGHT_NORMAL=400,PANGO_WEIGHT_BOLD=700};
PangoLayout* pango_cairo_create_layout(cairo_t*);
void pango_cairo_show_layout(cairo_t*,PangoLayout*);
PangoFontDescription* pango_font_description_new();
void pango_font_description_free(PangoFontDescription*);
void pango_font_description_set_family(PangoFontDescription*,const char*);
void pango_font_description_set_absolute_size(PangoFontDescription*,double);
void pango_font_description_set_weight(PangoFontDescription*,PangoWeight);
void pango_layout_set_font_description(PangoLayout*,const PangoFontDescription*);
void pango_layout_set_text(PangoLayout*,const char*,int);
void pango_layout_set_single_paragraph_mode(PangoLayout*,int);
void pango_layout_get_pixel_size(PangoLayout*,int*,int*);
void pango_layout_index_to_pos(PangoLayout*,int,PangoRectangle*);
void pango_layout_get_cursor_pos(PangoLayout*,int,PangoRectangle*,PangoRectangle*);
int pango_layout_xy_to_index(PangoLayout*,int,int,int*,int*);
PangoLayoutLine* pango_layout_get_line_readonly(PangoLayout*,int);
void pango_layout_line_get_x_ranges(PangoLayoutLine*,int,int,int**,int*);
int pango_layout_get_unknown_glyphs_count(PangoLayout*);
void g_object_unref(void*);
void g_free(void*);
}
#endif
namespace explicit_ui {
namespace {
constexpr double scale=1024;
void check_context(cairo_t* c){if(!c||cairo_status(c)!=CAIRO_STATUS_SUCCESS)throw std::invalid_argument("invalid Cairo text context");}
int units(double value){if(!std::isfinite(value))throw std::invalid_argument("non-finite text coordinate");return static_cast<int>(std::clamp(value*scale,static_cast<double>(std::numeric_limits<int>::min()),static_cast<double>(std::numeric_limits<int>::max())));}
}
struct ShapedText::Impl {
    std::string text;
    std::unique_ptr<PangoLayout,decltype(&g_object_unref)> layout{nullptr,g_object_unref};
    int width=0,height=0;
    void index(std::size_t i)const{if(i>text.size()||(i<text.size()&&(static_cast<unsigned char>(text[i])&0xc0)==0x80))throw std::invalid_argument("invalid UTF-8 text index");}
};
ShapedText::ShapedText(cairo_t* c,const std::string& text,double pixels,bool bold):impl_(std::make_unique<Impl>()){
    check_context(c);
    if(text.size()>65536||!std::isfinite(pixels)||pixels<1||pixels>256)throw std::invalid_argument("shaped text limit exceeded");
    require_utf8(text);
    for(unsigned char b:text)if(b<32||b==127)throw std::invalid_argument("shaped text must be a single line");
    if(text.find("\xe2\x80\xa8")!=std::string::npos||text.find("\xe2\x80\xa9")!=std::string::npos)throw std::invalid_argument("shaped text must be a single line");
    impl_->text=text;
    impl_->layout.reset(pango_cairo_create_layout(c));if(!impl_->layout)throw std::runtime_error("Pango layout creation failed");
    std::unique_ptr<PangoFontDescription,decltype(&pango_font_description_free)> font(pango_font_description_new(),pango_font_description_free);
    if(!font)throw std::runtime_error("Pango font description failed");
    pango_font_description_set_family(font.get(),"sans");pango_font_description_set_absolute_size(font.get(),pixels*scale);pango_font_description_set_weight(font.get(),bold?PANGO_WEIGHT_BOLD:PANGO_WEIGHT_NORMAL);
    pango_layout_set_font_description(impl_->layout.get(),font.get());pango_layout_set_single_paragraph_mode(impl_->layout.get(),1);
    pango_layout_set_text(impl_->layout.get(),text.data(),static_cast<int>(text.size()));
    pango_layout_get_pixel_size(impl_->layout.get(),&impl_->width,&impl_->height);
}
ShapedText::~ShapedText()=default;
ShapedText::ShapedText(ShapedText&&) noexcept=default;
ShapedText& ShapedText::operator=(ShapedText&&) noexcept=default;
double ShapedText::width()const{return impl_->width;}
double ShapedText::height()const{return impl_->height;}
TextRect ShapedText::caret(std::size_t i)const{impl_->index(i);PangoRectangle r{};pango_layout_get_cursor_pos(impl_->layout.get(),static_cast<int>(i),&r,nullptr);return {r.x/scale,r.y/scale,r.width/scale,r.height/scale};}
TextRect ShapedText::character_rect(std::size_t i)const{
    impl_->index(i);PangoRectangle r{};pango_layout_index_to_pos(impl_->layout.get(),static_cast<int>(i),&r);
    const double left=r.x,right=left+r.width;
    return {std::min(left,right)/scale,r.y/scale,std::abs(static_cast<double>(r.width))/scale,r.height/scale};
}
std::size_t ShapedText::character_at_x(double x)const{
    int index=0,trailing=0;
    if(!pango_layout_xy_to_index(impl_->layout.get(),units(x),units(height()/2),&index,&trailing))return impl_->text.size();
    return static_cast<std::size_t>(index); // trailing is for caret placement, not character lookup.
}
std::vector<int> ShapedText::word_starts()const{
    int count=0;const auto* attrs=pango_layout_get_log_attrs_readonly(impl_->layout.get(),&count);
    std::vector<int> out{0};
    for(int n=1;n<count-1;++n)if(attrs[n].is_word_start)out.push_back(n);
    if(count>1)out.push_back(count-1);
    return out;
}
std::size_t ShapedText::hit_test(double x)const{
    int index=0,trailing=0;pango_layout_xy_to_index(impl_->layout.get(),units(x),units(height()/2),&index,&trailing);
    std::size_t at=static_cast<std::size_t>(std::clamp(index,0,static_cast<int>(impl_->text.size())));
    for(int n=0;n<trailing&&at<impl_->text.size();++n){++at;while(at<impl_->text.size()&&(static_cast<unsigned char>(impl_->text[at])&0xc0)==0x80)++at;}
    impl_->index(at);return at;
}
std::vector<TextRect> ShapedText::selection_rects(std::size_t start,std::size_t end)const{
    impl_->index(start);impl_->index(end);if(start>end)throw std::invalid_argument("reversed text selection");if(start==end)return {};
    auto* line=pango_layout_get_line_readonly(impl_->layout.get(),0);if(!line)throw std::runtime_error("missing Pango line");
    int* raw=nullptr;int count=0;pango_layout_line_get_x_ranges(line,static_cast<int>(start),static_cast<int>(end),&raw,&count);
    std::unique_ptr<int,decltype(&g_free)> owned(raw,g_free);std::vector<TextRect> out;out.reserve(static_cast<std::size_t>(count));
    for(int i=0;i<count;++i)out.push_back({raw[2*i]/scale,0,(raw[2*i+1]-raw[2*i])/scale,height()});
    return out;
}
std::vector<std::size_t> ShapedText::grapheme_boundaries()const{
    int count=0;const auto* attrs=pango_layout_get_log_attrs_readonly(impl_->layout.get(),&count);
    std::vector<std::size_t> out;std::size_t at=0;
    for(int n=0;n<count;++n){if(attrs[n].is_cursor_position)out.push_back(at);if(at<impl_->text.size()){++at;while(at<impl_->text.size()&&(static_cast<unsigned char>(impl_->text[at])&0xc0)==0x80)++at;}}
    return out;
}
std::size_t ShapedText::previous_grapheme(std::size_t i)const{impl_->index(i);const auto b=grapheme_boundaries();auto at=std::lower_bound(b.begin(),b.end(),i);return at==b.begin()?0:*--at;}
std::size_t ShapedText::next_grapheme(std::size_t i)const{impl_->index(i);const auto b=grapheme_boundaries();auto at=std::upper_bound(b.begin(),b.end(),i);return at==b.end()?impl_->text.size():*at;}
std::size_t ShapedText::move_visual(std::size_t i,int direction)const{
    impl_->index(i);if(direction!=1&&direction!=-1)throw std::invalid_argument("invalid cursor direction");
    int index=0,trailing=0;pango_layout_move_cursor_visually(impl_->layout.get(),1,static_cast<int>(i),0,direction,&index,&trailing);
    if(index<0||index==std::numeric_limits<int>::max())return i;
    auto at=static_cast<std::size_t>(index);
    for(int n=0;n<trailing&&at<impl_->text.size();++n){++at;while(at<impl_->text.size()&&(static_cast<unsigned char>(impl_->text[at])&0xc0)==0x80)++at;}
    impl_->index(at);return at;
}
int ShapedText::unknown_glyphs()const{return pango_layout_get_unknown_glyphs_count(impl_->layout.get());}
void ShapedText::draw(cairo_t* c,double x,double y)const{check_context(c);units(x);units(y);cairo_save(c);cairo_move_to(c,x,y);pango_cairo_show_layout(c,impl_->layout.get());cairo_restore(c);if(cairo_status(c)!=CAIRO_STATUS_SUCCESS)throw std::runtime_error(cairo_status_to_string(cairo_status(c)));}
}
