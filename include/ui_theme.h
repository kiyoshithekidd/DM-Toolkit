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
    constexpr uint32_t AMBER_BG       = rgba32(252, 236, 194);
    constexpr uint32_t AMBER_HDR      = rgba32(168, 102, 18);
    constexpr uint32_t SPELL_BLUE     = rgba32(42, 74, 132);
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

struct DiceSkinPalette {
    const char* name;
    uint8_t     hiR, hiG, hiB;     // Lit facet color
    uint8_t     midR, midG, midB;  // Base die body color
    uint8_t     loR, loG, loB;     // Shaded facet color
    uint32_t    numColor;          // Engraved numeral color
    uint32_t    edgeColor;         // Facet bevel outline color
};

struct TraySkinPalette {
    const char* name;
    uint32_t    frameOuter;
    uint32_t    frameInner;
    uint32_t    trimColor;
    uint32_t    feltMain;
    uint32_t    feltDark;
};

constexpr int DICE_SKIN_COUNT = 8;
constexpr int TRAY_SKIN_COUNT = 8;

inline const DiceSkinPalette& getDiceSkin(int idx) {
    static const DiceSkinPalette kSkins[DICE_SKIN_COUNT] = {
        // Default: Royal Gold
        {"Royal Gold",      255, 232, 117,  212, 175,  55,  138, 100,  18, rgba32(28, 18, 8),     rgba32(92, 62, 10)},
        {"Obsidian Gold",    78,  78,  92,   36,  36,  44,   16,  16,  22, rgba32(245, 215, 110), rgba32(120, 96, 38)},
        {"Crimson Ruby",    230,  86,  86,  166,  32,  32,   88,  14,  14, rgba32(250, 243, 224), rgba32(62, 8, 8)},
        {"Arcane Sapphire", 108, 174, 242,   44,  96, 172,   18,  46,  92, rgba32(240, 248, 255), rgba32(14, 30, 64)},
        {"Ivory Bone",      255, 252, 240,  230, 218, 188,  176, 160, 126, rgba32(36, 26, 20),    rgba32(128, 110, 82)},
        {"Emerald Jade",    112, 220, 150,   38, 148,  84,   16,  86,  46, rgba32(246, 228, 152), rgba32(20, 74, 42)},
        {"Frost Silver",    242, 248, 255,  194, 210, 228,  132, 150, 172, rgba32(22, 34, 58),    rgba32(92, 112, 138)},
        {"Shadow Amethyst", 188, 128, 238,  118,  58, 176,   64,  24, 104, rgba32(250, 224, 132), rgba32(58, 24, 88)}
    };
    return kSkins[(idx >= 0 && idx < DICE_SKIN_COUNT) ? idx : 0];
}

inline const TraySkinPalette& getTraySkin(int idx) {
    static const TraySkinPalette kTrays[TRAY_SKIN_COUNT] = {
        // Default: Emerald Felt & Walnut
        {"Emerald Felt",    rgba32(32, 20, 12), rgba32(58, 36, 22), rgba32(212, 175, 55), rgba32(18, 56, 38), rgba32(12, 38, 25)},
        {"Crimson Velvet",  rgba32(24, 18, 20), rgba32(46, 32, 36), rgba32(212, 175, 55), rgba32(78, 18, 28), rgba32(52, 10, 18)},
        {"Royal Navy",      rgba32(48, 32, 18), rgba32(76, 52, 30), rgba32(198, 208, 224),rgba32(20, 40, 72), rgba32(12, 26, 48)},
        {"Tavern Leather",  rgba32(44, 24, 14), rgba32(74, 42, 24), rgba32(200, 155, 60), rgba32(70, 52, 38), rgba32(48, 34, 24)},
        {"Amethyst Slate",  rgba32(26, 28, 36), rgba32(44, 46, 58), rgba32(226, 194, 101),rgba32(48, 26, 72), rgba32(30, 16, 48)},
        {"Obsidian Marble", rgba32(18, 20, 24), rgba32(34, 38, 46), rgba32(184, 196, 212),rgba32(28, 32, 38), rgba32(16, 18, 24)},
        {"Sunlit Oak",      rgba32(68, 42, 20), rgba32(102, 66, 32),rgba32(230, 192, 76), rgba32(34, 74, 42), rgba32(20, 50, 28)},
        {"Abyssal Teal",    rgba32(20, 24, 28), rgba32(36, 44, 50), rgba32(196, 132, 82), rgba32(14, 58, 66), rgba32(8, 38, 44)}
    };
    return kTrays[(idx >= 0 && idx < TRAY_SKIN_COUNT) ? idx : 0];
}

} // namespace dmtk
