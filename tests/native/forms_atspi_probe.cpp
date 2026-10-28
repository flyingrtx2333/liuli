// Independent AT-SPI client. --edit deliberately changes only the discovered form's fields.
#include <atspi/atspi.h>
#include <chrono>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
namespace {
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
std::string name(AtspiAccessible* a){char* s=atspi_accessible_get_name(a,nullptr);std::string out=s?s:"";g_free(s);return out;}
AtspiAccessible* find(AtspiAccessible* root,const std::function<bool(AtspiAccessible*)>& match){
    if(match(root))return ATSPI_ACCESSIBLE(g_object_ref(root));
    int count=atspi_accessible_get_child_count(root,nullptr);
    for(int i=0;i<count;++i){auto* child=atspi_accessible_get_child_at_index(root,i,nullptr);if(!child)continue;auto* found=find(child,match);g_object_unref(child);if(found)return found;}
    return nullptr;
}
void until(const std::function<bool()>& test,const char* why){
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(8);
    do{while(g_main_context_iteration(nullptr,FALSE)){}if(test())return;std::this_thread::sleep_for(std::chrono::milliseconds(20));}while(std::chrono::steady_clock::now()<deadline);
    throw std::runtime_error(why);
}
std::string value(AtspiText* text){char* s=atspi_text_get_text(text,0,-1,nullptr);std::string out=s?s:"";g_free(s);return out;}
bool focused(AtspiAccessible* node){atspi_accessible_clear_cache(node);auto* states=atspi_accessible_get_state_set(node);bool out=atspi_state_set_contains(states,ATSPI_STATE_FOCUSED);g_object_unref(states);return out;}
struct Field {
    AtspiAccessible* node;AtspiText* text;AtspiComponent* component;AtspiEditableText* edit;
    explicit Field(AtspiAccessible* n):node(n),text(atspi_accessible_get_text_iface(n)),component(atspi_accessible_get_component_iface(n)),edit(atspi_accessible_get_editable_text_iface(n)){require(text&&component&&edit,"field interface missing");}
    ~Field(){g_object_unref(edit);g_object_unref(component);g_object_unref(text);g_object_unref(node);}
    void replace(const std::string& s){require(atspi_editable_text_set_text_contents(edit,s.c_str(),nullptr),"replace not queued");until([&]{return value(text)==s;},"replace did not commit");}
    void focus(){require(atspi_component_grab_focus(component,nullptr),"focus not queued");until([&]{return focused(node);},"focus did not commit");}
    void select(int a,int b){
        // This bounded provider supports set_selection(index 0), not add_selection.
        require(atspi_text_set_selection(text,0,a,b,nullptr),"selection not queued");
        until([&]{if(atspi_text_get_n_selections(text,nullptr)!=1)return false;auto* range=atspi_text_get_selection(text,0,nullptr);bool good=range&&range->start_offset==a&&range->end_offset==b;if(range)g_boxed_free(ATSPI_TYPE_RANGE,range);return good&&atspi_text_get_caret_offset(text,nullptr)==b;},"selection/caret did not commit");
    }
    void check_selection(int a,int b){auto* range=atspi_text_get_selection(text,0,nullptr);require(range&&range->start_offset==a&&range->end_offset==b,"another field overwrote selection");g_boxed_free(ATSPI_TYPE_RANGE,range);require(atspi_text_get_caret_offset(text,nullptr)==b,"another field overwrote caret");}
    int geometry(){
        const auto current=value(text);const int count=atspi_text_get_character_count(text,nullptr);require(count>0,"geometry requires nonempty field");
        auto* bounds=atspi_component_get_extents(component,ATSPI_COORD_TYPE_SCREEN,nullptr);require(bounds&&bounds->width>0&&bounds->height>0,"field extents unavailable");
        const int field_y=bounds->y;
        std::cout<<"FIELD "<<name(node)<<" TEXT "<<current<<" COUNT "<<count<<" CARET "<<atspi_text_get_caret_offset(text,nullptr)<<" ORIGIN "<<bounds->x<<","<<bounds->y<<'\n';
        for(int i=0;i<count;++i){
            auto* r=atspi_text_get_character_extents(text,i,ATSPI_COORD_TYPE_SCREEN,nullptr);require(r&&r->width>0&&r->height>0,"character extents unavailable");
            const int x=r->x+r->width/2,y=r->y+r->height/2;
            if(x>=bounds->x+14&&x<bounds->x+bounds->width-14){
                const int hit=atspi_text_get_offset_at_point(text,x,y,ATSPI_COORD_TYPE_SCREEN,nullptr);
                require(hit==i,"character point mapped to wrong field/offset");
                std::cout<<"CHAR "<<name(node)<<" "<<i<<" RECT "<<r->x<<","<<r->y<<","<<r->width<<","<<r->height<<" HIT "<<hit<<'\n';
            }
            require(y>=bounds->y&&y<bounds->y+bounds->height,"character located outside its field");g_free(r);
        }
        require(atspi_text_get_offset_at_point(text,bounds->x,bounds->y,ATSPI_COORD_TYPE_SCREEN,nullptr)==-1,"outside text clip accepted");g_free(bounds);return field_y;
    }
};
}
int main(int argc,char** argv){try{
    require(argc==1||(argc==2&&std::string(argv[1])=="--edit"),"usage: forms-atspi-probe [--edit]");
    require(atspi_init()==0,"AT-SPI initialization failed");auto* desktop=atspi_get_desktop(0);require(desktop,"no desktop");AtspiAccessible* app=nullptr;
    until([&]{app=find(desktop,[](AtspiAccessible* a){return name(a)=="Explicit UI";});return app!=nullptr;},"Explicit UI not found");
    auto entry=[&](const std::string& label){auto* node=find(app,[&](AtspiAccessible* a){return atspi_accessible_get_role(a,nullptr)==ATSPI_ROLE_ENTRY&&name(a)==label;});require(node,"named form entry missing");return node;};
    {
        Field first(entry("Name")),second(entry("Project"));
        if(argc==2){
            first.focus();first.replace("名🙂A");require(atspi_text_get_character_count(first.text,nullptr)==3,"name Unicode count wrong");first.select(0,1);
            second.focus();second.replace("项目é");require(atspi_text_get_character_count(second.text,nullptr)==3,"project Unicode count wrong");second.select(1,2);
            require(value(first.text)=="名🙂A"&&value(second.text)=="项目é","field values not independent");first.check_selection(0,1);second.check_selection(1,2);
            require(!focused(first.node)&&focused(second.node),"focus not isolated");
        }
        const int first_y=first.geometry(),second_y=second.geometry();require(first_y!=second_y,"fields share erroneous geometry origin");
        if(argc==2){first.check_selection(0,1);second.check_selection(1,2);}
    }
    g_object_unref(app);g_object_unref(desktop);atspi_exit();std::cout<<"FORMS_ATSPI_PASS distinct fields text geometry point mapping"<<(argc==2?" Unicode edits caret selection focus":" read-only")<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"FORMS_ATSPI_FAIL "<<e.what()<<'\n';return 1;}}
