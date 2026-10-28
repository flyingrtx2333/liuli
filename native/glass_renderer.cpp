#include "glass_renderer.h"
#include "cairo_abi.h"
#include "shaped_text.h"
#include "display_text.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

namespace explicit_ui {
namespace {
constexpr int width=1120, height=840;
constexpr double pi=3.141592653589793;
struct Pixel { double r,g,b; };
// Compact storage only; procedural colors and blur accumulators stay double.
struct StoredPixel {
    float r=0,g=0,b=0;
    StoredPixel() = default;
    StoredPixel(Pixel p):r(static_cast<float>(p.r)),g(static_cast<float>(p.g)),b(static_cast<float>(p.b)){}
    operator Pixel() const {return {r,g,b};}
};
using Surface=std::unique_ptr<cairo_surface_t, decltype(&cairo_surface_destroy)>;
using Context=std::unique_ptr<cairo_t, decltype(&cairo_destroy)>;
const Pixel ink{.10,.16,.25}, muted{.30,.36,.44}, blue{.12,.30,.58};
void check(cairo_status_t s) {
    if(s!=CAIRO_STATUS_SUCCESS) throw std::runtime_error(cairo_status_to_string(s));
}
Surface surface(int w=width,int h=height) {
    Surface s(cairo_image_surface_create(CAIRO_FORMAT_ARGB32,w,h),cairo_surface_destroy);
    check(cairo_surface_status(s.get())); return s;
}
double editor_origin(const Widget& w,const ShapedText& shaped,std::size_t caret){return w.x+18-std::max(0.0,shaped.caret(caret).x-std::max(1,w.width-36)+2);}
void color(cairo_t* c,Pixel p,double a=1) {cairo_set_source_rgba(c,p.r,p.g,p.b,a);}
void rounded(cairo_t*c,double x,double y,double w,double h,double r) {
    cairo_move_to(c,x+r,y);cairo_line_to(c,x+w-r,y);
    cairo_arc(c,x+w-r,y+r,r,-pi/2,0);cairo_arc(c,x+w-r,y+h-r,r,0,pi/2);
    cairo_arc(c,x+r,y+h-r,r,pi/2,pi);cairo_arc(c,x+r,y+r,r,pi,pi*1.5);cairo_close_path(c);
}
void box(cairo_t*c,double x,double y,double w,double h,double r,Pixel p,double a=1) {
    if(w<=0||h<=0)return;
    rounded(c,x,y,w,h,std::min(r,std::min(w,h)/2));color(c,p,a);cairo_fill(c);
}
void outline(cairo_t*c,double x,double y,double w,double h,double r,Pixel p,double a,double thickness=1) {
    if(w<=0||h<=0)return;
    rounded(c,x,y,w,h,std::min(r,std::min(w,h)/2));color(c,p,a);cairo_set_line_width(c,thickness);cairo_stroke(c);
}

void text(cairo_t*c,double x,double y,const std::string&s,double size=15,Pixel p=ink,bool bold=false,double max_width=900) {
    if(max_width<=0)return;
    cairo_save(c);cairo_rectangle(c,x,y-size-4,max_width,size+12);cairo_clip(c);
    color(c,p);const auto safe=display_text(s);ShapedText shaped(c,safe,size,bold);shaped.draw(c,x,y-size);cairo_restore(c);
}
Pixel mix(Pixel a,Pixel b,double t) {return {a.r+(b.r-a.r)*t,a.g+(b.g-a.g)*t,a.b+(b.b-a.b)*t};}
// Sliding-window box blur: constant work per pixel, with edge clamping.
void blur(std::vector<StoredPixel>& src,std::vector<StoredPixel>& tmp,int radius,bool horizontal) {
    const int lines=horizontal?height:width, length=horizontal?width:height;
    const auto index=[&](int line,int pos){return horizontal?line*width+pos:pos*width+line;};
    for(int line=0;line<lines;++line) {
        Pixel sum{0,0,0};
        auto add=[&](int pos,double sign){const auto p=src[index(line,std::clamp(pos,0,length-1))];sum.r+=p.r*sign;sum.g+=p.g*sign;sum.b+=p.b*sign;};
        for(int k=-radius;k<=radius;++k)add(k,1);
        for(int pos=0;pos<length;++pos) {
            tmp[index(line,pos)]=Pixel{sum.r/(radius*2+1),sum.g/(radius*2+1),sum.b/(radius*2+1)};
            add(pos-radius,-1);add(pos+radius+1,1);
        }
    }
    src.swap(tmp);
}
void put_pixels(cairo_surface_t*s,const std::vector<StoredPixel>&pixels) {
    cairo_surface_flush(s);auto*data=cairo_image_surface_get_data(s);const int stride=cairo_image_surface_get_stride(s);
    auto byte=[](double v){return static_cast<std::uint32_t>(std::clamp(v,0.0,1.0)*255+.5);};
    for(int y=0;y<height;++y)for(int x=0;x<width;++x) {
        const auto p=pixels[y*width+x];const std::uint32_t argb=0xff000000U|(byte(p.r)<<16)|(byte(p.g)<<8)|byte(p.b);
        std::memcpy(data+y*stride+x*4,&argb,4);
    }
    cairo_surface_mark_dirty(s);
}
struct Renderer {
    Surface cached{nullptr,cairo_surface_destroy};
    GlassMetrics metrics;
    bool layout_mode=false;
    explicit Renderer(bool mode=false):layout_mode(mode){}
    void build() {
        if(cached)return;
        std::vector<StoredPixel> blurred(width*height), temporary(width*height);
        for(int y=0;y<height;++y)for(int x=0;x<width;++x) {
            const double u=double(x)/width,v=double(y)/height;
            Pixel p=mix({.80,.87,.98},{.97,.85,.75},u);
            const double cyan=std::exp(-((u-.24)*(u-.24)/.10+(v-.72)*(v-.72)/.17));
            p=mix(p,{.23,.71,.81},cyan*.88);
            const double lilac=std::exp(-((u-.70)*(u-.70)/.12+(v-.20)*(v-.20)/.14));
            p=mix(p,{.68,.61,.90},lilac*.88);
            const double wave=std::sin(u*15+v*8+std::sin(v*8)*1.4);
            const double ribbon=std::pow(std::max(0.0,wave),12)*.18;
            p=mix(p,{1,1,1},ribbon);
            blurred[y*width+x]=p;
        }
        auto bg=surface();put_pixels(bg.get(),blurred);
        for(int pass=0;pass<3;++pass){blur(blurred,temporary,12,true);blur(blurred,temporary,12,false);}
        // Refractive-looking displacement is sampled from the actual blurred
        // backdrop, strongest at the perimeter; this is not physical ray tracing.
        for(int y=0;y<height;++y)for(int x=0;x<width;++x) {
            const double nx=(x-560.0)/496,ny=(y-445.0)/325;
            const double edge=std::pow(std::min(1.0,std::max(std::abs(nx),std::abs(ny))),9);
            const int sx=std::clamp(int(x+nx*edge*17),0,width-1),sy=std::clamp(int(y+ny*edge*17),0,height-1);
            temporary[y*width+x]=mix(blurred[sy*width+sx],{1,1,1},.45);
        }
        auto material=surface();put_pixels(material.get(),temporary);
        // No float work buffers overlap the final cache surface/text setup.
        std::vector<StoredPixel>().swap(blurred);
        std::vector<StoredPixel>().swap(temporary);
        auto target=surface();Context c(cairo_create(target.get()),cairo_destroy);check(cairo_status(c.get()));
        cairo_set_source_surface(c.get(),bg.get(),0,0);cairo_paint(c.get());
        if(layout_mode) {
            cairo_set_source_surface(c.get(),material.get(),0,0);cairo_paint(c.get());
            check(cairo_status(c.get()));cached=std::move(target);++metrics.cache_builds;return;
        }
        text(c.get(),66,65,"EXPLICIT / UI",14,ink,true);
        text(c.get(),824,65,"NATIVE MATERIAL STUDY   /   02",11,muted,true,250);
        for(int i=20;i>=1;--i)box(c.get(),64-i*.6,120+i*.6,992+i*1.2,650+i*.6,37+i*.4,{.19,.28,.42},.006);
        cairo_save(c.get());rounded(c.get(),64,120,992,650,36);cairo_clip(c.get());
        cairo_set_source_surface(c.get(),material.get(),0,0);cairo_paint(c.get());cairo_restore(c.get());
        outline(c.get(),64.5,120.5,991,649,36,{1,1,1},.85,1.5);
        outline(c.get(),67,123,986,643,34,{1,1,1},.24);
        if (!layout_mode) {
        box(c.get(),88,146,272,598,26,{1,1,1},.29);
        box(c.get(),108,166,38,38,13,{1,1,1},.66);text(c.get(),119,192,"E",21,blue,true);
        text(c.get(),158,182,"Explicit",18,ink,true);text(c.get(),158,201,"STATE, MADE VISIBLE",9,muted,true);
        text(c.get(),110,247,"WORKSPACE",10,muted,true);
        box(c.get(),104,264,240,44,15,{1,1,1},.67);
        box(c.get(),119,278,13,13,4,blue,.9);text(c.get(),145,292,"Live state",14,ink,true);
        text(c.get(),112,347,"Counter",14,muted);
        box(c.get(),108,452,232,43,21,blue,.92);text(c.get(),152,480,"Updated via stdin",14,{1,1,1},true);
        text(c.get(),110,535,"ONE MODEL. DIRECT PIXELS.",10,muted,true);
        text(c.get(),110,560,"Persistent C++ state",13,ink);
        text(c.get(),110,582,"Cached glass backdrop",13,ink);
        text(c.get(),110,604,"Redraw on visible change",13,ink);
        box(c.get(),108,659,232,61,17,{1,1,1},.45);
        text(c.get(),122,684,"CPU / Cairo",12,ink,true);
        text(c.get(),122,704,"Headless PNG renderer",11,muted);
        text(c.get(),393,186,"A little clarity.",31,ink,true);
        text(c.get(),394,216,"Live values, through a softer lens.",14,muted);
        box(c.get(),390,241,638,49,22,{1,1,1},.58);
        color(c.get(),muted);cairo_set_line_width(c.get(),1.7);cairo_arc(c.get(),413,263,6,0,2*pi);cairo_stroke(c.get());
        cairo_move_to(c.get(),417,268);cairo_line_to(c.get(),422,273);cairo_stroke(c.get());
        box(c.get(),390,312,638,360,23,{1,1,1},.40);
        text(c.get(),411,344,"Visible items",16,ink,true);
        text(c.get(),411,699,"RUNTIME TELEMETRY",9,muted,true);
        text(c.get(),66,806,"GLASS-INSPIRED / Linux CPU approximation, not an Apple material",11,muted);
        text(c.get(),840,806,"C++ state  →  Cairo  →  PNG",11,muted,false,245);
        }
        check(cairo_status(c.get()));cached=std::move(target);++metrics.cache_builds;
    }
    void render(const ViewModel&model,const std::string&path) {
        build();auto frame=surface();Context c(cairo_create(frame.get()),cairo_destroy);check(cairo_status(c.get()));
        cairo_set_source_surface(c.get(),cached.get(),0,0);cairo_paint(c.get());
        const auto& counter=model.view_text("counter");
        const auto value=counter.rfind("Count: ",0)==0?counter.substr(7):counter;
        const double counter_size=std::min(58.0,350.0/std::max<std::size_t>(1,value.size()));
        text(c.get(),109,421,value,counter_size,ink,true,230);
        const auto&rows=model.view_texts("list");
        text(c.get(),440,272,"Filtered / windowed results",14,muted,false,555);
        text(c.get(),891,344,std::to_string(rows.size())+" in view",12,muted,false,115);
        const auto count=std::min<std::size_t>(7,rows.size());
        for(std::size_t i=0;i<count;++i){
            const double y=359+i*40;
            if(i%2==0)box(c.get(),402,y,614,38,12,{1,1,1},.35);
            box(c.get(),414,y+9,20,20,7,{.56,.70,.85},.20);
            text(c.get(),420,y+23,std::to_string(i+1),10,blue,true,21);
            text(c.get(),448,y+25,rows[i],14,ink,false,525);
        }
        if(rows.empty())text(c.get(),412,401,"No matching items",15,muted);
        if(rows.size()>count)text(c.get(),412,657,"Showing first 7 of "+std::to_string(rows.size())+" current runtime rows",10,muted);
        const auto s=model.stats();
        text(c.get(),394,724,std::to_string(s.patches)+" patches   /   "+std::to_string(s.rows_materialized)+" rows materialized   /   "+std::to_string(s.evaluations)+" evaluations",12,ink,false,630);
        check(cairo_status(c.get()));check(cairo_surface_write_to_png(frame.get(),path.c_str()));++metrics.render_frames;
    }
};
Renderer& renderer(){static thread_local Renderer r;return r;}
} // namespace
void render_glass(const ViewModel&model,const std::string&output_path){renderer().render(model,output_path);}
GlassMetrics glass_metrics(){return renderer().metrics;}

PixelFrame render_layout(const ViewModel& model,const Layout& layout,const std::string& query,const std::string& focused_id,const TextEdit* legacy_editor,const std::map<std::string,TextEdit>* editors) {
    if(layout.width<1||layout.height<1||layout.width>4096||layout.height>4096) throw std::invalid_argument("unsupported render size");
    static thread_local Renderer material(true);
    material.build();
    auto frame=surface(layout.width,layout.height); Context c(cairo_create(frame.get()),cairo_destroy);check(cairo_status(c.get()));
    // Only the decorative backdrop scales. Widgets and glyphs rasterize at the
    // current window resolution after a fresh flow-layout pass.
    cairo_save(c.get());cairo_scale(c.get(),double(layout.width)/width,double(layout.height)/height);
    cairo_set_source_surface(c.get(),material.cached.get(),0,0);cairo_paint(c.get());cairo_restore(c.get());
    box(c.get(),16,16,layout.width-32,layout.height-32,24,{1,1,1},.22);
    outline(c.get(),16.5,16.5,layout.width-33,layout.height-33,24,{1,1,1},.7,1.5);
    for(const auto&w:layout.widgets) {
        if(w.width<=0||w.height<=0)continue;
        cairo_save(c.get());cairo_rectangle(c.get(),w.x,w.y,w.width,w.height);cairo_clip(c.get());
        const bool focused=w.id==focused_id;
        switch(w.kind) {
        case WidgetKind::text: {
            const auto value=w.binding.empty()?w.text:model.view_text(w.binding);
            const double size=w.height>=50?27:14;
            text(c.get(),w.x,w.y+size+5,value,size,ink,w.height>=50,w.width);break;
        }
        case WidgetKind::button:
            box(c.get(),w.x,w.y,w.width,w.height,17,w.enabled?blue:muted,w.enabled?.9:.25);
            text(c.get(),w.x+20,w.y+w.height/2+6,w.text,18,w.enabled?Pixel{1,1,1}:muted,true,w.width-30);
            break;
        case WidgetKind::search: {
            const auto* editor=editors?&editors->at(w.id):legacy_editor;
            const auto& field_query=editor?editor->text():query;
            box(c.get(),w.x,w.y,w.width,w.height,19,{1,1,1},.64);
            if(!focused||!editor){text(c.get(),w.x+18,w.y+w.height/2+5,field_query.empty()?w.text:field_query,16,field_query.empty()?muted:ink,false,w.width-36);break;}
            cairo_save(c.get());cairo_rectangle(c.get(),w.x+14,w.y+6,std::max(0,w.width-28),std::max(0,w.height-12));cairo_clip(c.get());
            std::string shown=field_query;std::size_t caret=editor->caret(),start=editor->selection_start(),end=editor->selection_end();
            const bool composing=!editor->preedit().empty();const auto preedit_at=start;
            if(composing){shown=field_query.substr(0,start)+editor->preedit()+field_query.substr(end);caret=start+editor->preedit_selection_end();start+=editor->preedit_selection_start();end=preedit_at+editor->preedit_selection_end();}
            ShapedText shaped(c.get(),shown,16);const auto cursor=shaped.caret(caret);
            const double origin=editor_origin(w,shaped,caret),top=w.y+(w.height-shaped.height())/2;
            for(const auto& rect:shaped.selection_rects(start,end))box(c.get(),origin+rect.x,top+rect.y,rect.width,rect.height,0,blue,.24);
            color(c.get(),ink);shaped.draw(c.get(),origin,top);
            if(shown.empty())text(c.get(),w.x+18,w.y+w.height/2+5,w.text,16,muted,false,w.width-36);
            if(composing){for(const auto& rect:shaped.selection_rects(preedit_at,preedit_at+editor->preedit().size())){color(c.get(),blue);cairo_set_line_width(c.get(),1);cairo_move_to(c.get(),origin+rect.x,top+rect.height);cairo_line_to(c.get(),origin+rect.x+rect.width,top+rect.height);cairo_stroke(c.get());}}
            color(c.get(),blue);cairo_set_line_width(c.get(),1.5);cairo_move_to(c.get(),origin+cursor.x,top+cursor.y);cairo_line_to(c.get(),origin+cursor.x,top+cursor.y+cursor.height);cairo_stroke(c.get());cairo_restore(c.get());
            break;
        }
        case WidgetKind::list: {
            box(c.get(),w.x,w.y,w.width,w.height,23,{1,1,1},.43);
            const auto&rows=model.view_texts(w.binding);
            text(c.get(),w.x+20,w.y+30,"Visible items",16,ink,true,w.width-40);
            const int capacity=std::max(0,(w.height-64)/36);
            const auto count=std::min<std::size_t>(rows.size(),static_cast<std::size_t>(capacity));
            for(std::size_t i=0;i<count;++i) {
                const double y=w.y+42+i*36;
                if(i%2==0)box(c.get(),w.x+10,y,w.width-20,34,10,{1,1,1},.32);
                text(c.get(),w.x+22,y+23,rows[i],14,ink,false,w.width-44);
            }
            if(rows.empty())text(c.get(),w.x+20,w.y+75,"No matching items",14,muted,false,w.width-40);
            if(rows.size()>count)text(c.get(),w.x+20,w.y+w.height-8,"Showing "+std::to_string(count)+" of "+std::to_string(rows.size())+" runtime rows; scroll for more",10,muted,false,w.width-40);
            break;
        }
        }
        if(focused)outline(c.get(),w.x+2,w.y+2,w.width-4,w.height-4,17,blue,.95,2);
        cairo_restore(c.get());
    }
    check(cairo_status(c.get()));cairo_surface_flush(frame.get());
    const int stride=cairo_image_surface_get_stride(frame.get());
    const auto*data=cairo_image_surface_get_data(frame.get());
    ++material.metrics.render_frames;
    return {layout.width,layout.height,stride,std::vector<unsigned char>(data,data+stride*layout.height)};
}
TextGeometry editor_text_geometry(const Widget& w,const TextEdit& editor,bool focused){
    if(editor.text().size()>256||w.width<0||w.width>4096||w.height<0||w.height>4096)throw std::length_error("accessible editor geometry limit exceeded");
    auto s=surface(1,1);Context c(cairo_create(s.get()),cairo_destroy);check(cairo_status(c.get()));
    ShapedText shaped(c.get(),editor.text(),16);TextGeometry out;out.word_starts=shaped.word_starts();
    if(!editor.preedit().empty()||w.width==0||w.height==0)return out;
    out.available=true;
    // Unfocused renderer uses text(..., baseline, size): top = baseline - size.
    const double origin=focused?editor_origin(w,shaped,editor.caret())-w.x:18;
    const double top=focused?(w.height-shaped.height())/2:w.height/2+5-16;
    std::vector<std::size_t> bytes;
    for(std::size_t i=0;i<editor.text().size();++i)if((static_cast<unsigned char>(editor.text()[i])&0xc0)!=0x80)bytes.push_back(i);
    for(auto byte:bytes){auto r=shaped.character_rect(byte);const auto left=static_cast<int>(std::floor(origin+r.x)),upper=static_cast<int>(std::floor(top+r.y));
        out.characters.push_back({left,upper,static_cast<int>(std::ceil(origin+r.x+r.width))-left,static_cast<int>(std::ceil(top+r.y+r.height))-upper});}
    const int clip_left=focused?14:18,clip_right=std::max(clip_left,w.width-(focused?14:18));
    out.hit_top=std::max(focused?6:w.height/2-15,static_cast<int>(std::ceil(top)));
    out.hit_bottom=std::min(focused?w.height-6:w.height/2+13,static_cast<int>(std::ceil(top+shaped.height())));
    // ATK coordinates are integral; sample Pango's exact mapping, compress equal runs.
    // Bounded to 4096 samples, <=4096 runs; no Pango object crosses threads.
    for(int x=clip_left;x<clip_right;++x){auto byte=shaped.character_at_x(x-origin);if(byte==editor.text().size())continue;
        const int offset=static_cast<int>(std::lower_bound(bytes.begin(),bytes.end(),byte)-bytes.begin());
        if(!out.hits.empty()&&out.hits.back().end==x&&out.hits.back().offset==offset)out.hits.back().end=x+1;
        else out.hits.push_back({x,x+1,offset});}
    return out;
}
std::size_t editor_hit_test(const Widget& w,const TextEdit& editor,double x){
    auto s=surface(1,1);Context c(cairo_create(s.get()),cairo_destroy);check(cairo_status(c.get()));
    ShapedText shaped(c.get(),editor.text(),16);
    return shaped.hit_test(x-editor_origin(w,shaped,editor.caret()));
}
void editor_snap_graphemes(TextEdit& editor){
    auto s=surface(1,1);Context c(cairo_create(s.get()),cairo_destroy);check(cairo_status(c.get()));
    ShapedText shaped(c.get(),editor.text(),16);const auto b=shaped.grapheme_boundaries();
    // Inserting before a combining suffix can join the new character to it.
    // Round both positions forward so the next edit never starts inside a cluster.
    editor.select(*std::lower_bound(b.begin(),b.end(),editor.anchor()),*std::lower_bound(b.begin(),b.end(),editor.caret()));
}
void editor_move_visual(TextEdit& editor,int direction,bool extend){
    auto s=surface(1,1);Context c(cairo_create(s.get()),cairo_destroy);check(cairo_status(c.get()));
    ShapedText shaped(c.get(),editor.text(),16);auto at=editor.caret();
    if(!extend&&editor.has_selection()){
        const auto a=editor.anchor(),b=editor.caret();const bool a_left=shaped.caret(a).x<shaped.caret(b).x;
        at=((direction<0)==a_left)?a:b;
    }else at=shaped.move_visual(at,direction);
    editor.move_to(at,extend);
}
void editor_delete_grapheme(TextEdit& editor,bool backward){
    auto s=surface(1,1);Context c(cairo_create(s.get()),cairo_destroy);check(cairo_status(c.get()));
    ShapedText shaped(c.get(),editor.text(),16);
    if(editor.has_selection())editor.insert("");
    else if(backward)editor.erase_range(shaped.previous_grapheme(editor.caret()),editor.caret());
    else editor.erase_range(editor.caret(),shaped.next_grapheme(editor.caret()));
}
void write_layout_png(const PixelFrame&frame,const std::string&path) {
    if(frame.width<1 || frame.height<1 || frame.width>4096 || frame.height>4096 || frame.stride<frame.width*4 || frame.pixels.size()!=static_cast<std::size_t>(frame.stride)*frame.height)
        throw std::invalid_argument("invalid frame buffer");
    auto target=surface(frame.width,frame.height);cairo_surface_flush(target.get());
    auto*data=cairo_image_surface_get_data(target.get());const int stride=cairo_image_surface_get_stride(target.get());
    for(int y=0;y<frame.height;++y)std::memcpy(data+y*stride,frame.pixels.data()+y*frame.stride,frame.width*4);
    cairo_surface_mark_dirty(target.get());check(cairo_surface_write_to_png(target.get(),path.c_str()));
}
} // namespace explicit_ui
