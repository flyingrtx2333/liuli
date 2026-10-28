#include "linux_accessibility.h"
#include <atk/atk.h>
#include <atk-bridge.h>
#include <algorithm>
#include <atomic>
#include <set>
#include <cstring>
#include <condition_variable>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <dlfcn.h>
#include <array>
#include <limits>
#include <charconv>
using explicit_ui::AccessibleNode;
using explicit_ui::AccessibilityRequest;
namespace eui_a11y_detail {
// Runtime APIs report upstream release numbers, not distro package versions.
// Parse numeric components strictly; never compare version strings lexically.
bool supported_atk_version(const char* version){
    if(!version)return false;
    std::array<int,3> parts{};const char* next=version;const char* end=version+std::strlen(version);
    for(std::size_t i=0;i<parts.size();++i){
        if(next==end||*next<'0'||*next>'9')return false;
        const auto result=std::from_chars(next,end,parts[i]);
        if(result.ec!=std::errc{})return false;
        next=result.ptr;if(i+1<parts.size()){if(next==end||*next!='.')return false;++next;}
    }
    return next==end&&parts>=std::array<int,3>{2,62,1};
}
struct Data {
    AccessibleNode value;
    AtkObject* parent=nullptr;
    std::vector<AtkObject*> children;
    std::function<bool(AccessibilityRequest)> enqueue;
    bool defunct=false;
};
struct EuiAccessible { AtkObject parent;Data* data; };
struct EuiAccessibleClass { AtkObjectClass parent; };
Data& data(gpointer p){return *reinterpret_cast<EuiAccessible*>(p)->data;}
bool has_area(const AccessibleNode& n){return n.width>0&&n.height>0;}
bool focusable(const AccessibleNode& n){return n.enabled&&has_area(n)&&(n.role==AccessibleNode::Role::button||n.role==AccessibleNode::Role::entry);}
void component_init(AtkComponentIface*);
G_DEFINE_TYPE_WITH_CODE(EuiAccessible,eui_accessible,ATK_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE(ATK_TYPE_COMPONENT,component_init))
void eui_accessible_init(EuiAccessible* self){self->data=new Data;}
void eui_accessible_class_init(EuiAccessibleClass* klass){
    G_OBJECT_CLASS(klass)->finalize=[](GObject* obj){delete reinterpret_cast<EuiAccessible*>(obj)->data;G_OBJECT_CLASS(eui_accessible_parent_class)->finalize(obj);};
    auto* c=ATK_OBJECT_CLASS(klass);
    c->get_parent=[](AtkObject* o){return data(o).parent;};
    c->get_n_children=[](AtkObject* o){return static_cast<int>(data(o).children.size());};
    c->ref_child=[](AtkObject* o,int i)->AtkObject*{auto& v=data(o).children;return i>=0&&static_cast<size_t>(i)<v.size()?ATK_OBJECT(g_object_ref(v[i])):nullptr;};
    c->get_index_in_parent=[](AtkObject* o){auto* p=data(o).parent;if(!p)return -1;auto& v=data(p).children;auto i=std::find(v.begin(),v.end(),o);return i==v.end()?-1:static_cast<int>(i-v.begin());};
    c->ref_state_set=[](AtkObject* o){auto* s=atk_state_set_new();auto& d=data(o);
        if(d.defunct){atk_state_set_add_state(s,ATK_STATE_DEFUNCT);return s;}
        atk_state_set_add_state(s,ATK_STATE_VISIBLE);
        if(d.value.enabled)for(auto flag:{ATK_STATE_ENABLED,ATK_STATE_SENSITIVE})atk_state_set_add_state(s,flag);
        if(has_area(d.value))atk_state_set_add_state(s,ATK_STATE_SHOWING);
        if(focusable(d.value))atk_state_set_add_state(s,ATK_STATE_FOCUSABLE);
        if(d.value.focused)atk_state_set_add_state(s,ATK_STATE_FOCUSED);
        if(d.value.role==AccessibleNode::Role::entry){atk_state_set_add_state(s,ATK_STATE_EDITABLE);atk_state_set_add_state(s,ATK_STATE_SINGLE_LINE);}
        return s;};
}
bool request(gpointer o,AccessibilityRequest::Kind kind,std::string text={},int anchor=0,int caret=0) noexcept {
    try{auto& d=data(o);return !d.defunct&&d.value.enabled&&((kind!=AccessibilityRequest::Kind::activate&&kind!=AccessibilityRequest::Kind::focus)||has_area(d.value))&&d.enqueue&&d.enqueue({kind,d.value.target.empty()?d.value.id:d.value.target,std::move(text),anchor,caret});}
    catch(...){return false;}
}
void component_init(AtkComponentIface* c){
    c->get_extents=[](AtkComponent* o,int* x,int* y,int* w,int* h,AtkCoordType type){auto& d=data(o);*x=d.value.x;*y=d.value.y;*w=d.value.width;*h=d.value.height;
        if(type==ATK_XY_WINDOW){auto* p=ATK_OBJECT(o);while(data(p).parent&&atk_object_get_role(p)!=ATK_ROLE_FRAME)p=data(p).parent;*x-=data(p).value.x;*y-=data(p).value.y;}
        else if(type==ATK_XY_PARENT&&d.parent){*x-=data(d.parent).value.x;*y-=data(d.parent).value.y;}};
    c->grab_focus=[](AtkComponent* o)->gboolean{auto role=data(o).value.role;return (role==AccessibleNode::Role::button||role==AccessibleNode::Role::entry)&&request(o,AccessibilityRequest::Kind::focus);};
    c->get_layer=[](AtkComponent*){return ATK_LAYER_WIDGET;};
}
struct EuiButton { EuiAccessible parent; };struct EuiButtonClass { EuiAccessibleClass parent; };
void action_init(AtkActionIface* c){c->get_n_actions=[](AtkAction*){return 1;};c->get_name=[](AtkAction*,int i)->const char*{return i==0?"click":nullptr;};c->do_action=[](AtkAction* o,int i)->gboolean{return i==0&&request(o,AccessibilityRequest::Kind::activate);};}
G_DEFINE_TYPE_WITH_CODE(EuiButton,eui_button,eui_accessible_get_type(),G_IMPLEMENT_INTERFACE(ATK_TYPE_ACTION,action_init))
void eui_button_init(EuiButton*){} void eui_button_class_init(EuiButtonClass*){}
int length(gpointer o){return static_cast<int>(g_utf8_strlen(data(o).value.text.c_str(),-1));}
char* substring(gpointer o,int start,int end){auto& t=data(o).value.text;int n=length(o);if(end==-1)end=n;if(start<0||start>n||end<start||end>n)return g_strdup("");auto* p=g_utf8_offset_to_pointer(t.c_str(),start);auto* q=g_utf8_offset_to_pointer(t.c_str(),end);return g_strndup(p,q-p);}
bool pixel_coordinate(long long value,int& out){
    if(value<std::numeric_limits<int>::min()||value>std::numeric_limits<int>::max())return false;
    out=static_cast<int>(value);return true;
}
bool coordinate_origin(gpointer o,AtkCoordType type,int& x,int& y){
    auto& d=data(o);long long ox=d.value.x,oy=d.value.y;
    if(type==ATK_XY_PARENT){if(d.parent){ox-=data(d.parent).value.x;oy-=data(d.parent).value.y;}}
    else if(type==ATK_XY_WINDOW){auto* p=ATK_OBJECT(o);while(data(p).parent&&atk_object_get_role(p)!=ATK_ROLE_FRAME)p=data(p).parent;
        ox-=data(p).value.x;oy-=data(p).value.y;}
    else if(type!=ATK_XY_SCREEN)return false;
    return pixel_coordinate(ox,x)&&pixel_coordinate(oy,y);
}
char* string_at(gpointer o,int at,AtkTextGranularity granularity,int* start,int* end){
    *start=*end=-1;const int n=length(o);if(data(o).defunct||at<0||at>=n)return nullptr;
    int s=0,e=n;
    switch(granularity){
    case ATK_TEXT_GRANULARITY_CHAR:s=at;e=at+1;break;
    case ATK_TEXT_GRANULARITY_WORD:{const auto& b=data(o).value.text_geometry.word_starts;
        auto next=std::upper_bound(b.begin(),b.end(),at);if(next==b.begin()||next==b.end())return nullptr;s=*std::prev(next);e=*next;break;}
    case ATK_TEXT_GRANULARITY_LINE:case ATK_TEXT_GRANULARITY_PARAGRAPH:break;
    default:return nullptr; // Sentence, legacy before/after, attributes and scrolling are not implemented.
    }
    *start=s;*end=e;return substring(o,s,e);
}
void text_init(AtkTextIface* c){
    c->get_string_at_offset=[](AtkText* o,int at,AtkTextGranularity g,int* s,int* e){return string_at(o,at,g,s,e);};
    c->get_character_extents=[](AtkText* o,int at,int* x,int* y,int* w,int* h,AtkCoordType type){
        *x=*y=*w=*h=0;auto& d=data(o);const auto& geometry=d.value.text_geometry;int ox=0,oy=0;
        if(d.defunct||!geometry.available||at<0||static_cast<std::size_t>(at)>=geometry.characters.size()||!coordinate_origin(o,type,ox,oy))return;
        const auto& r=geometry.characters[static_cast<std::size_t>(at)];int rx=0,ry=0;
        if(!pixel_coordinate(static_cast<long long>(ox)+r.x,rx)||!pixel_coordinate(static_cast<long long>(oy)+r.y,ry))return;
        *x=rx;*y=ry;*w=r.width;*h=r.height;
    };
    c->get_offset_at_point=[](AtkText* o,int x,int y,AtkCoordType type){
        auto& d=data(o);const auto& geometry=d.value.text_geometry;int ox=0,oy=0;
        if(d.defunct||!geometry.available||!coordinate_origin(o,type,ox,oy))return -1;
        const auto lx=static_cast<long long>(x)-ox,ly=static_cast<long long>(y)-oy;
        if(ly<geometry.hit_top||ly>=geometry.hit_bottom)return -1;
        for(const auto& run:geometry.hits)if(lx>=run.begin&&lx<run.end)return run.offset;
        return -1;
    };
    c->get_text=[](AtkText* o,int s,int e){return substring(o,s,e);};c->get_character_count=[](AtkText* o){return length(o);};
    c->get_character_at_offset=[](AtkText* o,int at)->gunichar{if(at<0||at>=length(o))return 0;return g_utf8_get_char(g_utf8_offset_to_pointer(data(o).value.text.c_str(),at));};
    c->get_caret_offset=[](AtkText* o){return data(o).value.caret;};
    c->get_n_selections=[](AtkText* o){return data(o).value.caret!=data(o).value.anchor?1:0;};
    c->get_selection=[](AtkText* o,int i,int* s,int* e)->char*{auto& d=data(o).value;*s=std::min(d.anchor,d.caret);*e=std::max(d.anchor,d.caret);return i==0&&*s!=*e?substring(o,*s,*e):nullptr;};
    c->set_caret_offset=[](AtkText* o,int p)->gboolean{return p>=0&&p<=length(o)&&request(o,AccessibilityRequest::Kind::select,{},p,p);};
    c->set_selection=[](AtkText* o,int i,int s,int e)->gboolean{return i==0&&s>=0&&e>=s&&e<=length(o)&&request(o,AccessibilityRequest::Kind::select,{},s,e);};
}
void editable_init(AtkEditableTextIface* c){c->set_text_contents=[](AtkEditableText* o,const char* t){if(!t)return;std::size_t size=0;while(size<=256&&t[size])++size;
    if(size>256||!g_utf8_validate(t,static_cast<gssize>(size),nullptr))return;
    try{request(o,AccessibilityRequest::Kind::replace,std::string(t,size));}catch(...){}};}
struct EuiEntry { EuiAccessible parent; };struct EuiEntryClass { EuiAccessibleClass parent; };
G_DEFINE_TYPE_WITH_CODE(EuiEntry,eui_entry,eui_accessible_get_type(),G_IMPLEMENT_INTERFACE(ATK_TYPE_TEXT,text_init) G_IMPLEMENT_INTERFACE(ATK_TYPE_EDITABLE_TEXT,editable_init))
void eui_entry_init(EuiEntry*){} void eui_entry_class_init(EuiEntryClass*){}
// Attaching a source (unlike invoke) can never run an ATK callback inline on
// the publishing SDL thread, even before the worker enters its main loop.
void post(GMainContext* context,GSourceFunc callback,gpointer user){auto* source=g_idle_source_new();g_source_set_callback(source,callback,user,nullptr);g_source_attach(source,context);g_source_unref(source);}
std::atomic<bool> provider_claimed{false};
struct ProviderLease {
    ProviderLease(){bool expected=false;if(!provider_claimed.compare_exchange_strong(expected,true))throw std::runtime_error("only one accessibility provider per process is supported");}
    ~ProviderLease(){provider_claimed.store(false);}
};
AtkObject* root=nullptr;
AtkRole role(AccessibleNode::Role r){switch(r){case AccessibleNode::Role::frame:return ATK_ROLE_FRAME;case AccessibleNode::Role::label:return ATK_ROLE_LABEL;case AccessibleNode::Role::button:return ATK_ROLE_PUSH_BUTTON;case AccessibleNode::Role::entry:return ATK_ROLE_ENTRY;case AccessibleNode::Role::list:return ATK_ROLE_LIST;case AccessibleNode::Role::row:return ATK_ROLE_LIST_ITEM;}return ATK_ROLE_UNKNOWN;}
void apply_nodes(std::map<std::string,AtkObject*>& nodes,const std::vector<AccessibleNode>& values,const std::function<bool(AccessibilityRequest)>& enqueue){
    // Existing IDs have stable roles and parents; reject topology mutation before edits.
    for(const auto& v:values){auto old=nodes.find(v.id);if(old!=nodes.end()&&(data(old->second).value.role!=v.role||data(old->second).value.parent!=v.parent))throw std::invalid_argument("accessibility ID changed role or parent");}
        std::map<std::string,bool> keep;for(const auto& v:values)keep[v.id]=true;
        // Detach every removed object while all parent allocations are alive.
        std::vector<std::string> removed;
        for(auto& item:nodes){if(keep.count(item.first))continue;auto* o=item.second;auto& d=data(o);if(d.parent){auto& children=data(d.parent).children;auto pos=std::find(children.begin(),children.end(),o);if(pos!=children.end()){auto n=pos-children.begin();children.erase(pos);g_signal_emit_by_name(d.parent,"children-changed::remove",static_cast<int>(n),o,nullptr);}}
            d.defunct=true;d.parent=nullptr;d.children.clear();d.enqueue={};atk_object_notify_state_change(o,ATK_STATE_DEFUNCT,TRUE);removed.push_back(item.first);}
        for(const auto& id:removed){g_object_unref(nodes.at(id));nodes.erase(id);}
        for(auto& v:values){AtkObject* o;auto found=nodes.find(v.id);bool fresh=found==nodes.end();if(fresh){GType type=v.role==AccessibleNode::Role::button?eui_button_get_type():v.role==AccessibleNode::Role::entry?eui_entry_get_type():eui_accessible_get_type();o=ATK_OBJECT(g_object_new(type,nullptr));nodes[v.id]=o;data(o).enqueue=enqueue;atk_object_set_role(o,role(v.role));}else o=found->second;
            auto& d=data(o);const auto old=d.value;d.value=v;atk_object_set_name(o,v.name.c_str());
            if(fresh){d.parent=v.parent.empty()?root:nodes.at(v.parent);auto& children=data(d.parent).children;children.push_back(o);g_signal_emit_by_name(d.parent,"children-changed::add",static_cast<int>(children.size()-1),o,nullptr);}
            if(old.enabled!=v.enabled)for(auto flag:{ATK_STATE_ENABLED,ATK_STATE_SENSITIVE})atk_object_notify_state_change(o,flag,v.enabled);
            if(focusable(old)!=focusable(v))atk_object_notify_state_change(o,ATK_STATE_FOCUSABLE,focusable(v));
            if(has_area(old)!=has_area(v))atk_object_notify_state_change(o,ATK_STATE_SHOWING,has_area(v));
            if(old.focused!=v.focused)atk_object_notify_state_change(o,ATK_STATE_FOCUSED,v.focused);
            if(v.role==AccessibleNode::Role::entry){if(old.text!=v.text){if(!old.text.empty())g_signal_emit_by_name(o,"text-remove",0,static_cast<int>(g_utf8_strlen(old.text.c_str(),-1)),old.text.c_str());if(!v.text.empty())g_signal_emit_by_name(o,"text-insert",0,length(o),v.text.c_str());}if(old.caret!=v.caret)g_signal_emit_by_name(o,"text-caret-moved",v.caret);if(old.anchor!=v.anchor||old.caret!=v.caret)g_signal_emit_by_name(o,"text-selection-changed");}
        }
        // Snapshot order is semantic order, including replacement at an old row index.
        std::map<AtkObject*,std::vector<AtkObject*>> order;
        order[root]={};for(const auto& item:nodes)order[item.second]={};
        for(const auto& v:values){auto* o=nodes.at(v.id);order[data(o).parent].push_back(o);}
        for(const auto& entry:order){auto& children=data(entry.first).children;if(children==entry.second)continue;
            while(!children.empty()){auto* child=children.back();auto at=children.size()-1;children.pop_back();g_signal_emit_by_name(entry.first,"children-changed::remove",static_cast<int>(at),child,nullptr);}
            for(auto* child:entry.second){children.push_back(child);g_signal_emit_by_name(entry.first,"children-changed::add",static_cast<int>(children.size()-1),child,nullptr);}
        }
}

void validate_snapshot(const std::vector<AccessibleNode>& nodes){
    // Validate before crossing the C callback boundary; malformed trees are atomic rejections.
    if(nodes.size()>1024)throw std::length_error("accessibility snapshot exceeds 1024 objects");
    std::set<std::string> ids;std::size_t bytes=0;
    for(const auto& n:nodes){
        if(n.id.empty()||!ids.insert(n.id).second)throw std::invalid_argument("accessibility IDs must be unique and nonempty");
        if(!n.parent.empty()&&(n.parent==n.id||!ids.count(n.parent)))throw std::invalid_argument("accessibility parents must precede children");
        if(n.id.size()>1024||n.parent.size()>1024||n.target.size()>256||n.name.size()>4096||n.text.size()>256)throw std::length_error("accessibility field exceeds limit");
        if(n.width<0||n.height<0)throw std::invalid_argument("accessibility size must be nonnegative");
        bytes+=n.id.size()+n.parent.size()+n.name.size()+n.text.size()+n.target.size();if(bytes>1024*1024)throw std::length_error("accessibility snapshot exceeds byte limit");
        if(!g_utf8_validate(n.name.c_str(),static_cast<gssize>(n.name.size()),nullptr)||!g_utf8_validate(n.text.c_str(),static_cast<gssize>(n.text.size()),nullptr))throw std::invalid_argument("accessibility strings require UTF-8");
        const auto chars=g_utf8_strlen(n.text.c_str(),-1);
        const auto& g=n.text_geometry;
        if(g.characters.size()>256||g.hits.size()>4096||g.word_starts.size()>257)throw std::length_error("accessibility text geometry limit exceeded");
        bytes+=g.characters.size()*sizeof(explicit_ui::TextPixelRect)+g.hits.size()*sizeof(explicit_ui::TextHitSpan)+g.word_starts.size()*sizeof(int);
        if(bytes>1024*1024)throw std::length_error("accessibility snapshot exceeds byte limit");
        if(g.available&&g.characters.size()!=static_cast<std::size_t>(chars))throw std::invalid_argument("text geometry character count mismatch");
        if(!g.available&&(!g.characters.empty()||!g.hits.empty()))throw std::invalid_argument("unavailable text geometry has coordinates");
        for(const auto& r:g.characters)if(r.width<0||r.height<0||r.width>4096||r.height>4096||r.x < -65536||r.x>65536||r.y < -65536||r.y>65536)throw std::invalid_argument("invalid text rectangle");
        int previous=-65537;for(const auto& r:g.hits){if(r.begin<previous||r.begin<0||r.end<=r.begin||r.end>4096||r.offset<0||r.offset>=chars)throw std::invalid_argument("invalid text hit span");previous=r.end;}
        previous=-1;for(int b:g.word_starts){if(b<=previous||b<0||b>chars)throw std::invalid_argument("invalid text word boundaries");previous=b;}
        if(!g.word_starts.empty()&&(g.word_starts.front()!=0||g.word_starts.back()!=chars))throw std::invalid_argument("incomplete text word boundaries");
        if(n.caret<0||n.anchor<0||n.caret>chars||n.anchor>chars)throw std::invalid_argument("accessibility text offset out of range");
    }
}

}
using namespace eui_a11y_detail;
namespace explicit_ui {
void require_linux_accessibility_runtime(){
    const auto reject=[](const std::string& why){throw std::runtime_error(
        "Linux accessibility runtime rejected: "+why+
        ". This opt-in spike requires coherent ATK, AT-SPI and atk-bridge packages >= 2.62.1. "
        "For the verified amd64 cloud route, run python3 scripts/prepare-a11y-runtime.py and prepend "
        "<repo>/build/a11y-runtime/usr/lib/x86_64-linux-gnu to this process's LD_LIBRARY_PATH. "
        "Do not replace system libraries. atk-bridge has no public version query: these API checks "
        "cannot attest mixed/custom libraries. See docs/linux-accessibility-spike.md.");};
    const char* atk_version=atk_get_version();
    if(!supported_atk_version(atk_version))reject("ATK reports "+std::string(atk_version?atk_version:"unknown"));
    // Supported since AT-SPI 2.50. Resolve dynamically so old SDK headers can
    // still build the guard and old runtimes fail before bridge initialization.
    // Public signature: atspi-misc.h, void atspi_get_version(gint*,gint*,gint*).
    auto* version_symbol=dlsym(RTLD_DEFAULT,"atspi_get_version");
    if(!version_symbol)reject("AT-SPI runtime version API is unavailable");
    auto version=reinterpret_cast<void(*)(gint*,gint*,gint*)>(version_symbol);
    gint major=0,minor=0,micro=0;version(&major,&minor,&micro);
    if(major<0||minor<0||micro<0||std::array<int,3>{major,minor,micro}<std::array<int,3>{2,62,1})
        reject("AT-SPI reports "+std::to_string(major)+"."+std::to_string(minor)+"."+std::to_string(micro));
    // There is deliberately no binary hash/path heuristic here. Unlike ATK and
    // AT-SPI, the bridge cannot be versioned through a supported runtime API.
}

struct LinuxAccessibility::Impl {
    ProviderLease lease;
    GMainContext* context=g_main_context_ref(g_main_context_default());GMainLoop* loop=g_main_loop_new(context,FALSE);
    std::thread worker;std::mutex mutex;std::condition_variable ready;
    bool started=false;std::string error;std::vector<AccessibleNode> pending;bool queued=false;
    std::function<bool(AccessibilityRequest)> enqueue;std::map<std::string,AtkObject*> nodes;
    explicit Impl(std::function<bool(AccessibilityRequest)> callback):enqueue(std::move(callback)){
        worker=std::thread([this]{
            if(!g_main_context_acquire(context)){
                {std::lock_guard<std::mutex> lock(mutex);error="default GLib context is already owned";started=true;}
                ready.notify_one();return;
            }
            g_main_context_push_thread_default(context);
            if(root){error="only one accessibility provider per process is supported";}
            else {
                root=ATK_OBJECT(g_object_new(eui_accessible_get_type(),nullptr));atk_object_set_role(root,ATK_ROLE_APPLICATION);atk_object_set_name(root,"Explicit UI");
                static auto* util=ATK_UTIL_CLASS(g_type_class_ref(ATK_TYPE_UTIL));util->get_root=[](){return root;};util->get_toolkit_name=[]()->const char*{return "Explicit UI";};util->get_toolkit_version=[]()->const char*{return "0.5-spike";};
                if(atk_bridge_adaptor_init(nullptr,nullptr)!=0)error="AT-SPI bridge initialization failed";
                // The dedicated worker services the default context: bridge cleanup
                // uses g_source_remove(), which only searches that context.
            }
            {std::lock_guard<std::mutex> lock(mutex);started=true;}ready.notify_one();
            if(error.empty())g_main_loop_run(loop);
            // Finalize owned descendants while the bridge's weak-reference registry
            // is still alive. Keep the application root alive for bridge deregistration.
            apply_nodes(nodes,{},enqueue);
            atk_bridge_adaptor_cleanup();
            if(root){g_object_unref(root);root=nullptr;}
            g_main_context_pop_thread_default(context);g_main_context_release(context);
        });
        std::unique_lock<std::mutex> lock(mutex);ready.wait(lock,[this]{return started;});if(!error.empty()){lock.unlock();worker.join();g_main_loop_unref(loop);g_main_context_unref(context);throw std::runtime_error(error);}
    }
    ~Impl(){post(context,[](gpointer p)->gboolean{g_main_loop_quit(static_cast<Impl*>(p)->loop);return G_SOURCE_REMOVE;},this);worker.join();g_main_loop_unref(loop);g_main_context_unref(context);}
    void apply(){
        std::vector<AccessibleNode> values;{std::lock_guard<std::mutex> lock(mutex);values.swap(pending);queued=false;}
        apply_nodes(nodes,values,enqueue);
    }
};
LinuxAccessibility::LinuxAccessibility(std::function<bool(AccessibilityRequest)> f){require_linux_accessibility_runtime();impl=std::make_unique<Impl>(std::move(f));}
LinuxAccessibility::~LinuxAccessibility()=default;
void LinuxAccessibility::publish(std::vector<AccessibleNode> nodes){
    validate_snapshot(nodes);
    bool schedule;{std::lock_guard<std::mutex> lock(impl->mutex);impl->pending=std::move(nodes);schedule=!impl->queued;impl->queued=true;}if(schedule)post(impl->context,[](gpointer p)->gboolean{try{static_cast<Impl*>(p)->apply();}catch(const std::exception& error){g_warning("accessibility snapshot rejected: %s",error.what());}catch(...){g_warning("accessibility snapshot failed");}return G_SOURCE_REMOVE;},impl.get());}
}
