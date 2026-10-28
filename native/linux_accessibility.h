#pragma once
#include "text_geometry.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>
namespace explicit_ui {
inline std::string accessible_widget_id(const std::string& id){return "widget/"+id;}
inline std::string accessible_row_id(const std::string& parent,std::size_t index,const std::string& text){return "row/"+std::to_string(parent.size())+":"+parent+"/"+std::to_string(index)+"/"+std::to_string(text.size())+":"+text;}
inline const char* accessible_frame_id(){return "frame";}
// Immutable semantic state, captured only on the SDL/model thread.
struct AccessibleNode {
    enum class Role { frame,label,button,entry,list,row };
    std::string id,parent,name,text,target;
    Role role=Role::label;
    int x=0,y=0,width=0,height=0,caret=0,anchor=0;
    TextGeometry text_geometry;
    bool focused=false;
    bool enabled=true;
};
struct AccessibilityRequest {
    enum class Kind { activate,focus,replace,select };
    Kind kind;std::string id,text;int anchor=0,caret=0;
};
// Pure startup check: no ATK tree, bus connection, context, or worker is created.
// Requires supported ATK/AT-SPI versions. A coherent fixed bridge is a
// deployment requirement: atk-bridge has no public runtime version query.
void require_linux_accessibility_runtime();
class LinuxAccessibility {
    struct Impl;std::unique_ptr<Impl> impl;
public:
    explicit LinuxAccessibility(std::function<bool(AccessibilityRequest)> enqueue);
    ~LinuxAccessibility();
    LinuxAccessibility(const LinuxAccessibility&)=delete;
    LinuxAccessibility& operator=(const LinuxAccessibility&)=delete;
    void publish(std::vector<AccessibleNode> nodes);
};
}
