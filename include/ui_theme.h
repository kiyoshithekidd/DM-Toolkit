#pragma once

#include <cstdint>

namespace dmtk {

struct Rect {
    float x;
    float y;
    float w;
    float h;

    bool contains(float px, float py) const {
        return px >= x && px <= (x + w) && py >= y && py <= (y + h);
    }
};

// Warm tavern / bard's toolkit color palette (RGBA8 packed for C2D_Color32)
constexpr uint32_t rgba32(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return static_cast<uint32_t>(r) |
           (static_cast<uint32_t>(g) << 8) |
           (static_cast<uint32_t>(b) << 16) |
           (static_cast<uint32_t>(a) << 24);
}

namespace theme {
    constexpr uint32_t WALNUT_DARK    = rgba32(28, 18, 12);
    constexpr uint32_t WALNUT         = rgba32(46, 30, 20);
    constexpr uint32_t WALNUT_LIGHT   = rgba32(72, 48, 34);
    constexpr uint32_t PARCHMENT      = rgba32(250, 243, 224);
    constexpr uint32_t PARCHMENT_ALT  = rgba32(238, 228, 202);
    constexpr uint32_t PARCHMENT_EDGE = rgba32(188, 168, 130);
    constexpr uint32_t GOLD           = rgba32(212, 175, 55);
    constexpr uint32_t GOLD_BRIGHT    = rgba32(245, 215, 110);
    constexpr uint32_t GOLD_DARK      = rgba32(138, 100, 18);
    constexpr uint32_t FELT           = rgba32(18, 53, 36);
    constexpr uint32_t FELT_LIGHT     = rgba32(30, 82, 56);
    constexpr uint32_t CRIMSON        = rgba32(158, 27, 27);
    constexpr uint32_t CRIMSON_DARK   = rgba32(110, 18, 18);
    constexpr uint32_t INK            = rgba32(36, 26, 20);
    constexpr uint32_t INK_MUTED      = rgba32(104, 88, 74);
    constexpr uint32_t WHITE          = rgba32(255, 255, 255);
    constexpr uint32_t HP_GOOD        = rgba32(46, 139, 68);
    constexpr uint32_t HP_WARN        = rgba32(214, 146, 32);
    constexpr uint32_t HP_CRIT        = rgba32(178, 34, 34);
    constexpr uint32_t COND_BADGE     = rgba32(126, 58, 140);
    constexpr uint32_t PLAYER_TAG     = rgba32(38, 92, 148);
    constexpr uint32_t ACTIVE_ROW     = rgba32(252, 232, 166);
} // namespace theme

} // namespace dmtk
