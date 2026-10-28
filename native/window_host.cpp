#include "model.h"
#include "scene.h"
#include "glass_renderer.h"
#include "sdl_abi.h"
#include "text_edit.h"
#include "app_state.h"
#include "async_state_store.h"
#ifdef EUI_LINUX_ACCESSIBILITY
#include "linux_accessibility.h"
#include "display_text.h"
#include <mutex>
#include <deque>
#endif
#include <atomic>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <map>
#include <type_traits>
#include <stdexcept>

namespace {
using namespace explicit_ui;
void check(int result) { if(result<0) throw std::runtime_error(SDL_GetError()); }
struct Sdl { Sdl(){
    // CPU Cairo already produced the frame; avoid loading a 3D texture pipeline
    // merely to copy it. Explicit environment overrides still take precedence.
    SDL_SetHint("SDL_FRAMEBUFFER_ACCELERATION","0");check(SDL_Init(SDL_INIT_VIDEO));} ~Sdl(){SDL_Quit();} };
using Window=std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)>;
using Surface=std::unique_ptr<SDL_Surface,decltype(&SDL_FreeSurface)>;
constexpr std::size_t query_limit=256;
class App {
public:
    Model model;
    Layout layout=make_layout();
    Window window{nullptr,SDL_DestroyWindow};
    PixelFrame frame{};
    std::string focused, state_path;
    std::map<std::string,TextEdit> editors;
    TextEdit& edit(const Widget& w){return editors.at(w.id);}
    const TextEdit& edit(const Widget& w) const{return editors.at(w.id);}
    using StagedEditors=std::map<std::string,TextEdit>;
    void publish_editors(StagedEditors& staged) noexcept {
        static_assert(std::is_nothrow_move_assignable<TextEdit>::value,"editor publication must not throw");
        for(auto& entry:staged)editors.find(entry.first)->second=std::move(entry.second);
    }
    bool running=true,save_blocked=false,cancellation_requested=false,close_after_save=false;
    AsyncStateStore::Operation pending_operation=AsyncStateStore::Operation::load;
    std::uint64_t paints=0,errors=0,generation=0,request_generation=0;
    std::uint64_t stored_revision=0,request_persistence_revision=0;
    std::string clean_status="New state file";
    AsyncStateStore::RequestId pending_request=0;
    struct Notification {std::atomic<bool> queued{false},failed{false};Uint32 type=0;};
    std::shared_ptr<Notification> notification;
    std::unique_ptr<AsyncStateStore> effects;
    std::string status;
#ifdef EUI_LINUX_ACCESSIBILITY
    struct AccessibleQueue {std::mutex mutex;std::deque<AccessibilityRequest> requests;Uint32 type=0;};
    std::shared_ptr<AccessibleQueue> accessible_queue;
    std::unique_ptr<LinuxAccessibility> accessibility;
    static int characters(const std::string& s,std::size_t end){int n=0;for(std::size_t i=0;i<end;++i)if((static_cast<unsigned char>(s[i])&0xc0)!=0x80)++n;return n;}
    void publish_accessibility(){
        if(!accessibility)return;
        int x=0,y=0;SDL_GetWindowPosition(window.get(),&x,&y);
        std::vector<AccessibleNode> nodes;
        AccessibleNode frame_node;frame_node.id=accessible_frame_id();frame_node.name=SDL_GetWindowTitle(window.get());frame_node.role=AccessibleNode::Role::frame;frame_node.x=x;frame_node.y=y;frame_node.width=layout.width;frame_node.height=layout.height;nodes.push_back(frame_node);
        for(const auto& w:layout.widgets){AccessibleNode n;n.id=accessible_widget_id(w.id);n.target=w.id;n.parent=accessible_frame_id();n.x=x+w.x;n.y=y+w.y;n.width=w.width;n.height=w.height;n.focused=focused==w.id;n.enabled=w.enabled;
            switch(w.kind){
            case WidgetKind::text:n.role=AccessibleNode::Role::label;n.name=display_text(w.binding.empty()?w.text:model.view_text(w.binding));break;
            case WidgetKind::button:n.role=AccessibleNode::Role::button;n.name=display_text(w.text);break;
            case WidgetKind::search:{const auto& editor=edit(w);n.role=AccessibleNode::Role::entry;n.name=display_text(w.text.empty()?w.id:w.text);n.text=model.state_text(w.state);n.caret=characters(editor.text(),editor.caret());n.anchor=characters(editor.text(),editor.anchor());n.text_geometry=editor_text_geometry(w,editor,n.focused);break;}
            case WidgetKind::list:n.role=AccessibleNode::Role::list;n.name="Visible items";break;
            }nodes.push_back(n);
            if(w.kind==WidgetKind::list){const auto& rows=model.view_texts(w.binding);auto count=std::min(rows.size(),static_cast<std::size_t>(std::max(0,(w.height-64)/36)));
                for(std::size_t i=0;i<count;++i){AccessibleNode row;row.name=display_text(rows[i]);row.id=accessible_row_id(w.id,i,row.name);row.parent=n.id;row.role=AccessibleNode::Role::row;row.x=n.x+10;row.y=n.y+42+static_cast<int>(i)*36;row.width=std::max(0,n.width-20);row.height=34;nodes.push_back(std::move(row));}}
        }accessibility->publish(std::move(nodes));
    }
    void accessibility_event(){
        std::deque<AccessibilityRequest> requests;{std::lock_guard<std::mutex> lock(accessible_queue->mutex);requests.swap(accessible_queue->requests);}
        if(close_after_save||!running)return; // Save-on-close freezes mutations, including AT requests.
        const auto reject=[this](const std::exception& e){++errors;std::cerr<<"accessibility request rejected: "<<e.what()<<'\n';};
        for(const auto& r:requests){const auto* w=find(r.id);if(!w||!widget_has_area(*w)||!widget_enabled(model,*w))continue;
            try {switch(r.kind){
            case AccessibilityRequest::Kind::activate:if(w->kind==WidgetKind::button)activate(*w);break;
            case AccessibilityRequest::Kind::focus:focus(w);break;
            case AccessibilityRequest::Kind::replace:if(w->kind==WidgetKind::search)set_query(*w,r.text);break;
            case AccessibilityRequest::Kind::select:if(w->kind==WidgetKind::search){TextEdit candidate=edit(*w);candidate.select(character_byte(candidate.text(),r.anchor),character_byte(candidate.text(),r.caret));apply_editor(*w,std::move(candidate));}break;
            }}catch(const std::invalid_argument& e){reject(e);}
            catch(const std::overflow_error& e){reject(e);}
            catch(const std::out_of_range& e){reject(e);}
            catch(const std::length_error& e){reject(e);}
        }
    }
#endif

