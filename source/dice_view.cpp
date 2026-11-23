#include "dice_view.h"
#include "ui_theme.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace dmtk {

namespace {

static size_t spriteIndexForShape(DieShape shape) {
    switch (shape) {
        case DIE_D4:   return 6;
        case DIE_D6:   return 7;
        case DIE_D8:   return 8;
        case DIE_D10:
        case DIE_D100: return 9;
        case DIE_D12:  return 10;
        case DIE_D20:
        default:       return 11;
    }
}

static uint32_t skinTintColor(const DiceSkinPalette& skin, bool dimmed) {
    float mult = dimmed ? 0.50f : 1.0f;
    uint8_t r = static_cast<uint8_t>(std::clamp(skin.midR * mult, 0.0f, 255.0f));
    uint8_t g = static_cast<uint8_t>(std::clamp(skin.midG * mult, 0.0f, 255.0f));
    uint8_t b = static_cast<uint8_t>(std::clamp(skin.midB * mult, 0.0f, 255.0f));
    return rgba32(r, g, b, 255);
}

static void drawD6Pips(float cx, float cy, float r, int val, uint32_t pipCol) {
    float d = r * 0.44f;
    float pr = std::max(2.2f, r * 0.14f);
    auto pip = [&](float ox, float oy) {
        C2D_DrawCircleSolid(cx + ox, cy + oy, 0.65f, pr, pipCol);
    };
    if (val == 1 || val == 3 || val == 5) pip(0, 0);
    if (val >= 2) { pip(-d, -d); pip(d, d); }
    if (val >= 4) { pip(d, -d); pip(-d, d); }
    if (val == 6) { pip(-d, 0); pip(d, 0); }
}

static void drawDieSprite(C2D_TextBuf buf, C2D_SpriteSheet sheet, const DieBody& die, int idx,
                          const DiceSkinPalette& skin, float sx, float sy,
                          float targetDiameter, bool dimmed) {
    if (!die.active) return;

    float heightScale = die.settled ? 1.0f : (1.0f + die.pos.z * 0.28f);
    float drawDiam = targetDiameter * heightScale;
    float scale = drawDiam / 96.0f;

    // Soft drop shadow on felt underneath the die
    float shadowOff = die.settled ? 4.0f : (5.0f + die.pos.z * 12.0f);
    float shadowRad = drawDiam * 0.40f;
    C2D_DrawEllipseSolid(sx - shadowRad + shadowOff * 0.45f,
                         sy - shadowRad * 0.85f + shadowOff,
                         0.25f, shadowRad * 2.0f, shadowRad * 1.7f,
                         rgba32(0, 0, 0, 105));

    float angle = 0.0f;
    if (!die.settled) {
        angle = static_cast<float>(die.framesRolling) * 0.28f + static_cast<float>(idx) * 0.9f;
    }

    if (sheet) {
        size_t sprIdx = spriteIndexForShape(die.shape);
        C2D_Image img = C2D_SpriteSheetGetImage(sheet, sprIdx);
        C2D_ImageTint tint;
        uint32_t tcol = skinTintColor(skin, dimmed);
        C2D_PlainImageTint(&tint, tcol, 0.72f);
        C2D_DrawImageAtRotated(img, sx, sy, 0.45f, angle, &tint, scale, scale);
    } else {
        C2D_DrawCircleSolid(sx, sy, 0.45f, drawDiam * 0.45f, skinTintColor(skin, dimmed));
    }

    // Determine displayed number / pip value
    int shownVal = die.settled
        ? die.result
        : (((die.framesRolling * 3 + die.result + idx) % std::max(1, die.sides)) + 1);
    if (die.shape == DIE_D100 && !die.settled) {
        shownVal = ((die.framesRolling * 3 + idx) % 10) * 10;
    }

    if (die.shape == DIE_D6 && shownVal >= 1 && shownVal <= 6 && die.settled) {
        uint32_t pipCol = dimmed ? theme::INK_MUTED : skin.numColor;
        if (shownVal == 6 && !dimmed) pipCol = theme::GOLD_BRIGHT;
        else if (shownVal == 1 && !dimmed) pipCol = theme::CRIMSON;
        drawD6Pips(sx, sy, drawDiam * 0.42f, shownVal, pipCol);
    } else {
        char numStr[8];
        if (die.shape == DIE_D100) {
            std::snprintf(numStr, sizeof(numStr), "%02d", shownVal);
        } else {
            std::snprintf(numStr, sizeof(numStr), "%d", shownVal);
        }

        C2D_Text txt;
        C2D_TextParse(&txt, buf, numStr);
        C2D_TextOptimize(&txt);
        float sc = (drawDiam >= 64.0f) ? 0.74f : (drawDiam >= 48.0f ? 0.58f : 0.46f);
        float tw = 0.0f, th = 0.0f;
        C2D_TextGetDimensions(&txt, sc, sc, &tw, &th);
        uint32_t ncol = dimmed ? theme::INK_MUTED : skin.numColor;
        if (die.settled && shownVal == die.sides && !dimmed && die.shape != DIE_D100) {
            ncol = theme::GOLD_BRIGHT;
        } else if (die.settled && shownVal == 1 && !dimmed && die.shape != DIE_D100) {
            ncol = theme::CRIMSON;
        }
        float yOff = (die.shape == DIE_D4) ? (drawDiam * 0.10f) : 0.0f;
        C2D_DrawText(&txt, C2D_WithColor, sx - tw * 0.5f, sy - th * 0.5f + yOff, 0.65f, sc, sc, ncol);
    }
}

} // namespace

