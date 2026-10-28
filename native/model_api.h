#pragma once
#include <cstdint>
#include <string>
#include <stdexcept>
#include <vector>

namespace explicit_ui {
// Strict UTF-8 validation at the typed runtime input boundary.
inline void require_utf8(const std::string& text) {
    for (std::size_t i = 0; i < text.size();) {
        const auto lead = static_cast<unsigned char>(text[i++]);
        if (lead < 0x80) continue;
        unsigned remaining = 0;
        uint32_t value = 0, minimum = 0;
        if (lead >= 0xc2 && lead <= 0xdf) { remaining = 1; value = lead & 31; minimum = 0x80; }
        else if (lead >= 0xe0 && lead <= 0xef) { remaining = 2; value = lead & 15; minimum = 0x800; }
        else if (lead >= 0xf0 && lead <= 0xf4) { remaining = 3; value = lead & 7; minimum = 0x10000; }
        else throw std::invalid_argument("invalid UTF-8 text");
        if (remaining > text.size() - i) throw std::invalid_argument("invalid UTF-8 text");
        while (remaining--) {
            const auto byte = static_cast<unsigned char>(text[i++]);
            if ((byte & 0xc0) != 0x80) throw std::invalid_argument("invalid UTF-8 text");
            value = (value << 6) | (byte & 63);
        }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
            throw std::invalid_argument("invalid UTF-8 text");
    }
}
struct Stats {
    uint64_t patches = 0;
    uint64_t rows_materialized = 0;
    uint64_t evaluations = 0;
};
// Read-only values remain valid until the next successful model commit.
class ViewModel {
public:
    virtual ~ViewModel() = default;
    virtual const std::string& view_text(const std::string& id) const = 0;
    virtual const std::vector<std::string>& view_texts(const std::string& id) const = 0;
    virtual bool view_bool(const std::string&) const { throw std::invalid_argument("unknown bool view"); }
    virtual Stats stats() const = 0;
};
}
