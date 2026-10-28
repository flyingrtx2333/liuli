#pragma once
#include "cairo_abi.h"
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
namespace explicit_ui {
struct TextRect { double x=0,y=0,width=0,height=0; };
// Pango single-line shaped text. Pixel coordinates are relative to the top-left.
// UTF-8 byte indices, not codepoint or glyph indices. Strong bidi caret only.
class ShapedText {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    ShapedText(cairo_t* context, const std::string& text, double font_pixels, bool bold=false);
    ~ShapedText();
    ShapedText(const ShapedText&)=delete;
    ShapedText& operator=(const ShapedText&)=delete;
    ShapedText(ShapedText&&) noexcept;
    ShapedText& operator=(ShapedText&&) noexcept;
    double width() const;
    double height() const;
    TextRect caret(std::size_t byte_index) const;
    TextRect character_rect(std::size_t byte_index) const;
    // Returns grapheme-leading UTF-8 byte index, not trailing caret position.
    std::size_t character_at_x(double x) const;
    std::vector<int> word_starts() const; // Unicode character offsets
    double caret_x(std::size_t byte_index) const {return caret(byte_index).x;}
    std::size_t hit_test(double x) const;
    std::vector<TextRect> selection_rects(std::size_t start,std::size_t end) const;
    std::vector<std::size_t> grapheme_boundaries() const;
    std::size_t previous_grapheme(std::size_t byte_index) const;
    std::size_t next_grapheme(std::size_t byte_index) const;
    std::size_t move_visual(std::size_t byte_index,int direction) const;
    int unknown_glyphs() const;
    // Uses caller's current Cairo source and clip; does not change them.
    void draw(cairo_t* context,double x,double y) const;
};
}