void renderTopDiceTray(C2D_TextBuf buf, C2D_SpriteSheet sheet, const AppState& app) {
    const TraySkinPalette& tray = getTraySkin(app.traySkinIdx);
    const DiceSkinPalette& skin = getDiceSkin(app.diceSkinIdx);

    // Left 268px: Dice Tray (Wood frame + Gold/Silver trim + Felt bed)
    C2D_DrawRectSolid(0.0f, 0.0f, 0.05f, 268.0f, 240.0f, tray.frameOuter);
    C2D_DrawRectSolid(8.0f, 8.0f, 0.10f, 252.0f, 224.0f, tray.frameInner);
    C2D_DrawRectSolid(14.0f, 14.0f, 0.12f, 240.0f, 212.0f, tray.trimColor);
    C2D_DrawRectSolid(16.0f, 16.0f, 0.15f, 236.0f, 208.0f, tray.feltMain);
    // Inner felt shadow border
    C2D_DrawRectSolid(16.0f, 16.0f, 0.17f, 236.0f, 8.0f, tray.feltDark);
    C2D_DrawRectSolid(16.0f, 216.0f, 0.17f, 236.0f, 8.0f, tray.feltDark);
    C2D_DrawRectSolid(16.0f, 16.0f, 0.17f, 8.0f, 208.0f, tray.feltDark);
    C2D_DrawRectSolid(244.0f, 16.0f, 0.17f, 8.0f, 208.0f, tray.feltDark);

    // Subtle skin label in top-left corner of felt
    C2D_Text lbl;
    char trayInfo[64];
    std::snprintf(trayInfo, sizeof(trayInfo), "%s  |  %s", skin.name, tray.name);
    C2D_TextParse(&lbl, buf, trayInfo);
    C2D_TextOptimize(&lbl);
    C2D_DrawText(&lbl, C2D_WithColor, 26.0f, 24.0f, 0.55f, 0.38f, 0.38f, theme::GOLD_BRIGHT);

    int activeN = app.dice.activeDieCount();
    if (activeN <= 2) {
        // Hero Mode (1-2 dice): large studio-shaded polyhedral sprites with smooth physics motion
        const float cx = 134.0f;
        const float cy = 124.0f;
        const float scale = 72.0f;

        bool dimA = false, dimB = false;
        if (!app.dice.isRolling() && activeN == 2 && app.dice.currentShape() == DIE_D20) {
            if (app.dice.currentAdvMode() == ROLL_ADVANTAGE) {
                dimA = (app.dice.dieA().result < app.dice.dieB().result);
                dimB = (app.dice.dieB().result < app.dice.dieA().result);
            } else if (app.dice.currentAdvMode() == ROLL_DISADVANTAGE) {
                dimA = (app.dice.dieA().result > app.dice.dieB().result);
                dimB = (app.dice.dieB().result > app.dice.dieA().result);
            }
        }

        const DieBody& dA = app.dice.dieA();
        drawDieSprite(buf, sheet, dA, 0, skin, cx + dA.pos.x * scale, cy - dA.pos.y * scale, 84.0f, dimA);
        if (activeN == 2) {
            const DieBody& dB = app.dice.dieB();
            drawDieSprite(buf, sheet, dB, 1, skin, cx + dB.pos.x * scale, cy - dB.pos.y * scale, 78.0f, dimB);
        }
    } else {
        // Grid Mode (3-12 dice): crisp sprite grid with lively roll bounce
        int cols = (activeN <= 4) ? 2 : (activeN <= 6 ? 3 : 4);
        int rows = (activeN + cols - 1) / cols;
        float diam   = (activeN <= 4) ? 62.0f : (activeN <= 6 ? 54.0f : 44.0f);
        float cellW  = 212.0f / static_cast<float>(cols);
        float cellH  = 170.0f / static_cast<float>(rows);
        float startX = 28.0f + cellW * 0.5f;
        float startY = 44.0f + cellH * 0.5f;

        for (int i = 0; i < activeN; ++i) {
            const DieBody& d = app.dice.dieAt(i);
            int c = i % cols;
            int r = i / cols;
            float dx = startX + c * cellW;
            float dy = startY + r * cellH;
            if (!d.settled) {
                dx += std::sin(d.framesRolling * 0.50f + i * 1.7f) * 3.5f;
                dy += std::cos(d.framesRolling * 0.60f + i * 2.1f) * 3.5f;
            }
            drawDieSprite(buf, sheet, d, i, skin, dx, dy, diam, false);
        }
    }

    // Right 132px: Result Breakdown & Roll History
    C2D_DrawRectSolid(268.0f, 0.0f, 0.1f, 132.0f, 240.0f, theme::PARCHMENT);
    C2D_DrawRectSolid(268.0f, 0.0f, 0.15f, 132.0f, 26.0f, theme::WALNUT);

    C2D_Text hdr;
    C2D_TextParse(&hdr, buf, "DICE TRAY");
    C2D_TextOptimize(&hdr);
    C2D_DrawText(&hdr, C2D_WithColor, 276.0f, 5.0f, 0.55f, 0.46f, 0.46f, theme::GOLD_BRIGHT);

    char totalStr[24];
    if (app.dice.isRolling()) {
        std::snprintf(totalStr, sizeof(totalStr), "...");
    } else {
        std::snprintf(totalStr, sizeof(totalStr), "%d", app.dice.lastTotal());
    }
    C2D_Text totTxt;
    C2D_TextParse(&totTxt, buf, totalStr);
    C2D_TextOptimize(&totTxt);
    float tw = 0.0f, th = 0.0f;
    C2D_TextGetDimensions(&totTxt, 1.15f, 1.15f, &tw, &th);
    C2D_DrawText(&totTxt, C2D_WithColor, 268.0f + (132.0f - tw) * 0.5f, 34.0f, 0.55f, 1.15f, 1.15f, theme::CRIMSON_DARK);

    if (!app.dice.history().empty() && !app.dice.isRolling()) {
        const auto& latest = app.dice.history()[0];
        C2D_Text bk;
        C2D_TextParse(&bk, buf, latest.label.c_str());
        C2D_TextOptimize(&bk);
        C2D_DrawText(&bk, C2D_WithColor, 274.0f, 74.0f, 0.55f, 0.38f, 0.38f, theme::INK);

        if (latest.rolls.size() > 1) {
            std::string rollList = "[";
            for (size_t i = 0; i < latest.rolls.size(); ++i) {
                if (i > 0) rollList += ",";
                rollList += std::to_string(latest.rolls[i]);
            }
            rollList += "]";
            C2D_Text rl;
            C2D_TextParse(&rl, buf, rollList.c_str());
            C2D_TextOptimize(&rl);
            C2D_DrawText(&rl, C2D_WithColor, 274.0f, 90.0f, 0.55f, 0.38f, 0.38f, theme::INK_MUTED);
        }
    }

    C2D_DrawRectSolid(274.0f, 108.0f, 0.2f, 120.0f, 1.0f, theme::PARCHMENT_EDGE);
    C2D_Text hTitle;
    C2D_TextParse(&hTitle, buf, "Recent Rolls:");
    C2D_TextOptimize(&hTitle);
    C2D_DrawText(&hTitle, C2D_WithColor, 274.0f, 113.0f, 0.55f, 0.40f, 0.40f, theme::INK_MUTED);

    float hy = 130.0f;
    for (size_t i = 0; i < app.dice.history().size() && i < 6; ++i) {
        C2D_Text ht;
        C2D_TextParse(&ht, buf, app.dice.history()[i].label.c_str());
        C2D_TextOptimize(&ht);
        C2D_DrawText(&ht, C2D_WithColor, 274.0f, hy, 0.55f, 0.38f, 0.38f,
                     (i == 0) ? theme::INK : theme::INK_MUTED);
        hy += 17.0f;
    }
}

} // namespace dmtk
