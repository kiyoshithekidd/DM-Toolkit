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

static uint32_t scaleSkinColor(uint8_t r0, uint8_t g0, uint8_t b0, float mult) {
    uint8_t r = static_cast<uint8_t>(std::clamp(r0 * mult, 0.0f, 255.0f));
    uint8_t g = static_cast<uint8_t>(std::clamp(g0 * mult, 0.0f, 255.0f));
    uint8_t b = static_cast<uint8_t>(std::clamp(b0 * mult, 0.0f, 255.0f));
    return rgba32(r, g, b, 255);
}

static void drawD6Pips(float cx, float cy, float r, int val, uint32_t pipCol, float angle) {
    float d = r * 0.44f;
    float pr = std::max(2.2f, r * 0.14f);
    float ca = std::cos(angle), sa = std::sin(angle);
    auto pip = [&](float ox, float oy) {
        float rx = cx + ox * ca - oy * sa;
        float ry = cy + ox * sa + oy * ca;
        C2D_DrawCircleSolid(rx + 0.8f, ry + 0.8f, 0.62f, pr, rgba32(0, 0, 0, 120));
        C2D_DrawCircleSolid(rx, ry, 0.65f, pr, pipCol);
    };
    if (val == 1 || val == 3 || val == 5) pip(0, 0);
    if (val >= 2) { pip(-d, -d); pip(d, d); }
    if (val >= 4) { pip(d, -d); pip(-d, d); }
    if (val == 6) { pip(-d, 0); pip(d, 0); }
}

static void drawPolyNgon(float cx, float cy, float z, float r, int sides, float angle,
                         uint32_t hiCol, uint32_t midCol, uint32_t loCol, uint32_t edgeCol) {
    // Outer edge silhouette
    for (int i = 0; i < sides; ++i) {
        float a0 = angle + (static_cast<float>(i) * 2.0f * 3.1415926f) / sides - 1.5707963f;
        float a1 = angle + (static_cast<float>(i + 1) * 2.0f * 3.1415926f) / sides - 1.5707963f;
        float x0 = cx + std::cos(a0) * (r + 1.5f), y0 = cy + std::sin(a0) * (r + 1.5f);
        float x1 = cx + std::cos(a1) * (r + 1.5f), y1 = cy + std::sin(a1) * (r + 1.5f);
        C2D_DrawTriangle(cx, cy, edgeCol, x0, y0, edgeCol, x1, y1, edgeCol, z - 0.02f);
    }
    // Outer shaded facets
    for (int i = 0; i < sides; ++i) {
        float a0 = angle + (static_cast<float>(i) * 2.0f * 3.1415926f) / sides - 1.5707963f;
        float a1 = angle + (static_cast<float>(i + 1) * 2.0f * 3.1415926f) / sides - 1.5707963f;
        float midA = (a0 + a1) * 0.5f;
        // Top-left lit, bottom-right shaded
        float light = -std::cos(midA) * 0.45f - std::sin(midA) * 0.85f;
        uint32_t fcol = (light > 0.25f) ? hiCol : (light < -0.20f ? loCol : midCol);
        float x0 = cx + std::cos(a0) * r, y0 = cy + std::sin(a0) * r;
        float x1 = cx + std::cos(a1) * r, y1 = cy + std::sin(a1) * r;
        C2D_DrawTriangle(cx, cy, fcol, x0, y0, fcol, x1, y1, fcol, z);
        C2D_DrawLine(cx, cy, edgeCol, x0, y0, edgeCol, 1.0f, z + 0.02f);
        C2D_DrawLine(x0, y0, edgeCol, x1, y1, edgeCol, 1.2f, z + 0.02f);
    }
}

