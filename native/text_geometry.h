#pragma once
#include <vector>
namespace explicit_ui {
// Immutable UI-thread capture. ATK offsets count Unicode characters, never bytes.
// Rectangles/runs are widget-local device pixels (verified 1x SDL surface only).
struct TextPixelRect {int x=0,y=0,width=0,height=0;};
struct TextHitSpan {int begin=0,end=0,offset=0;};
struct TextGeometry {
    bool available=false; // false during preedit; committed text is not the displayed text.
    int hit_top=0,hit_bottom=0;
    std::vector<TextPixelRect> characters;
    std::vector<TextHitSpan> hits;
    std::vector<int> word_starts;
};
}
