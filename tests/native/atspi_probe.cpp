// Independent process: no provider/model headers or application internals.
#include <atspi/atspi.h>
#include <chrono>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <map>
#include <cstdlib>
namespace {
std::string name(AtspiAccessible* a){char* v=atspi_accessible_get_name(a,nullptr);std::string s=v?v:"";g_free(v);return s;}
AtspiAccessible* find(AtspiAccessible* parent,const std::function<bool(AtspiAccessible*)>& match){
    if(match(parent))return ATSPI_ACCESSIBLE(g_object_ref(parent));
    int n=atspi_accessible_get_child_count(parent,nullptr);
    for(int i=0;i<n;++i){auto* child=atspi_accessible_get_child_at_index(parent,i,nullptr);if(!child)continue;auto* found=find(child,match);g_object_unref(child);if(found)return found;}
    return nullptr;
}
void pump(){while(g_main_context_iteration(nullptr,FALSE)){}std::this_thread::sleep_for(std::chrono::milliseconds(20));}
void require(bool p,const char* why){if(!p)throw std::runtime_error(why);}
void until(const std::function<bool()>& check,const char* why){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(8);do{pump();if(check())return;}while(std::chrono::steady_clock::now()<end);throw std::runtime_error(why);}
void dump(AtspiAccessible* a,int depth=0){std::cout<<std::string(depth*2,' ')<<atspi_accessible_get_role(a,nullptr)<<" "<<name(a)<<'\n';int n=atspi_accessible_get_child_count(a,nullptr);for(int i=0;i<n;++i){auto* c=atspi_accessible_get_child_at_index(a,i,nullptr);if(c){dump(c,depth+1);g_object_unref(c);}}}
std::string text(AtspiAccessible* a){auto* t=atspi_accessible_get_text_iface(a);require(t,"entry has no Text interface");char* s=atspi_text_get_text(t,0,-1,nullptr);std::string result=s?s:"";g_free(s);g_object_unref(t);return result;}
}
int main(int argc,char** argv){try{
    require(atspi_init()==0,"AT-SPI initialization failed");
    auto* desktop=atspi_get_desktop(0);require(desktop,"no AT-SPI desktop");AtspiAccessible* app=nullptr;
    until([&]{app=find(desktop,[](AtspiAccessible* a){return name(a)=="Explicit UI";});return app!=nullptr;},"application was not discovered");
    dump(app);
    if(argc>1&&std::string(argv[1])=="--dump"){g_object_unref(app);g_object_unref(desktop);atspi_exit();return 0;}
    struct Observed {std::map<std::string,int> counts;bool removed_text=false;} observed;
    auto* listener=atspi_event_listener_new([](AtspiEvent* event,void* user){auto& out=*static_cast<Observed*>(user);++out.counts[event->type];if(std::string(event->type).find("text-changed:delete")!=std::string::npos&&G_VALUE_HOLDS_STRING(&event->any_data)){const char* text=g_value_get_string(&event->any_data);out.removed_text|=text&&std::string(text)=="00007";}g_boxed_free(ATSPI_TYPE_EVENT,event);},&observed,nullptr);
    for(const char* type:{"object:state-changed:focused","object:text-changed","object:text-caret-moved","object:children-changed"})require(atspi_event_listener_register(listener,type,nullptr),"event registration failed");
    auto* frame=find(app,[](AtspiAccessible* a){return atspi_accessible_get_role(a,nullptr)==ATSPI_ROLE_FRAME;});require(frame,"no frame");
    auto* button=find(app,[](AtspiAccessible* a){return name(a)=="+ 1";});require(button,"no increment button");
    auto* label=find(app,[](AtspiAccessible* a){return name(a).find("Count:")==0;});require(label,"no counter label");
    auto before=name(label);auto* action=atspi_accessible_get_action_iface(button);require(action,"no Action interface");require(atspi_action_do_action(action,0,nullptr),"action not accepted");
    until([&]{atspi_accessible_clear_cache(label);return name(label)!=before;},"counter action did not commit");std::cout<<"ACTION "<<before<<" -> "<<name(label)<<'\n';
    auto* entry=find(app,[](AtspiAccessible* a){return atspi_accessible_get_role(a,nullptr)==ATSPI_ROLE_ENTRY;});require(entry,"no entry");
    auto* component=atspi_accessible_get_component_iface(entry);require(component,"no Component interface");require(atspi_component_grab_focus(component,nullptr),"focus not queued");
    until([&]{atspi_accessible_clear_cache(entry);auto* states=atspi_accessible_get_state_set(entry);bool focused=atspi_state_set_contains(states,ATSPI_STATE_FOCUSED);g_object_unref(states);return focused;},"focus did not commit");
    auto* edit=atspi_accessible_get_editable_text_iface(entry);require(edit,"no EditableText interface");require(atspi_editable_text_set_text_contents(edit,"00007",nullptr),"replace not accepted");until([&]{return text(entry)=="00007";},"text did not commit");
    auto* list=find(app,[](AtspiAccessible* a){return atspi_accessible_get_role(a,nullptr)==ATSPI_ROLE_LIST;});require(list,"no list");
    until([&]{atspi_accessible_clear_cache(list);return atspi_accessible_get_child_count(list,nullptr)==1;},"filtered list did not update");
    auto* row=atspi_accessible_get_child_at_index(list,0,nullptr);require(row&&name(row).find("00007")!=std::string::npos,"wrong filtered row");
    require(atspi_editable_text_set_text_contents(edit,"中文🙂",nullptr),"Unicode replace not accepted");until([&]{return text(entry)=="中文🙂";},"Unicode value mismatch");auto* ti=atspi_accessible_get_text_iface(entry);require(atspi_text_get_character_count(ti,nullptr)==3,"UTF-8 character count mismatch");
    require(atspi_text_get_caret_offset(ti,nullptr)==3,"Unicode caret mismatch");
    until([&]{atspi_accessible_clear_cache(row);auto* states=atspi_accessible_get_state_set(row);bool dead=atspi_state_set_contains(states,ATSPI_STATE_DEFUNCT);g_object_unref(states);return dead;},"removed retained row was not defunct");
    atspi_editable_text_set_text_contents(edit,"bad\nline",nullptr);
    // Barrier: another ordered D-Bus text request plus a short test-only observation.
    auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(300);while(std::chrono::steady_clock::now()<end)pump();require(text(entry)=="中文🙂","invalid edit mutated committed text");
    auto replace=[&](const std::string& value){require(atspi_editable_text_set_text_contents(edit,value.c_str(),nullptr),"geometry edit rejected");until([&]{return text(entry)==value;},"geometry edit did not commit");};
    auto* fc=atspi_accessible_get_component_iface(frame);auto* fr=atspi_component_get_extents(fc,ATSPI_COORD_TYPE_SCREEN,nullptr);require(fr,"missing frame extents");
    if(const char* expected=std::getenv("EUI_EXPECT_CLIENT_X"))require(fr->x==std::stoi(expected),"AT-SPI frame x differs from X11 client origin");
    if(const char* expected=std::getenv("EUI_EXPECT_CLIENT_Y"))require(fr->y==std::stoi(expected),"AT-SPI frame y differs from X11 client origin");
    std::cout<<"CLIENT_ORIGIN "<<fr->x<<" "<<fr->y<<'\n';
    auto rect=[&](int offset){auto* r=atspi_text_get_character_extents(ti,offset,ATSPI_COORD_TYPE_SCREEN,nullptr);require(r&&r->width>=0&&r->height>0,"missing character geometry");return r;};
    auto check_rect=[&](int offset,int expected_hit){auto* r=rect(offset);auto* local=atspi_text_get_character_extents(ti,offset,ATSPI_COORD_TYPE_WINDOW,nullptr);
        require(local&&r->x-local->x==fr->x&&r->y-local->y==fr->y,"screen/window text transform mismatch");
        int hit=atspi_text_get_offset_at_point(ti,r->x+r->width/2,r->y+r->height/2,ATSPI_COORD_TYPE_SCREEN,nullptr);
        require(hit==expected_hit,"character point maps to wrong Unicode offset");
        require(atspi_text_get_offset_at_point(ti,local->x+local->width/2,local->y+local->height/2,ATSPI_COORD_TYPE_WINDOW,nullptr)==expected_hit,"window point maps to wrong offset");
        std::cout<<"CHAR "<<offset<<" RECT "<<r->x<<" "<<r->y<<" "<<r->width<<" "<<r->height<<" HIT "<<hit<<'\n';g_free(local);g_free(r);};
    for(int i=0;i<3;++i)check_rect(i,i);
    auto range=[&](int offset,AtspiTextGranularity granularity,int start,int end,const std::string& expected){auto* r=atspi_text_get_string_at_offset(ti,offset,granularity,nullptr);
        require(r&&r->start_offset==start&&r->end_offset==end&&r->content&&expected==r->content,"text granularity range mismatch");g_boxed_free(ATSPI_TYPE_TEXT_RANGE,r);};
    replace("one  two");range(4,ATSPI_TEXT_GRANULARITY_WORD,0,5,"one  ");range(5,ATSPI_TEXT_GRANULARITY_WORD,5,8,"two");range(7,ATSPI_TEXT_GRANULARITY_CHAR,7,8,"o");range(4,ATSPI_TEXT_GRANULARITY_LINE,0,8,"one  two");range(4,ATSPI_TEXT_GRANULARITY_PARAGRAPH,0,8,"one  two");
    replace("é 👩‍💻");require(atspi_text_get_character_count(ti,nullptr)==6,"cluster character count wrong");range(1,ATSPI_TEXT_GRANULARITY_CHAR,1,2,"́");check_rect(0,0);check_rect(3,3);
    replace("אבג");for(int i=0;i<3;++i)check_rect(i,i);auto* r0=rect(0);auto* r2=rect(2);require(r0->x>r2->x,"RTL visual ordering lost");g_free(r0);g_free(r2);
    replace("abc אבג xyz");for(int i=0;i<11;++i)check_rect(i,i);
    replace(std::string(200,'W'));auto* er=atspi_component_get_extents(component,ATSPI_COORD_TYPE_SCREEN,nullptr);r0=rect(0);auto* last=rect(199);
    require(r0->x<er->x&&last->x>=er->x&&last->x+last->width<=er->x+er->width,"horizontal scroll geometry diverges from editor");
    require(atspi_text_get_offset_at_point(ti,er->x,er->y,ATSPI_COORD_TYPE_SCREEN,nullptr)==-1,"outside clipped text accepted");g_free(r0);g_free(last);
    auto* bc=atspi_accessible_get_component_iface(button);require(atspi_component_grab_focus(bc,nullptr),"button focus not queued");
    until([&]{atspi_accessible_clear_cache(button);auto* states=atspi_accessible_get_state_set(button);bool focused=atspi_state_set_contains(states,ATSPI_STATE_FOCUSED);g_object_unref(states);return focused;},"button focus did not commit");
    r0=rect(0);require(r0->x==er->x+18,"unfocused renderer origin mismatch");g_free(r0);g_free(er);g_free(fr);g_object_unref(fc);g_object_unref(bc);
    require(atspi_component_grab_focus(component,nullptr),"entry focus restore failed");replace("中文🙂");
    std::cout<<"GEOMETRY_PASS Unicode offsets Pango clusters RTL scroll focus word char line paragraph screen/window coordinates\n";
    bool focus_event=false,text_event=false,caret_event=false,child_event=false;
    for(const auto& event:observed.counts){std::cout<<"EVENT "<<event.first<<" "<<event.second<<'\n';focus_event|=event.first.find("state-changed:focused")!=std::string::npos;text_event|=event.first.find("text-changed")!=std::string::npos;caret_event|=event.first.find("text-caret-moved")!=std::string::npos;child_event|=event.first.find("children-changed")!=std::string::npos;}
    require(focus_event&&text_event&&caret_event&&child_event,"required accessibility event missing");
    require(observed.removed_text,"removed-text event payload mismatch");
    std::cout<<"PASS discovery roles action main-thread commit focus UTF-8 text caret rendered-list invalid-edit events retained-row-defunct\n"<<std::flush;
    for(const char* type:{"object:state-changed:focused","object:text-changed","object:text-caret-moved","object:children-changed"})atspi_event_listener_deregister(listener,type,nullptr);
    g_object_unref(listener);
    g_object_unref(ti);g_object_unref(edit);g_object_unref(component);
    for(auto* o:{row,list,entry,label,button,frame,app,desktop})g_object_unref(o);
    g_object_unref(action);atspi_exit();return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
