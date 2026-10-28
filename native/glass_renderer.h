#pragma once
#include "model_api.h"
#include "layout.h"
#include "text_edit.h"
#include "text_geometry.h"
#include <cstdint>
#include <string>
#include <map>

namespace explicit_ui {
struct GlassMetrics {
    std::uint64_t cache_builds = 0;
    std::uint64_t render_frames = 0;
};
// CPU/headless glass-inspired approximation, not Apple's Liquid Glass API.
// Per-thread static material cache; each call paints current model values.
void render_glass(const ViewModel& model, const std::string& output_path);
GlassMetrics glass_metrics();
// ARGB32 native-endian pixels, owning buffer. Pixels match resolved layout size; fixed decorative backdrop may scale.
PixelFrame render_layout(const ViewModel& model, const Layout& layout, const std::string& query, const std::string& focused_id, const TextEdit* editor=nullptr, const std::map<std::string,TextEdit>* editors=nullptr);
// Same font, strong caret and horizontal scroll calculation as the painter.
TextGeometry editor_text_geometry(const Widget&,const TextEdit&,bool focused);
std::size_t editor_hit_test(const Widget&,const TextEdit&,double layout_x);
void editor_snap_graphemes(TextEdit&);
void editor_move_visual(TextEdit&,int direction,bool extend);
void editor_delete_grapheme(TextEdit&,bool backward);
void write_layout_png(const PixelFrame& frame, const std::string& path);
} // namespace explicit_ui
