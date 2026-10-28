// Native headless snapshot renderer. No browser, display server, or UI event loop.
#include "cairo_abi.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
struct Snapshot {
    std::string label, counter;
    std::vector<std::string> rows;
    long long patches, materialized, evaluations;
};
#include "preview_data.inc"
struct Color { double r,g,b; };
constexpr Color bg{.043,.055,.079}, panel{.075,.09,.122}, ink{.91,.94,.98}, muted{.48,.55,.65}, accent{.48,.86,.69};
void color(cairo_t* c, Color a){cairo_set_source_rgb(c,a.r,a.g,a.b);}
void box(cairo_t*c,double x,double y,double w,double h,Color a,double r=12){
    color(c,a); constexpr double pi=3.141592653589793;
    cairo_move_to(c,x+r,y);cairo_line_to(c,x+w-r,y);cairo_arc(c,x+w-r,y+r,r,-pi/2,0);
    cairo_arc(c,x+w-r,y+h-r,r,0,pi/2);cairo_arc(c,x+r,y+h-r,r,pi/2,pi);
    cairo_arc(c,x+r,y+r,r,pi,pi*1.5);cairo_close_path(c);cairo_fill(c);
}
void text(cairo_t*c,double x,double y,const std::string&s,double size=15,Color a=ink,bool bold=false,double width=900){
    cairo_save(c);cairo_rectangle(c,x,y-size-3,width,size+10);cairo_clip(c);
    color(c,a);cairo_select_font_face(c,"sans",CAIRO_FONT_SLANT_NORMAL,bold?CAIRO_FONT_WEIGHT_BOLD:CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(c,size);cairo_move_to(c,x,y);cairo_show_text(c,s.c_str());cairo_restore(c);
}
void stat(cairo_t*c,double x,const std::string&label,const std::string&value){
    box(c,x,181,274,112,panel);text(c,x+20,211,label,12,muted);text(c,x+20,263,value,30,ink,true,234);
}
void check(cairo_status_t s){if(s!=CAIRO_STATUS_SUCCESS)throw std::runtime_error(cairo_status_to_string(s));}
void render(const Snapshot&s,const std::filesystem::path&path){
    auto*surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,1200,900);check(cairo_surface_status(surface));
    auto*c=cairo_create(surface);
    color(c,bg);cairo_paint(c);
    box(c,24,24,224,852,panel,18);
    box(c,46,48,32,32,accent,9);text(c,56,71,"E",19,bg,true);
    text(c,90,70,"EXPLICIT",18,ink,true);
    text(c,46,126,"WORKSPACE",10,muted,true);
    box(c,38,148,196,44,{.12,.20,.20},9);text(c,55,176,"State inspector",14,accent,true);
    text(c,46,235,"RENDER SETTINGS",10,muted,true);
    text(c,46,275,"Backend",12,muted);text(c,46,300,"Cairo / CPU",15);
    text(c,46,348,"Surface",12,muted);text(c,46,373,"ARGB32",15);
    text(c,46,421,"Output",12,muted);text(c,46,446,"PNG snapshot",15);
    text(c,46,796,"HEADLESS PREVIEW",10,accent,true);
    text(c,46,822,"No live window",12,muted);
    text(c,46,843,"No event loop",12,muted);
    text(c,280,63,"NATIVE RUNTIME  /  STATE SNAPSHOT",11,accent,true);
    text(c,280,108,"Small updates. Explicit state.",31,ink,true);
    text(c,280,142,"Runtime checkpoint: " + s.label,14,muted,false,860);
    stat(c,280,"COUNTER VIEW",s.counter);
    stat(c,574,"VIEW PATCHES / CUMULATIVE",std::to_string(s.patches));
    stat(c,868,"ROWS MATERIALIZED / CUMULATIVE",std::to_string(s.materialized));
    box(c,280,315,862,506,panel,16);
    text(c,304,351,"Visible items",18,ink,true);
    text(c,304,377,std::to_string(s.rows.size())+" rows in runtime view  /  "+std::to_string(s.evaluations)+" total derivation evaluations",12,muted);
    const auto n=std::min<std::size_t>(12,s.rows.size());
    for(std::size_t i=0;i<n;i++){
        const double y=397+i*33;
        if(i%2==0)box(c,296,y,830,31,{.086,.105,.14},5);
        text(c,311,y+21,std::to_string(i+1),11,muted);
        text(c,356,y+22,s.rows[i],14,ink,false,740);
    }
    if(s.rows.empty())text(c,304,433,"No visible items",15,muted);
    if(s.rows.size()>12)text(c,304,810,"Preview clipped to 12 rows; full data retained in snapshots.jsonl",10,muted);
    text(c,280,853,"Compiled state output  ->  captured snapshot  ->  native Cairo pixels",12,muted);
    check(cairo_status(c));check(cairo_surface_write_to_png(surface,path.string().c_str()));
    cairo_destroy(c);cairo_surface_destroy(surface);
}
int main(int argc,char**argv){
    try{
        if(argc!=2)throw std::runtime_error("usage: cairo-preview OUTPUT_DIRECTORY");
        std::filesystem::create_directories(argv[1]);
        render(preview_snapshots.at(0),std::filesystem::path(argv[1])/"before.png");
        render(preview_snapshots.at(1),std::filesystem::path(argv[1])/"after.png");
        std::cout<<"Rendered two 1200x900 native Cairo PNG snapshots\n";
    }catch(const std::exception&e){std::cerr<<"preview error: "<<e.what()<<'\n';return 1;}
}
