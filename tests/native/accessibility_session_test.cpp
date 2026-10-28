#include "linux_accessibility.h"
#include <iostream>
#include <stdexcept>
using namespace explicit_ui;
int main(){try{
    for(int i=0;i<3;++i){
        LinuxAccessibility first([](AccessibilityRequest){return true;});
        bool duplicate=false;try{LinuxAccessibility second([](AccessibilityRequest){return true;});}catch(const std::runtime_error&){duplicate=true;}
        if(!duplicate)throw std::runtime_error("duplicate provider was accepted");
        AccessibleNode frame;frame.id="frame";frame.name="Lifecycle test";frame.role=AccessibleNode::Role::frame;
        AccessibleNode list;list.id="list";list.parent="frame";list.role=AccessibleNode::Role::list;
        AccessibleNode row;row.id="row";row.parent="list";row.role=AccessibleNode::Role::row;
        bool invalid=false;try{first.publish({row,frame});}catch(const std::invalid_argument&){invalid=true;}
        if(!invalid)throw std::runtime_error("invalid parent order accepted");
        first.publish({frame,list,row});
        // Destructor posts after the snapshot source; FIFO main-context dispatch
        // applies the tree before ordered teardown. No sleep or polling needed.
    }
    std::cout<<"PASS 3 same-process bridge init/teardown cycles; duplicate constructor rejection; malformed snapshot rejection\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
