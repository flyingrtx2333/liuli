// White-box unit test runs real ATK objects synchronously without a session bus.
// Including the implementation keeps test-only inspection out of the public API.
#include "../../native/linux_accessibility.cpp"
#include <iostream>
#include "../../native/display_text.h"
void require(bool p,const char* why){if(!p)throw std::runtime_error(why);}
int main(){try{
    require(explicit_ui::accessible_widget_id("window")!=explicit_ui::accessible_frame_id(),"frame/widget ID collision");
    require(explicit_ui::accessible_widget_id("list/row/0/foo")!=explicit_ui::accessible_row_id("list",0,"foo"),"row/widget ID collision");
    require(explicit_ui::accessible_row_id("a/1",2,"b")!=explicit_ui::accessible_row_id("a",1,"2/b"),"row ID components are ambiguous");
    require(explicit_ui::display_text(std::string("a\0b\n",4))=="a b ","semantic/display sanitizer mismatch");
    for(const char* v:{"2.62.1","2.62.10","2.64.0","3.0.0"})require(supported_atk_version(v),"supported version rejected");
    for(const char* v:{"2.56.2","2.62.0","2.9.99","","2.62","2.62.1.0","2.62.1-custom","-2.62.1","2.-62.1","9999999999999999.0.0"," 2.62.1"})require(!supported_atk_version(v),"old/malformed version accepted");
    int coordinate=0;require(pixel_coordinate(2147483647LL,coordinate)&&coordinate==2147483647,"coordinate maximum rejected");require(!pixel_coordinate(2147483648LL,coordinate)&&!pixel_coordinate(-2147483649LL,coordinate),"coordinate overflow accepted");
    require(!supported_atk_version(nullptr),"unknown version accepted");
    ProviderLease first;bool duplicate=false;try{ProviderLease second;}catch(const std::runtime_error&){duplicate=true;}require(duplicate,"singleton did not reject duplicate");require(provider_claimed.load(),"duplicate released original lease");
    root=ATK_OBJECT(g_object_new(eui_accessible_get_type(),nullptr));
    std::map<std::string,AtkObject*> nodes;int requests=0;std::string last_target;
    auto enqueue=[&](AccessibilityRequest request){++requests;last_target=request.id;return true;};
    AccessibleNode frame;frame.id="a";frame.role=AccessibleNode::Role::frame;
    AccessibleNode list;list.id="b";list.parent="a";list.role=AccessibleNode::Role::list;
    AccessibleNode row;row.id="c";row.parent="b";row.role=AccessibleNode::Role::row;row.name="child";
    std::vector<AccessibleNode> snapshot{frame,list,row};validate_snapshot(snapshot);apply_nodes(nodes,snapshot,enqueue);
    auto* held=ATK_OBJECT(g_object_ref(nodes.at("c")));auto* parent=ATK_OBJECT(g_object_ref(nodes.at("a")));
    apply_nodes(nodes,{},enqueue);require(nodes.empty(),"nodes not removed");
    auto* states=atk_object_ref_state_set(held);require(atk_state_set_contains_state(states,ATK_STATE_DEFUNCT),"retained descendant not defunct");g_object_unref(states);
    require(!atk_object_get_parent(held)&&atk_object_get_n_accessible_children(parent)==0,"removed subtree retained parent links");g_object_unref(held);g_object_unref(parent);
    AccessibleNode second=row;second.id="d";snapshot={frame,list,row,second};apply_nodes(nodes,snapshot,enqueue);
    snapshot={frame,list,second,row};apply_nodes(nodes,snapshot,enqueue);auto* child=atk_object_ref_accessible_child(nodes.at("b"),0);require(child==nodes.at("d"),"child reorder failed");g_object_unref(child);
    auto invalid=snapshot;invalid.back().parent="missing";bool bad=false;try{validate_snapshot(invalid);}catch(const std::invalid_argument&){bad=true;}require(bad,"invalid parent accepted");
    invalid=snapshot;invalid.push_back(row);bad=false;try{validate_snapshot(invalid);}catch(const std::invalid_argument&){bad=true;}require(bad,"duplicate ID accepted");
    AccessibleNode button;button.id="button";button.parent="a";button.role=AccessibleNode::Role::button;button.target="window";button.width=200;button.height=60;
    AccessibleNode entry;entry.id="entry";entry.parent="a";entry.role=AccessibleNode::Role::entry;
    snapshot.push_back(button);snapshot.push_back(entry);apply_nodes(nodes,snapshot,enqueue);
    require(atk_action_do_action(ATK_ACTION(nodes.at("button")),0),"button callback rejected");require(requests==1&&last_target=="window","button action target not preserved");
    int enabled_events=0,sensitive_events=0,focusable_events=0,showing_events=0;
    struct Signals {int* enabled;int* sensitive;int* focusable;int* showing;} signals{&enabled_events,&sensitive_events,&focusable_events,&showing_events};
    g_signal_connect(nodes.at("button"),"state-change",G_CALLBACK(+[](AtkObject*,const char* name,gboolean,gpointer ptr){
        auto& counts=*static_cast<Signals*>(ptr);
        if(std::string(name)=="enabled")++*counts.enabled;
        if(std::string(name)=="sensitive")++*counts.sensitive;
        if(std::string(name)=="focusable")++*counts.focusable;
        if(std::string(name)=="showing")++*counts.showing;
    }),&signals);
    snapshot[snapshot.size()-2].enabled=false;apply_nodes(nodes,snapshot,enqueue);
    states=atk_object_ref_state_set(nodes.at("button"));
    for(auto flag:{ATK_STATE_ENABLED,ATK_STATE_SENSITIVE,ATK_STATE_FOCUSABLE})require(!atk_state_set_contains_state(states,flag),"disabled button exposed active state");
    require(atk_state_set_contains_state(states,ATK_STATE_VISIBLE),"disabled button disappeared");g_object_unref(states);
    require(!atk_action_do_action(ATK_ACTION(nodes.at("button")),0),"disabled action accepted");
    require(!atk_component_grab_focus(ATK_COMPONENT(nodes.at("button"))),"disabled focus accepted");
    require(requests==1&&enabled_events==1&&sensitive_events==1&&focusable_events==1,"disabled state notifications missing");
    apply_nodes(nodes,snapshot,enqueue);require(enabled_events==1,"unchanged enabled emitted an event");
    snapshot[snapshot.size()-2].enabled=true;apply_nodes(nodes,snapshot,enqueue);
    states=atk_object_ref_state_set(nodes.at("button"));
    for(auto flag:{ATK_STATE_ENABLED,ATK_STATE_SENSITIVE,ATK_STATE_FOCUSABLE})require(atk_state_set_contains_state(states,flag),"reenabled state missing");
    g_object_unref(states);require(enabled_events==2&&sensitive_events==2&&focusable_events==2,"reenabled notifications missing");
    // Collapsing geometry removes interaction, not the semantic enabled state.
    snapshot[snapshot.size()-2].width=0;apply_nodes(nodes,snapshot,enqueue);
    states=atk_object_ref_state_set(nodes.at("button"));
    require(atk_state_set_contains_state(states,ATK_STATE_ENABLED)&&atk_state_set_contains_state(states,ATK_STATE_SENSITIVE),"collapse changed semantic enabled");
    require(!atk_state_set_contains_state(states,ATK_STATE_FOCUSABLE)&&!atk_state_set_contains_state(states,ATK_STATE_SHOWING),"collapsed button remains interactive/showing");
    g_object_unref(states);
    require(!atk_action_do_action(ATK_ACTION(nodes.at("button")),0)&&!atk_component_grab_focus(ATK_COMPONENT(nodes.at("button"))),"collapsed action/focus accepted");
    require(requests==1&&enabled_events==2&&sensitive_events==2&&focusable_events==3&&showing_events==1,"collapse state events inconsistent");
    snapshot[snapshot.size()-2].width=200;apply_nodes(nodes,snapshot,enqueue);
    require(focusable_events==4&&enabled_events==2&&showing_events==2,"restored area state events inconsistent");
    auto* edit=ATK_EDITABLE_TEXT(nodes.at("entry"));atk_editable_text_set_text_contents(edit,std::string(257,'x').c_str());require(requests==1,"oversized replacement queued");atk_editable_text_set_text_contents(edit,"中文🙂");require(requests==2,"valid replacement not queued");
    entry.text="one  two";entry.text_geometry.available=true;entry.text_geometry.word_starts={0,5,8};
    entry.text_geometry.characters.resize(8,{18,12,10,20});entry.text_geometry.hits={{18,28,0},{28,38,1}};entry.text_geometry.hit_top=12;entry.text_geometry.hit_bottom=32;
    frame.x=100;frame.y=200;entry.x=150;entry.y=260;snapshot={frame,entry};validate_snapshot(snapshot);apply_nodes(nodes,snapshot,enqueue);
    auto* tx=ATK_TEXT(nodes.at("entry"));int x=0,y=0,w=0,h=0;
    atk_text_get_character_extents(tx,0,&x,&y,&w,&h,ATK_XY_SCREEN);require(x==168&&y==272&&w==10&&h==20,"screen text extents wrong");
    atk_text_get_character_extents(tx,0,&x,&y,&w,&h,ATK_XY_WINDOW);require(x==68&&y==72,"window text extents wrong");
    require(atk_text_get_offset_at_point(tx,170,275,ATK_XY_SCREEN)==0&&atk_text_get_offset_at_point(tx,80,75,ATK_XY_WINDOW)==1,"text hit coordinate transform wrong");
    require(atk_text_get_offset_at_point(tx,170,232,ATK_XY_SCREEN)==-1,"outside text hit accepted");
    int a=0,b=0;char* part=atk_text_get_string_at_offset(tx,4,ATK_TEXT_GRANULARITY_WORD,&a,&b);require(part&&std::string(part)=="one  "&&a==0&&b==5,"Pango word-start range wrong");g_free(part);
    part=atk_text_get_string_at_offset(tx,5,ATK_TEXT_GRANULARITY_CHAR,&a,&b);require(part&&std::string(part)=="t"&&a==5&&b==6,"character range wrong");g_free(part);
    part=atk_text_get_string_at_offset(tx,7,ATK_TEXT_GRANULARITY_LINE,&a,&b);require(part&&std::string(part)==entry.text&&a==0&&b==8,"line range wrong");g_free(part);
    part=atk_text_get_string_at_offset(tx,8,ATK_TEXT_GRANULARITY_CHAR,&a,&b);require(!part&&a==-1&&b==-1,"invalid offset accepted");
    part=atk_text_get_string_at_offset(tx,0,ATK_TEXT_GRANULARITY_SENTENCE,&a,&b);require(!part&&a==-1&&b==-1,"unsupported granularity not explicit");
    auto malformed=snapshot;malformed.back().text_geometry.hits.push_back({20,21,0});bad=false;try{validate_snapshot(malformed);}catch(const std::invalid_argument&){bad=true;}require(bad,"overlapping hit spans accepted");
    malformed=snapshot;malformed.back().text_geometry.characters.resize(257);bad=false;try{validate_snapshot(malformed);}catch(const std::length_error&){bad=true;}require(bad,"unbounded geometry accepted");
    malformed=snapshot;malformed.back().text_geometry.word_starts={0,8,5};bad=false;try{validate_snapshot(malformed);}catch(const std::invalid_argument&){bad=true;}require(bad,"unsorted word boundaries accepted");
    auto huge=entry;huge.text_geometry.hits.clear();for(int i=0;i<4096;++i)huge.text_geometry.hits.push_back({i,i+1,0});
    std::vector<AccessibleNode> oversized{frame};for(int i=0;i<30;++i){huge.id="bulk"+std::to_string(i);oversized.push_back(huge);}
    bad=false;try{validate_snapshot(oversized);}catch(const std::length_error&){bad=true;}require(bad,"snapshot geometry byte budget not enforced");
    entry.text_geometry={};snapshot={frame,entry};apply_nodes(nodes,snapshot,enqueue);
    atk_text_get_character_extents(tx,0,&x,&y,&w,&h,ATK_XY_SCREEN);require(x==0&&y==0&&w==0&&h==0,"unavailable geometry returned stale rectangles");
    require(atk_text_get_offset_at_point(tx,170,275,ATK_XY_SCREEN)==-1,"unavailable geometry returned stale hit");
    apply_nodes(nodes,{},enqueue);g_object_unref(root);root=nullptr;
    std::cout<<"PASS singleton lease; subtree removal retained references; child order; malformed snapshots; bounded edit; action queue\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