    explicit App(std::string path={}):state_path(std::move(path)) {
        for(const auto& w:layout.widgets)if(w.kind==WidgetKind::search){single_line(model.state_text(w.state));editors.emplace(w.id,TextEdit(model.state_text(w.state),query_limit));}
        if(editors.size()>16)throw std::invalid_argument("too many input fields");
        if(!state_path.empty())load_initial_state();
        window.reset(SDL_CreateWindow("Explicit UI / native interaction",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,layout.width,layout.height,SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE));
        if(!window) throw std::runtime_error(SDL_GetError());
        SDL_SetWindowMinimumSize(window.get(),320,320);SDL_SetWindowMaximumSize(window.get(),4096,4096);
        SDL_StopTextInput();resize();
        if(!state_path.empty()){
            notification=std::make_shared<Notification>();notification->type=SDL_RegisterEvents(1);
            if(notification->type==static_cast<Uint32>(-1))throw std::runtime_error("cannot register state event");
            const auto signal=notification;
            effects=std::make_unique<AsyncStateStore>(state_path,[signal]{
                if(signal->queued.exchange(true))return;
                SDL_Event event{};event.type=signal->type;
                if(SDL_PushEvent(&event)!=1){signal->queued.store(false);signal->failed.store(true);}
            });
        }
#ifdef EUI_LINUX_ACCESSIBILITY
        if(std::getenv("EUI_ACCESSIBILITY")){
            accessible_queue=std::make_shared<AccessibleQueue>();accessible_queue->type=SDL_RegisterEvents(1);
            if(accessible_queue->type==static_cast<Uint32>(-1))throw std::runtime_error("cannot register accessibility event");
            auto queue=accessible_queue;accessibility=std::make_unique<LinuxAccessibility>([queue](AccessibilityRequest request){
                std::lock_guard<std::mutex> lock(queue->mutex);if(queue->requests.size()>=64)return false;
                queue->requests.push_back(std::move(request));SDL_Event event{};event.type=queue->type;
                if(SDL_PushEvent(&event)!=1){queue->requests.pop_back();return false;}return true;
            });publish_accessibility();
        }
#endif
        if(!status.empty())set_status(status);

    }
    ~App(){
#ifdef EUI_LINUX_ACCESSIBILITY
        accessibility.reset();if(accessible_queue)SDL_FlushEvent(accessible_queue->type);
#endif
        if(effects)effects->close();
        if(notification)SDL_FlushEvent(notification->type);
    }
    void set_status(const std::string& next){
        status=next;if(window)SDL_SetWindowTitle(window.get(),("Explicit UI / "+status).c_str());
#ifdef EUI_LINUX_ACCESSIBILITY
        if(window&&accessibility)publish_accessibility();
#endif
    }
    bool has_composition() const noexcept {
        for(const auto& entry:editors)if(!entry.second.preedit().empty())return true;
        return false;
    }
    void mark_unsaved(){
        if(state_path.empty()||pending_request||save_blocked)return;
        const char* next=nullptr;
        if(model.persistence_revision()!=stored_revision)next="Unsaved changes";
        else if(has_composition())next="Uncommitted text composition";
        else if(status=="Uncommitted text composition"||status=="Saved snapshot; text composition pending"||status=="Save rejected: finish text composition")next=clean_status.c_str();
        if(next&&status!=next)set_status(next);
    }
    bool apply_model(const std::vector<Update>& updates){
        if(generation==std::numeric_limits<std::uint64_t>::max())throw std::overflow_error("model generation exhausted");
        const bool changed=model.apply(updates);++generation;return changed;
    }
    const Widget* find(const std::string& id) const {for(const auto& w:layout.widgets)if(w.id==id)return &w;return nullptr;}
    const Widget* search() const {for(const auto& w:layout.widgets)if(w.kind==WidgetKind::search)return &w;return nullptr;}
    std::string query() const {const auto* w=search();return w?model.state_text(w->state):"";}
    void resize() {
        int width,height;SDL_GetWindowSize(window.get(),&width,&height);
        if(width<=0||height<=0)return;
        if(width!=layout.width||height!=layout.height||frame.pixels.empty()){layout=make_layout(width,height);
            const auto* active=find(focused);if(active&&active->kind==WidgetKind::search){SDL_Rect rect{active->x,active->y,active->width,active->height};SDL_SetTextInputRect(&rect);}
            paint();}
        else present();
    }
    void paint() {
        // Called only after model/editor transactions have both committed.
        for(auto& w:layout.widgets)w.enabled=widget_enabled(model,w);
        const auto* active=find(focused);
        if(active&&(!active->enabled||!widget_has_area(*active))){focused.clear();selecting_text=false;if(active->kind==WidgetKind::search){const bool composing=!edit(*active).preedit().empty();edit(*active).cancel_preedit();if(composing)mark_unsaved();}SDL_StopTextInput();}
        frame=render_layout(model,layout,query(),focused,nullptr,&editors);
        present(); ++paints;
#ifdef EUI_LINUX_ACCESSIBILITY
        publish_accessibility();
#endif
    }
    void present() {
        auto* dst=SDL_GetWindowSurface(window.get());if(!dst)throw std::runtime_error(SDL_GetError());
        Surface src(SDL_CreateRGBSurfaceFrom(frame.pixels.data(),frame.width,frame.height,32,frame.stride,0x00ff0000,0x0000ff00,0x000000ff,0xff000000),SDL_FreeSurface);
        if(!src)throw std::runtime_error(SDL_GetError());
        check(SDL_UpperBlitScaled(src.get(),nullptr,dst,nullptr));
        // Bound update to the allocated destination surface extent.
        SDL_Rect bounds{};SDL_GetClipRect(dst,&bounds);
        if(std::getenv("EUI_DIAGNOSTICS")){int w,h;SDL_GetWindowSize(window.get(),&w,&h);std::cerr<<"present window="<<w<<'x'<<h<<" frame="<<frame.width<<'x'<<frame.height<<" clip="<<bounds.x<<','<<bounds.y<<','<<bounds.w<<','<<bounds.h<<'\n';}
        check(SDL_UpdateWindowSurfaceRects(window.get(),&bounds,1));
    }
    bool selecting_text=false;
    void focus(const Widget* w) {
        if(w&&(!widget_has_area(*w)||!widget_enabled(model,*w)))return;
        const std::string next=w&&(w->kind==WidgetKind::button||w->kind==WidgetKind::search)?w->id:"";
        if(next==focused)return;
        selecting_text=false;bool cancelled_composition=false;
        if(const auto* previous=find(focused);previous&&previous->kind==WidgetKind::search){cancelled_composition=!edit(*previous).preedit().empty();edit(*previous).cancel_preedit();}
        focused=next;if(cancelled_composition)mark_unsaved();
        if(w&&w->kind==WidgetKind::search){
            int width,height;SDL_GetWindowSize(window.get(),&width,&height);
            SDL_Rect rect{w->x*width/layout.width,w->y*height/layout.height,w->width*width/layout.width,w->height*height/layout.height};
            SDL_SetTextInputRect(&rect);SDL_StartTextInput();
        }else SDL_StopTextInput();
        paint();
    }
    static bool same_editor(const TextEdit& a,const TextEdit& b){return a.text()==b.text()&&a.caret()==b.caret()&&a.anchor()==b.anchor()&&a.preedit()==b.preedit()&&a.preedit_selection_start()==b.preedit_selection_start()&&a.preedit_selection_end()==b.preedit_selection_end();}
    template<class V> void validate_ui_value(const std::string& id,const V& value,StagedEditors& staged) const {
        for(const auto& widget:layout.widgets){
            if(widget.state!=id)continue;
            if(widget.kind==WidgetKind::search){const auto* text=std::get_if<std::string>(&value);if(!text)throw std::invalid_argument("editor state requires text");single_line(*text);if(edit(widget).text()!=*text||!edit(widget).preedit().empty()){TextEdit candidate=edit(widget);candidate.reset(*text);staged.insert_or_assign(widget.id,std::move(candidate));}}
            if(widget.kind==WidgetKind::list){const auto* offset=std::get_if<std::int64_t>(&value);if(!offset||*offset<0||*offset>100000)throw std::invalid_argument("list offset out of range");}
        }
    }
    void activate(const Widget& w) {
        if(!widget_has_area(w)||!widget_enabled(model,w))return; // UI policy, not a model authorization boundary.
        std::vector<Update> updates;
        if(!w.action.empty())updates=model.prepare_action(w.action);
        else{
            const auto old=model.state_int(w.state);
            if((w.delta>0&&old>std::numeric_limits<std::int64_t>::max()-w.delta)||(w.delta<0&&old<std::numeric_limits<std::int64_t>::min()-w.delta))throw std::overflow_error("button integer overflow");
            updates.push_back({w.state,old+w.delta});
        }
        StagedEditors staged;for(const auto& update:updates)validate_ui_value(update.id,update.value,staged);
        const bool changed=apply_model(updates);
        publish_editors(staged);mark_unsaved();if(changed||!staged.empty())paint();
    }
    static void single_line(const std::string& value) {
        require_utf8(value);if(value.size()>query_limit)throw std::invalid_argument("single-line text exceeds byte limit");if(value.find("\xe2\x80\xa8")!=std::string::npos||value.find("\xe2\x80\xa9")!=std::string::npos)throw std::invalid_argument("single-line text rejects separators");for(unsigned char c:value)if(c<32||c==127)throw std::invalid_argument("single-line text rejects control characters");
    }
    void apply_editor(const Widget& w,TextEdit candidate) {
        single_line(candidate.text());
        auto& editor=edit(w);
        StagedEditors staged;
        const bool changed=candidate.text()!=model.state_text(w.state);
        if(!changed&&same_editor(candidate,editor))return;
        if(changed){
            editor_snap_graphemes(candidate);
            std::vector<Update> updates{{w.state,candidate.text()}};
            for(const auto& id:w.reset_states)updates.push_back({id,std::int64_t{0}});
            for(const auto& update:updates)validate_ui_value(update.id,update.value,staged);
            staged.insert_or_assign(w.id,std::move(candidate));
            apply_model(updates);
        }
        if(!changed){if(generation==std::numeric_limits<std::uint64_t>::max())throw std::overflow_error("model generation exhausted");++generation;}
        if(changed)publish_editors(staged);else editor=std::move(candidate);
        mark_unsaved();
        paint(); // Caret/preedit can change independently of model views.
    }
    void set_query(const Widget& w,const std::string& value){TextEdit candidate=edit(w);candidate.reset(value);apply_editor(w,std::move(candidate));}
    void restore_values(const state_store::State& values){
        StagedEditors staged;for(const auto& entry:values)validate_ui_value(entry.first,entry.second,staged);
        if(generation==std::numeric_limits<std::uint64_t>::max())throw std::overflow_error("model generation exhausted");
        app_state::restore(model,values);publish_editors(staged);++generation;
    }
    static bool missing_file(const std::exception_ptr& error){
        try{if(error)std::rethrow_exception(error);}catch(const std::system_error& e){return e.code()==std::errc::no_such_file_or_directory;}catch(...){}return false;
    }
    void load_initial_state(){
        try{restore_values(state_store::load(state_path));save_blocked=false;stored_revision=model.persistence_revision();clean_status=status="Loaded";std::cout<<"state_loaded\n";}
        catch(const std::exception& error){++errors;save_blocked=!missing_file(std::current_exception());status=save_blocked?"Load rejected; saving blocked":"New state file";std::cerr<<"state load rejected: "<<error.what()<<'\n';}
    }
    void save_state() {
        if(!effects)return;
        if(has_composition()){++errors;set_status("Save rejected: finish text composition");return;}
        if(save_blocked){++errors;set_status("Save blocked: reload a valid state file");std::cerr<<"state save rejected: original file was not accepted\n";return;}
        try{const auto captured=app_state::capture(model);pending_request=effects->save(captured);request_persistence_revision=model.persistence_revision();request_generation=generation;pending_operation=AsyncStateStore::Operation::save;cancellation_requested=false;set_status("Saving");}
        catch(const std::exception& error){++errors;set_status("Save rejected");std::cerr<<"state save rejected: "<<error.what()<<'\n';}
    }
    void load_state() {
        if(!effects||close_after_save)return;
        try{pending_request=effects->load();request_generation=generation;pending_operation=AsyncStateStore::Operation::load;cancellation_requested=false;set_status("Loading");}
        catch(const std::exception& error){++errors;set_status("Load rejected");std::cerr<<"state load rejected: "<<error.what()<<'\n';}
    }
    void cancel_state(){if(effects&&pending_request){effects->cancel();cancellation_requested=true;set_status("Cancel requested");if(close_after_save)running=false;}}
    void request_close(){
        if(pending_request&&pending_operation==AsyncStateStore::Operation::save&&!cancellation_requested){close_after_save=true;set_status("Saving before close; Ctrl+Esc cancels");return;}
        if(pending_request)cancel_state();
        running=false;
    }
    void complete_state(){
        if(!effects)return;
        auto completion=effects->take_completed();
        if(!completion){if(notification&&notification->failed.exchange(false))set_status("State notification failed; retry");return;}
        if(notification)notification->failed.store(false);
        if(completion->id!=pending_request)return;
        pending_request=0;
        if(cancellation_requested||completion->cancelled){set_status(completion->save_completed?"Cancelled after save committed":"Cancelled");std::cout<<"state_cancelled\n";return;}
        if(completion->error){close_after_save=false;++errors;if(completion->operation==AsyncStateStore::Operation::load)save_blocked=!missing_file(completion->error);set_status(completion->operation==AsyncStateStore::Operation::load?"Load rejected":"Save error");try{std::rethrow_exception(completion->error);}catch(const std::exception& error){std::cerr<<(completion->operation==AsyncStateStore::Operation::load?"state load rejected: ":"state save rejected: ")<<error.what()<<'\n';}return;}
        if(completion->operation==AsyncStateStore::Operation::save){stored_revision=request_persistence_revision;clean_status="Saved";set_status(model.persistence_revision()!=stored_revision?"Saved snapshot; newer edits pending":has_composition()?"Saved snapshot; text composition pending":"Saved");std::cout<<"state_saved\n";if(close_after_save)running=false;return;}
        if(request_generation!=generation){set_status("Load skipped: newer edits");std::cout<<"state_load_stale\n";return;}
        try{restore_values(completion->state);save_blocked=false;stored_revision=model.persistence_revision();clean_status="Loaded";set_status("Loaded");std::cout<<"state_loaded\n";}
        catch(const std::exception& error){++errors;save_blocked=true;set_status("Load rejected; saving blocked");std::cerr<<"state load rejected: "<<error.what()<<'\n';return;}
        paint();
    }
    // Synthetic-test helper only. The normal event loop has no timeout/polling.
    void wait_state_for_test(){
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
        while(pending_request){if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("state test timeout");SDL_Event e{};if(SDL_WaitEventTimeout(&e,100))dispatch(e);else complete_state();}
    }
    static std::size_t character_byte(const std::string& value,std::int64_t index) {
        if(index<0)throw std::invalid_argument("negative composition range");
        std::size_t at=0;while(index--){if(at==value.size())throw std::invalid_argument("composition range exceeds text");++at;while(at<value.size()&&(static_cast<unsigned char>(value[at])&0xc0)==0x80)++at;}return at;
    }
    void tab(bool reverse) {
        std::vector<const Widget*> widgets;
        for(const auto& w:layout.widgets)if((w.kind==WidgetKind::button||w.kind==WidgetKind::search)&&widget_has_area(w)&&widget_enabled(model,w))widgets.push_back(&w);
        if(widgets.empty())return;
        auto it=std::find_if(widgets.begin(),widgets.end(),[&](const Widget* w){return w->id==focused;});
        std::size_t index=it==widgets.end()?(reverse?widgets.size()-1:0):(static_cast<std::size_t>(it-widgets.begin())+widgets.size()+(reverse?-1:1))%widgets.size();
        focus(widgets[index]);
    }
    void event(const SDL_Event& e) {
#ifdef EUI_LINUX_ACCESSIBILITY
        if(accessible_queue&&e.type==accessible_queue->type){accessibility_event();return;}
        if(e.type==SDL_WINDOWEVENT)publish_accessibility();
#endif
        if(notification&&e.type==notification->type){notification->queued.store(false);complete_state();return;}
        if(e.type==SDL_QUIT){request_close();return;}
        if(e.type==SDL_WINDOWEVENT){
            if(e.window.event==SDL_WINDOWEVENT_CLOSE)request_close();
            else if(e.window.event==SDL_WINDOWEVENT_FOCUS_LOST)focus(nullptr);
            else if(e.window.event==SDL_WINDOWEVENT_EXPOSED||e.window.event==SDL_WINDOWEVENT_RESIZED||e.window.event==SDL_WINDOWEVENT_SIZE_CHANGED)resize();
            return;
        }
        if(close_after_save){if(e.type==SDL_KEYDOWN&&e.key.keysym.sym==SDLK_ESCAPE&&(e.key.keysym.mod&(KMOD_CTRL|KMOD_GUI)))cancel_state();return;}
        if(e.type==SDL_MOUSEBUTTONDOWN&&e.button.button==SDL_BUTTON_LEFT){
            int width,height;SDL_GetWindowSize(window.get(),&width,&height);if(width<=0||height<=0)return;
            const auto* w=hit_test(layout,e.button.x*layout.width/width,e.button.y*layout.height/height);
            const bool was_focused=w&&w->id==focused;
            focus(w);if(w&&w->kind==WidgetKind::button)activate(*w);
            if(w&&w->kind==WidgetKind::search&&widget_has_area(*w)&&widget_enabled(model,*w)&&edit(*w).preedit().empty()){
                auto& editor=edit(*w);TextEdit candidate=editor;candidate.move_to(editor_hit_test(*w,editor,e.button.x*layout.width/static_cast<double>(width)),was_focused&&(SDL_GetModState()&3));
                apply_editor(*w,std::move(candidate));selecting_text=true;
            }
            return;
        }
        const auto* w=find(focused);
        if(e.type==SDL_MOUSEBUTTONUP&&e.button.button==SDL_BUTTON_LEFT){selecting_text=false;return;}
        if(e.type==SDL_MOUSEMOTION&&selecting_text&&w&&w->kind==WidgetKind::search){
            if(!(e.motion.state&1)){selecting_text=false;return;}
            auto& editor=edit(*w);
            if(!editor.preedit().empty())return;
            int width,height;SDL_GetWindowSize(window.get(),&width,&height);if(width<=0)return;
            TextEdit candidate=editor;candidate.move_to(editor_hit_test(*w,editor,e.motion.x*layout.width/static_cast<double>(width)),true);
            apply_editor(*w,std::move(candidate));return;
        }
        if(e.type==SDL_TEXTINPUT&&w&&w->kind==WidgetKind::search){
            const auto end=std::find(e.text.text,e.text.text+sizeof e.text.text,'\0');
            if(end==e.text.text+sizeof e.text.text)throw std::invalid_argument("unterminated SDL text event");
            TextEdit candidate=edit(*w);candidate.insert(std::string(e.text.text,end));apply_editor(*w,std::move(candidate));return;
        }
        if(e.type==SDL_TEXTEDITING&&w&&w->kind==WidgetKind::search){
            const auto end=std::find(e.edit.text,e.edit.text+sizeof e.edit.text,'\0');
            if(end==e.edit.text+sizeof e.edit.text)throw std::invalid_argument("unterminated composition event");
            const std::string preedit(e.edit.text,end);single_line(preedit);
            if(e.edit.length<0)throw std::invalid_argument("negative composition length");
            TextEdit candidate=edit(*w);candidate.set_preedit(preedit,character_byte(preedit,e.edit.start),character_byte(preedit,static_cast<std::int64_t>(e.edit.start)+e.edit.length));
            apply_editor(*w,std::move(candidate));return;
        }
        if(e.type==SDL_KEYDOWN){
            const auto key=e.key.keysym.sym;
            if((e.key.keysym.mod&(KMOD_CTRL|KMOD_GUI))&&!e.key.repeat){if(key=='s'){save_state();return;}if(key=='o'){load_state();return;}if(key==SDLK_ESCAPE){cancel_state();return;}}
            if(key==SDLK_TAB){tab((e.key.keysym.mod&3)!=0);return;}
            if(key==SDLK_ESCAPE){focus(nullptr);return;}
            if(!w)return;
            if(w->kind==WidgetKind::button&&(key==SDLK_RETURN||key==' ')){if(!e.key.repeat)activate(*w);return;}
            if(w->kind==WidgetKind::search){
                auto& editor=edit(*w);
                if(!editor.preedit().empty())return; // The IME owns editing keys until commit/cancel.
                const bool command=(e.key.keysym.mod&(KMOD_CTRL|KMOD_GUI))!=0,shift=(e.key.keysym.mod&3)!=0;
                TextEdit candidate=editor;
                if(command&&key=='z'){if(shift)candidate.redo();else candidate.undo();}
                else if(command&&key=='y')candidate.redo();
                else if(command&&key=='a')candidate.select_all();
                else if(command&&(key=='c'||key=='x')){
                    if(!candidate.has_selection())return;
                    if(SDL_SetClipboardText(candidate.selected_text().c_str())!=0)throw std::invalid_argument("clipboard write failed");
                    if(key=='c')return;
                    candidate.insert("");
                }else if(command&&key=='v'){
                    SDL_ClearError();std::unique_ptr<char,decltype(&SDL_free)> value(SDL_GetClipboardText(),SDL_free);
                    if(!value||*SDL_GetError())throw std::invalid_argument("clipboard read failed");
                    std::size_t count=0;while(count<=query_limit&&value.get()[count])++count;
                    if(count>query_limit)throw std::invalid_argument("clipboard exceeds query limit");
                    candidate.insert(std::string(value.get(),count));
                }else if(key==SDLK_LEFT)editor_move_visual(candidate,-1,shift);
                else if(key==SDLK_RIGHT)editor_move_visual(candidate,1,shift);
                else if(key==SDLK_HOME)candidate.move_home(shift);
                else if(key==SDLK_END)candidate.move_end(shift);
                else if(key==SDLK_DELETE)editor_delete_grapheme(candidate,false);
                else if(key==SDLK_BACKSPACE){if(command){candidate.select_all();candidate.insert("");}else editor_delete_grapheme(candidate,true);}
                else return;
                apply_editor(*w,std::move(candidate));return;
            }
        }
        if(e.type==SDL_MOUSEWHEEL){
            int x,y,width,height;SDL_GetMouseState(&x,&y);SDL_GetWindowSize(window.get(),&width,&height);if(width<=0||height<=0)return;
            const auto* list=hit_test(layout,x*layout.width/width,y*layout.height/height);
            if(!list||list->kind!=WidgetKind::list||list->state.empty())return;
            const auto old=std::clamp<std::int64_t>(model.state_int(list->state),0,100000);
            const auto movement=std::clamp<std::int64_t>(e.wheel.y,-100,100)*(e.wheel.direction==1?-1:1);
            // Do not scroll past the final currently materialized row. This needs no
            // extra data scan and prevents repeatedly walking into empty space.
            const auto visible=model.view_texts(list->binding).size();
            const auto forward_limit=std::min<std::int64_t>(100000,old+static_cast<std::int64_t>(visible?visible-1:0));
            const auto next=std::clamp<std::int64_t>(old-movement*std::clamp<std::int64_t>(list->delta,1,100),0,forward_limit);
            const bool changed=apply_model({{list->state,next}});mark_unsaved();if(changed)paint();
        }
    }
    void dispatch(const SDL_Event& e) {
        try{event(e);}
        catch(const std::invalid_argument& error){++errors;std::cerr<<"input rejected: "<<error.what()<<'\n';}
        catch(const std::overflow_error& error){++errors;std::cerr<<"input rejected: "<<error.what()<<'\n';}
        catch(const std::out_of_range& error){++errors;std::cerr<<"input rejected: "<<error.what()<<'\n';}
        catch(const std::length_error& error){++errors;std::cerr<<"input rejected: "<<error.what()<<'\n';}
        complete_state();
        // Rendering, SDL, allocation and other runtime failures are fatal, not bad input.
    }
    void pump() {SDL_Event e{};if(SDL_WaitEvent(&e)!=1)throw std::runtime_error(SDL_GetError());dispatch(e);}
    void drain() {SDL_Event e{};while(SDL_PollEvent(&e))dispatch(e);}
    void push(SDL_Event e){if(SDL_PushEvent(&e)!=1)throw std::runtime_error("SDL event queue rejected synthetic event");pump();drain();}
    void click(const Widget& w){int width,height;SDL_GetWindowSize(window.get(),&width,&height);SDL_Event e{};e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=(w.x+w.width/2)*width/layout.width;e.button.y=(w.y+w.height/2)*height/layout.height;push(e);}
    void key(Sint32 sym,Uint16 mod=0){SDL_Event e{};e.type=SDL_KEYDOWN;e.key.keysym.sym=sym;e.key.keysym.mod=mod;push(e);}
    void text(const std::string& value){if(value.size()>=32)throw std::runtime_error("test event too long");SDL_Event e{};e.type=SDL_TEXTINPUT;std::memcpy(e.text.text,value.c_str(),value.size()+1);push(e);}
    void smoke(const std::string& output) {
        auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
        drain();
        const Widget* button=nullptr;for(const auto& w:layout.widgets)if(w.kind==WidgetKind::button&&w.delta>0){button=&w;break;}
        require(button&&search(),"smoke needs increment and search widgets");
        auto& editor=edit(*search());
        set_query(*search(),"");
        const auto initial_errors=errors;
        const auto initial=model.state_int(button->state);click(*button);click(*button);require(model.state_int(button->state)==initial+2*button->delta,"repeated click failed");
        key(SDLK_RETURN);require(model.state_int(button->state)==initial+3*button->delta,"keyboard activation failed");
        click(*search());text("00007");require(query()=="00007"&&model.view_texts("list").size()==1,"search failed");
        text("\xe4\xb8\xad");key(SDLK_BACKSPACE);require(query()=="00007","UTF-8 backspace failed");
        key(SDLK_HOME);key(SDLK_RIGHT);text("X");require(query()=="0X0007","middle insertion failed");
        key(SDLK_LEFT,1);key(SDLK_DELETE);require(query()=="00007","selected delete failed");
        key('a',KMOD_CTRL);text("abcd");key(SDLK_HOME);key(SDLK_RIGHT,1);key(SDLK_RIGHT,1);
        key('c',KMOD_CTRL);key(SDLK_END);key('v',KMOD_CTRL);require(query()=="abcdab","clipboard selection copy/paste failed");
        key(SDLK_HOME);key(SDLK_RIGHT,1);key('x',KMOD_CTRL);require(query()=="bcdab","cut failed");
        key('a',KMOD_CTRL);text("00007");
        SDL_Event composition{};composition.type=SDL_TEXTEDITING;std::memcpy(composition.edit.text,"\xe4\xb8\xad",4);composition.edit.start=1;
        push(composition);require(query()=="00007"&&editor.preedit()=="\xe4\xb8\xad"&&editor.preedit_selection_start()==3,"composition must remain uncommitted");
        const auto composing_caret=editor.caret();key(SDLK_LEFT);require(editor.caret()==composing_caret,"IME editing key leaked to committed buffer");
        text("\xe6\x96\x87");require(query()=="00007\xe6\x96\x87"&&editor.preedit().empty(),"composition commit failed");key(SDLK_BACKSPACE);

        key(SDLK_BACKSPACE,KMOD_CTRL);require(query().empty(),"clear failed");
        set_query(*search(),std::string(query_limit,'z'));
        const auto before_rejections=paints;
        text("x");require(query().size()==query_limit&&errors==initial_errors+1&&paints==before_rejections,"oversized query was not atomically rejected");
        key(SDLK_BACKSPACE,KMOD_CTRL);text("safe");const auto safe_paints=paints;
        text(std::string(1,static_cast<char>(0xff)));require(query()=="safe"&&errors==initial_errors+2&&paints==safe_paints,"invalid UTF-8 was not atomically rejected");
        key(SDLK_BACKSPACE,KMOD_CTRL);
        const Widget* list=nullptr;for(const auto& item:layout.widgets)if(item.kind==WidgetKind::list){list=&item;break;}
        require(list!=nullptr,"smoke needs a list");
        SDL_WarpMouseInWindow(window.get(),list->x+list->width/2,list->y+list->height/2);drain();
        SDL_Event wheel{};wheel.type=SDL_MOUSEWHEEL;wheel.wheel.y=-1;push(wheel);
        require(model.state_int(list->state)==list->delta,"forward wheel failed");
        wheel.wheel.y=1;push(wheel);require(model.state_int(list->state)==0,"reverse wheel failed");
        push(wheel);require(model.state_int(list->state)==0,"negative offset clamp failed");
        model.apply({{list->state,std::int64_t{9999}}});paint();wheel.wheel.y=-1;push(wheel);
        require(model.state_int(list->state)==9999&&model.view_texts(list->binding).size()==1,"end scroll clamp failed");
        set_query(*search(),"reset");key(SDLK_BACKSPACE,KMOD_CTRL);
        text("zzzz");const auto before=paints;text("x");require(paints==before+1&&model.view_texts("list").empty(),"input-only repaint failed");
        key(SDLK_BACKSPACE,KMOD_CTRL);text("00007");
        const auto button_id=button->id;
        SDL_SetWindowSize(window.get(),520,740);drain();resize();button=find(button_id);
        require(layout.width==520&&frame.width==520&&frame.height==740,"resize must relayout and rerasterize");
        require(search()->y>button->y,"narrow layout did not stack content");
        click(*button);require(model.state_int(button->state)==initial+4*button->delta,"resized hit testing failed");
        SDL_Event lost{};lost.type=SDL_WINDOWEVENT;lost.window.event=SDL_WINDOWEVENT_FOCUS_LOST;push(lost);text("ignored");require(query()=="00007","unfocused input accepted");
        key(SDLK_TAB);require(!focused.empty(),"Tab focus failed");
        if(!state_path.empty()){
            const auto saved=app_state::capture(model);const auto error_count=errors;save_state();wait_state_for_test();
            if(errors==error_count){model.apply({{"count",std::int64_t{777}}});set_query(*search(),"temporary");load_state();wait_state_for_test();require(app_state::capture(model)==saved&&editor.text()==query(),"state restore failed");}
        }
        write_layout_png(frame,output);
        SDL_Event quit{};quit.type=SDL_QUIT;push(quit);require(!running,"quit failed");
        model.snapshot("synthetic-sdl-smoke",paints);
        std::cout<<"{\"smoke\":\"passed\",\"synthetic_events\":true,\"errors\":"<<errors<<",\"video_driver\":\""<<SDL_GetCurrentVideoDriver()<<"\"}\n";
    }
};
}
int main(int argc,char** argv){
    try{
        std::string smoke_path,state_path;
        for(int i=1;i<argc;++i){const std::string option=argv[i];
            if((option=="--smoke"||option=="--state-file")&&i+1<argc){auto& target=option=="--smoke"?smoke_path:state_path;if(!target.empty())throw std::invalid_argument("duplicate option");target=argv[++i];if(target.empty())throw std::invalid_argument("empty option path");}
            else {std::cerr<<"Usage: window-host [--state-file path] [--smoke output.png]\n";return 2;}
        }
        Sdl sdl;App app(state_path);
        std::cout<<"video_driver="<<SDL_GetCurrentVideoDriver()<<"\nframebuffer_acceleration="<<SDL_GetHint("SDL_FRAMEBUFFER_ACCELERATION")<<"\n";app.model.snapshot("window-ready",app.paints);
        if(!smoke_path.empty())app.smoke(smoke_path);else{while(app.running)app.pump();app.model.snapshot("window-closed",app.paints);}
        return 0;
    }catch(const std::exception& error){std::cerr<<"window host: "<<error.what()<<'\n';return 1;}
}
