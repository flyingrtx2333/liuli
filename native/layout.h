#pragma once
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
#include "model_api.h"
namespace explicit_ui {
enum class WidgetKind { text, button, search, list };
struct Widget {
    std::string id;
    WidgetKind kind;
    int x, y, width, height;
    std::string text, binding, state;
    std::int64_t delta;
    std::string action = {}; // Empty means legacy integer delta action.
    std::string enabled_view = {};
    bool enabled = true;
    std::vector<std::string> reset_states = {}; // Applied atomically with input text edits.
};
inline bool widget_has_area(const Widget& w) { return w.width>0 && w.height>0; }
inline bool widget_enabled(const ViewModel& model,const Widget& w) {
    return w.kind!=WidgetKind::button || w.enabled_view.empty() || model.view_bool(w.enabled_view);
}
struct Layout { int width, height; std::vector<Widget> widgets; };
enum class Axis { row, column };
struct Flow {
    bool leaf;
    Widget widget;
    Axis axis;
    int basis_width, basis_height, grow, padding, gap, stack_below;
    std::vector<Flow> children;
};
inline void place_flow(const Flow& node,int x,int y,int width,int height,std::vector<Widget>& out) {
    if(node.leaf) {auto widget=node.widget;widget.x=x;widget.y=y;widget.width=width;widget.height=height;out.push_back(std::move(widget));return;}
    const int px=std::min(node.padding,width/2),py=std::min(node.padding,height/2);
    x+=px;y+=py;width-=px*2;height-=py*2;
    const bool horizontal=node.axis==Axis::row && (node.stack_below==0 || width>=node.stack_below);
    const int available=horizontal?width:height;
    const int count=static_cast<int>(node.children.size());
    if(count==0)return;
    const int gap=count>1?std::min(node.gap,available/(count-1)):0;
    const int usable=available-gap*(count-1);
    int basis_total=0,grow_total=0;
    for(const auto& child:node.children){basis_total+=horizontal?child.basis_width:child.basis_height;grow_total+=child.grow;}
    int used=0,cumulative_basis=0,cumulative_grow=0;
    for(const auto& child:node.children) {
        cumulative_basis+=horizontal?child.basis_width:child.basis_height;
        cumulative_grow+=child.grow;
        // Cumulative division distributes rounding pixels deterministically without
        // negative rectangles. When constrained, basis lengths shrink proportionally.
        int edge=0;
        if(basis_total>usable)edge=static_cast<int>(std::int64_t(cumulative_basis)*usable/basis_total);
        else if(grow_total>0)edge=cumulative_basis+static_cast<int>(std::int64_t(usable-basis_total)*cumulative_grow/grow_total);
        else edge=cumulative_basis;
        const int size=edge-used;
        place_flow(child,x,y,horizontal?size:width,horizontal?height:size,out);
        if(horizontal)x+=size+gap;else y+=size+gap;
        used=edge;
    }
}
inline Layout resolve_layout(const Flow& root,int width,int height) {
    if(width<1||height<1||width>4096||height>4096)throw std::invalid_argument("layout size must be 1..4096 pixels per axis");
    Layout result{width,height,{}};place_flow(root,0,0,width,height,result.widgets);return result;
}
inline Layout scale_layout(Layout layout,int width,int height) {
    if(width<1||height<1||width>4096||height>4096)throw std::invalid_argument("layout size must be 1..4096 pixels per axis");
    for(auto& w:layout.widgets){const int right=(w.x+w.width)*width/layout.width,bottom=(w.y+w.height)*height/layout.height;w.x=w.x*width/layout.width;w.y=w.y*height/layout.height;w.width=right-w.x;w.height=bottom-w.y;}
    layout.width=width;layout.height=height;return layout;
}
// Half-open rectangles. Last declared interactive widget is topmost.
// Disabled controls block underlying controls; text overlays remain pass-through.
inline const Widget* hit_test(const Layout& layout, int x, int y) {
    for (auto it=layout.widgets.rbegin(); it!=layout.widgets.rend(); ++it)
        if(it->kind!=WidgetKind::text && it->width>0 && it->height>0 && x>=it->x && y>=it->y && x<it->x+it->width && y<it->y+it->height) return it->enabled?&*it:nullptr;
    return nullptr;
}
struct PixelFrame { int width, height, stride; std::vector<unsigned char> pixels; };
}