static void drawDieSprite(C2D_TextBuf buf, C2D_SpriteSheet /*sheet*/, const DieBody& die, int idx,
                          const DiceSkinPalette& skin, float sx, float sy,
                          float targetDiameter, bool dimmed) {
    if (!die.active) return;

    float heightScale = die.settled ? 1.0f : (1.0f + die.pos.z * 0.28f);
    float drawDiam = targetDiameter * heightScale;
    float R = drawDiam * 0.46f;

    // Soft drop shadow on felt underneath the die
    float shadowOff = die.settled ? 4.0f : (5.0f + die.pos.z * 12.0f);
    float shadowRad = drawDiam * 0.40f;
    C2D_DrawEllipseSolid(sx - shadowRad + shadowOff * 0.45f,
                         sy - shadowRad * 0.85f + shadowOff,
                         0.25f, shadowRad * 2.0f, shadowRad * 1.7f,
                         rgba32(0, 0, 0, 110));

    float angle = 0.0f;
    if (!die.settled) {
        angle = static_cast<float>(die.framesRolling) * 0.28f + static_cast<float>(idx) * 0.9f;
    } else {
        // Subtle natural resting angle per die
        angle = ((idx % 2 == 0) ? 0.05f : -0.05f);
    }

    float mult = dimmed ? 0.50f : 1.0f;
    uint32_t hiCol   = scaleSkinColor(skin.hiR,  skin.hiG,  skin.hiB,  mult);
    uint32_t midCol  = scaleSkinColor(skin.midR, skin.midG, skin.midB, mult);
    uint32_t loCol   = scaleSkinColor(skin.loR,  skin.loG,  skin.loB,  mult);
    uint32_t edgeCol = dimmed ? rgba32(30, 24, 20, 255) : skin.edgeColor;

    const float z = 0.45f;
    if (die.shape == DIE_D4) {
        drawPolyNgon(sx, sy + R * 0.12f, z, R * 1.05f, 3, angle, hiCol, midCol, loCol, edgeCol);
    } else if (die.shape == DIE_D6) {
        float hs = R * 0.84f;
        float ca = std::cos(angle), sa = std::sin(angle);
        auto rotPt = [&](float lx, float ly, float& ox, float& oy) {
            ox = sx + lx * ca - ly * sa;
            oy = sy + lx * sa + ly * ca;
        };
        float x0, y0, x1, y1, x2, y2, x3, y3;
        float bx0, by0, bx1, by1, bx2, by2, bx3, by3;
        rotPt(-hs, -hs, x0, y0); rotPt(hs, -hs, x1, y1);
        rotPt(hs, hs, x2, y2);   rotPt(-hs, hs, x3, y3);
        float b = hs + 2.0f;
        rotPt(-b, -b, bx0, by0); rotPt(b, -b, bx1, by1);
        rotPt(b, b, bx2, by2);   rotPt(-b, b, bx3, by3);
        C2D_DrawTriangle(bx0, by0, edgeCol, bx1, by1, edgeCol, bx2, by2, edgeCol, z - 0.02f);
        C2D_DrawTriangle(bx0, by0, edgeCol, bx2, by2, edgeCol, bx3, by3, edgeCol, z - 0.02f);
        C2D_DrawTriangle(x0, y0, hiCol, x1, y1, midCol, x2, y2, loCol, z);
        C2D_DrawTriangle(x0, y0, hiCol, x2, y2, loCol, x3, y3, midCol, z);
        float in = hs * 0.78f;
        float ix0, iy0, ix1, iy1, ix2, iy2, ix3, iy3;
        rotPt(-in, -in, ix0, iy0); rotPt(in, -in, ix1, iy1);
        rotPt(in, in, ix2, iy2);   rotPt(-in, in, ix3, iy3);
        C2D_DrawTriangle(ix0, iy0, midCol, ix1, iy1, midCol, ix2, iy2, midCol, z + 0.02f);
        C2D_DrawTriangle(ix0, iy0, midCol, ix2, iy2, midCol, ix3, iy3, midCol, z + 0.02f);
        C2D_DrawLine(ix0, iy0, edgeCol, ix1, iy1, edgeCol, 1.0f, z + 0.03f);
        C2D_DrawLine(ix1, iy1, edgeCol, ix2, iy2, edgeCol, 1.0f, z + 0.03f);
        C2D_DrawLine(ix2, iy2, edgeCol, ix3, iy3, edgeCol, 1.0f, z + 0.03f);
        C2D_DrawLine(ix3, iy3, edgeCol, ix0, iy0, edgeCol, 1.0f, z + 0.03f);
    } else if (die.shape == DIE_D8) {
        drawPolyNgon(sx, sy, z, R, 4, angle, hiCol, midCol, loCol, edgeCol);
        drawPolyNgon(sx, sy, z + 0.04f, R * 0.56f, 4, angle + 0.7854f, hiCol, hiCol, midCol, edgeCol);
    } else if (die.shape == DIE_D10 || die.shape == DIE_D100) {
        // Classic d10 kite with 4 outer facets and central kite face
        float ca = std::cos(angle), sa = std::sin(angle);
        auto rotPt = [&](float lx, float ly, float& ox, float& oy) {
            ox = sx + lx * ca - ly * sa;
            oy = sy + lx * sa + ly * ca;
        };
        float tx, ty, rx, ry, bx, by, lx, ly;
        rotPt(0.0f, -R, tx, ty);
        rotPt(R * 0.88f, -R * 0.08f, rx, ry);
        rotPt(0.0f, R, bx, by);
        rotPt(-R * 0.88f, -R * 0.08f, lx, ly);
        C2D_DrawTriangle(sx, sy, hiCol, lx, ly, hiCol, tx, ty, hiCol, z);
        C2D_DrawTriangle(sx, sy, midCol, tx, ty, midCol, rx, ry, midCol, z);
        C2D_DrawTriangle(sx, sy, loCol, rx, ry, loCol, bx, by, loCol, z);
        C2D_DrawTriangle(sx, sy, midCol, bx, by, midCol, lx, ly, midCol, z);
        C2D_DrawLine(tx, ty, edgeCol, rx, ry, edgeCol, 1.4f, z + 0.03f);
        C2D_DrawLine(rx, ry, edgeCol, bx, by, edgeCol, 1.4f, z + 0.03f);
        C2D_DrawLine(bx, by, edgeCol, lx, ly, edgeCol, 1.4f, z + 0.03f);
        C2D_DrawLine(lx, ly, edgeCol, tx, ty, edgeCol, 1.4f, z + 0.03f);
        // Inner kite face
        float itx, ity, irx, iry, ibx, iby, ilx, ily;
        rotPt(0.0f, -R * 0.54f, itx, ity);
        rotPt(R * 0.46f, -R * 0.05f, irx, iry);
        rotPt(0.0f, R * 0.54f, ibx, iby);
        rotPt(-R * 0.46f, -R * 0.05f, ilx, ily);
        C2D_DrawTriangle(itx, ity, hiCol, irx, iry, midCol, ibx, iby, midCol, z + 0.04f);
        C2D_DrawTriangle(itx, ity, hiCol, ibx, iby, midCol, ilx, ily, hiCol, z + 0.04f);
        C2D_DrawLine(itx, ity, edgeCol, irx, iry, edgeCol, 1.0f, z + 0.05f);
        C2D_DrawLine(irx, iry, edgeCol, ibx, iby, edgeCol, 1.0f, z + 0.05f);
        C2D_DrawLine(ibx, iby, edgeCol, ilx, ily, edgeCol, 1.0f, z + 0.05f);
        C2D_DrawLine(ilx, ily, edgeCol, itx, ity, edgeCol, 1.0f, z + 0.05f);
    } else if (die.shape == DIE_D12) {
        drawPolyNgon(sx, sy, z, R, 5, angle, hiCol, midCol, loCol, edgeCol);
        drawPolyNgon(sx, sy, z + 0.04f, R * 0.58f, 5, angle + 3.14159f, hiCol, hiCol, midCol, edgeCol);
    } else {
        // DIE_D20: Hexagon outer icosahedron + central lit equilateral triangle face
        drawPolyNgon(sx, sy, z, R, 6, angle, hiCol, midCol, loCol, edgeCol);
        drawPolyNgon(sx, sy, z + 0.04f, R * 0.58f, 3, angle, hiCol, hiCol, midCol, edgeCol);
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
        drawD6Pips(sx, sy, R * 0.85f, shownVal, pipCol, angle);
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
        float sc = (drawDiam >= 64.0f) ? 0.68f : (drawDiam >= 48.0f ? 0.54f : 0.44f);
        float tw = 0.0f, th = 0.0f;
        C2D_TextGetDimensions(&txt, sc, sc, &tw, &th);
        uint32_t ncol = dimmed ? theme::INK_MUTED : skin.numColor;
        if (die.settled && shownVal == die.sides && !dimmed && die.shape != DIE_D100) {
            ncol = theme::GOLD_BRIGHT;
        } else if (die.settled && shownVal == 1 && !dimmed && die.shape != DIE_D100) {
            ncol = theme::CRIMSON;
        }
        float yOff = (die.shape == DIE_D4 || die.shape == DIE_D20) ? (drawDiam * 0.06f) : 0.0f;
        float tx = sx - tw * 0.5f;
        float ty = sy - th * 0.5f + yOff;
        // Engraved shadow beneath numeral so it sits on the top facet
        C2D_DrawText(&txt, C2D_WithColor, tx + 1.0f, ty + 1.0f, 0.62f, sc, sc, rgba32(0, 0, 0, 185));
        C2D_DrawText(&txt, C2D_WithColor, tx, ty, 0.65f, sc, sc, ncol);
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
