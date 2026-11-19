#include "dice_view.h"
#include "ui_theme.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace dmtk {

namespace {

struct TriMesh {
    std::vector<Vec3> verts;
    std::vector<int>  indices; // groups of 3
};

static Vec3 vNorm(float x, float y, float z) {
    float l = std::sqrt(x * x + y * y + z * z);
    if (l < 1e-6f) return {0.0f, 0.0f, 1.0f};
    return {x / l, y / l, z / l};
}

static void buildDieMesh(DieShape shape, TriMesh& mesh) {
    mesh.verts.clear();
    mesh.indices.clear();
    DieShape base = (shape == DIE_D100) ? DIE_D10 : shape;

    switch (base) {
        case DIE_D4: {
            mesh.verts = {
                vNorm( 1,  1,  1), vNorm(-1, -1,  1),
                vNorm(-1,  1, -1), vNorm( 1, -1, -1)
            };
            mesh.indices = {0,1,2, 0,3,1, 0,2,3, 1,3,2};
            break;
        }
        case DIE_D6: {
            const float s = 0.62f;
            mesh.verts = {
                {-s,-s, s}, { s,-s, s}, { s, s, s}, {-s, s, s},
                {-s,-s,-s}, { s,-s,-s}, { s, s,-s}, {-s, s,-s}
            };
            mesh.indices = {
                0,1,2, 0,2,3,  // +Z
                5,4,7, 5,7,6,  // -Z
                1,5,6, 1,6,2,  // +X
                4,0,3, 4,3,7,  // -X
                3,2,6, 3,6,7,  // +Y
                4,5,1, 4,1,0   // -Y
            };
            break;
        }
        case DIE_D8: {
            mesh.verts = {
                {0,0,1}, {1,0,0}, {0,1,0}, {-1,0,0}, {0,-1,0}, {0,0,-1}
            };
            mesh.indices = {
                0,1,2, 0,2,3, 0,3,4, 0,4,1,
                5,2,1, 5,3,2, 5,4,3, 5,1,4
            };
            break;
        }
        case DIE_D10: {
            const float pi = 3.14159265f;
            mesh.verts.push_back({0.0f, 0.0f, 1.0f});  // 0: top pole
            mesh.verts.push_back({0.0f, 0.0f, -1.0f}); // 1: bottom pole
            for (int i = 0; i < 10; ++i) {
                float a = (2.0f * pi * i) / 10.0f;
                float z = (i % 2 == 0) ? 0.18f : -0.18f;
                mesh.verts.push_back(vNorm(std::cos(a), std::sin(a), z));
            }
            for (int i = 0; i < 10; ++i) {
                int a = 2 + i;
                int b = 2 + ((i + 1) % 10);
                mesh.indices.push_back(0); mesh.indices.push_back(a); mesh.indices.push_back(b);
                mesh.indices.push_back(1); mesh.indices.push_back(b); mesh.indices.push_back(a);
            }
            break;
        }
        case DIE_D12:
        case DIE_D20:
        default: {
            const float phi = 1.61803398875f;
            mesh.verts = {
                vNorm(-1,  phi, 0), vNorm( 1,  phi, 0), vNorm(-1, -phi, 0), vNorm( 1, -phi, 0),
                vNorm( 0, -1,  phi), vNorm( 0,  1,  phi), vNorm( 0, -1, -phi), vNorm( 0,  1, -phi),
                vNorm( phi, 0, -1), vNorm( phi, 0,  1), vNorm(-phi, 0, -1), vNorm(-phi, 0,  1)
            };
            mesh.indices = {
                0,11,5, 0,5,1, 0,1,7, 0,7,10, 0,10,11,
                1,5,9, 5,11,4, 11,10,2, 10,7,6, 7,1,8,
                3,9,4, 3,4,2, 3,2,6, 3,6,8, 3,8,9,
                4,9,5, 2,4,11, 6,2,10, 8,6,7, 9,8,1
            };
            break;
        }
    }
}

static uint32_t shadeSkin(const DiceSkinPalette& skin, float ndl, bool dimmed) {
    float t = std::clamp(ndl, 0.0f, 1.0f);
    float r, g, b;
    if (t >= 0.5f) {
        float u = (t - 0.5f) * 2.0f;
        r = skin.midR + (skin.hiR - skin.midR) * u;
        g = skin.midG + (skin.hiG - skin.midG) * u;
        b = skin.midB + (skin.hiB - skin.midB) * u;
    } else {
        float u = t * 2.0f;
        r = skin.loR + (skin.midR - skin.loR) * u;
        g = skin.loG + (skin.midG - skin.loG) * u;
        b = skin.loB + (skin.midB - skin.loB) * u;
    }
    if (dimmed) {
        r *= 0.55f;
        g *= 0.55f;
        b *= 0.55f;
    }
    return rgba32(static_cast<uint8_t>(std::clamp(r, 0.0f, 255.0f)),
                  static_cast<uint8_t>(std::clamp(g, 0.0f, 255.0f)),
                  static_cast<uint8_t>(std::clamp(b, 0.0f, 255.0f)), 255);
}

static void drawDie3D(C2D_TextBuf buf, const DieBody& die, const DiceSkinPalette& skin,
                      float trayCenterX, float trayCenterY, float trayScale, bool dimmed) {
    if (!die.active) return;

    float sx = trayCenterX + die.pos.x * trayScale;
    float sy = trayCenterY - die.pos.y * trayScale;
    float heightScale = 1.0f + die.pos.z * 0.32f;
    float radiusPx = 34.0f * heightScale;

    // Soft drop shadow on felt underneath the die
    float shadowOff = 6.0f + die.pos.z * 14.0f;
    float shadowRad = 32.0f * (1.0f - std::min(0.35f, die.pos.z * 0.25f));
    C2D_DrawEllipseSolid(sx - shadowRad + shadowOff * 0.5f,
                         sy - shadowRad * 0.85f + shadowOff,
                         0.25f, shadowRad * 2.0f, shadowRad * 1.7f,
                         rgba32(0, 0, 0, 105));

    TriMesh mesh;
    buildDieMesh(die.shape, mesh);

    std::vector<Vec3> rotVerts(mesh.verts.size());
    for (size_t i = 0; i < mesh.verts.size(); ++i) {
        rotVerts[i] = DiceRoller::rotateVec(die.rot, mesh.verts[i]);
    }

    struct DrawTri {
        float x0, y0, x1, y1, x2, y2;
        float avgZ;
        uint32_t col;
    };
    std::vector<DrawTri> tris;
    Vec3 lightDir = vNorm(-0.35f, 0.45f, 0.82f);

    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const Vec3& a = rotVerts[static_cast<size_t>(mesh.indices[i])];
        const Vec3& b = rotVerts[static_cast<size_t>(mesh.indices[i + 1])];
        const Vec3& c = rotVerts[static_cast<size_t>(mesh.indices[i + 2])];

        // Compute outward normal
        Vec3 ab{b.x - a.x, b.y - a.y, b.z - a.z};
        Vec3 ac{c.x - a.x, c.y - a.y, c.z - a.z};
        Vec3 n{
            ab.y * ac.z - ab.z * ac.y,
            ab.z * ac.x - ab.x * ac.z,
            ab.x * ac.y - ab.y * ac.x
        };
        // Ensure outward winding relative to centroid
        Vec3 cent{(a.x + b.x + c.x) / 3.0f, (a.y + b.y + c.y) / 3.0f, (a.z + b.z + c.z) / 3.0f};
        if (n.x * cent.x + n.y * cent.y + n.z * cent.z < 0.0f) {
            n = {-n.x, -n.y, -n.z};
        }
        n = vNorm(n.x, n.y, n.z);

        // Back-face cull facets facing away from camera (+Z)
        if (n.z <= 0.02f) continue;

        float ndl = 0.25f + 0.75f * std::max(0.0f, n.x * lightDir.x + n.y * lightDir.y + n.z * lightDir.z);
        DrawTri dt;
        dt.x0 = sx + a.x * radiusPx;
        dt.y0 = sy - a.y * radiusPx;
        dt.x1 = sx + b.x * radiusPx;
        dt.y1 = sy - b.y * radiusPx;
        dt.x2 = sx + c.x * radiusPx;
        dt.y2 = sy - c.y * radiusPx;
        dt.avgZ = cent.z;
        dt.col = shadeSkin(skin, ndl, dimmed);
        tris.push_back(dt);
    }

    std::sort(tris.begin(), tris.end(), [](const DrawTri& a, const DrawTri& b) {
        return a.avgZ < b.avgZ;
    });

    for (const auto& t : tris) {
        C2D_DrawTriangle(t.x0, t.y0, t.col, t.x1, t.y1, t.col, t.x2, t.y2, t.col, 0.45f);
        C2D_DrawLine(t.x0, t.y0, skin.edgeColor, t.x1, t.y1, skin.edgeColor, 1.0f, 0.48f);
        C2D_DrawLine(t.x1, t.y1, skin.edgeColor, t.x2, t.y2, skin.edgeColor, 1.0f, 0.48f);
        C2D_DrawLine(t.x2, t.y2, skin.edgeColor, t.x0, t.y0, skin.edgeColor, 1.0f, 0.48f);
    }

    // Draw the rolled face number cleanly on the die center (animated while rolling, exact on settle)
    char numStr[8];
    if (die.settled) {
        if (die.shape == DIE_D100) {
            std::snprintf(numStr, sizeof(numStr), "%02d", die.result);
        } else {
            std::snprintf(numStr, sizeof(numStr), "%d", die.result);
        }
    } else {
        int spinVal = ((die.framesRolling * 3 + die.result) % std::max(1, die.sides)) + 1;
        std::snprintf(numStr, sizeof(numStr), "%d", spinVal);
    }

    C2D_Text txt;
    C2D_TextParse(&txt, buf, numStr);
    C2D_TextOptimize(&txt);
    float sc = die.settled ? 0.78f : 0.62f;
    float tw = 0.0f, th = 0.0f;
    C2D_TextGetDimensions(&txt, sc, sc, &tw, &th);
    uint32_t ncol = dimmed ? theme::INK_MUTED : skin.numColor;
    if (die.settled && die.shape == DIE_D20 && die.result == 20 && !dimmed) {
        ncol = theme::GOLD_BRIGHT;
    } else if (die.settled && die.shape == DIE_D20 && die.result == 1 && !dimmed) {
        ncol = theme::CRIMSON;
    }
    C2D_DrawText(&txt, C2D_WithColor, sx - tw * 0.5f, sy - th * 0.5f, 0.65f, sc, sc, ncol);
}

} // namespace

void renderTopDiceTray(C2D_TextBuf buf, const AppState& app) {
    const TraySkinPalette& tray = getTraySkin(app.traySkinIdx);
    const DiceSkinPalette& skin = getDiceSkin(app.diceSkinIdx);

    // Left 266px: Dice Tray (Wood frame + Gold/Silver trim + Felt bed)
    C2D_DrawRectSolid(0.0f, 0.0f, 0.05f, 268.0f, 240.0f, tray.frameOuter);
    C2D_DrawRectSolid(8.0f, 8.0f, 0.10f, 252.0f, 224.0f, tray.frameInner);
    C2D_DrawRectSolid(14.0f, 14.0f, 0.12f, 240.0f, 212.0f, tray.trimColor);
    C2D_DrawRectSolid(16.0f, 16.0f, 0.15f, 236.0f, 208.0f, tray.feltMain);
    // Inner felt shadow border
    C2D_DrawRectSolid(16.0f, 16.0f, 0.17f, 236.0f, 8.0f, tray.feltDark);
    C2D_DrawRectSolid(16.0f, 216.0f, 0.17f, 236.0f, 8.0f, tray.feltDark);
    C2D_DrawRectSolid(16.0f, 16.0f, 0.17f, 8.0f, 208.0f, tray.feltDark);
    C2D_DrawRectSolid(244.0f, 16.0f, 0.17f, 8.0f, 208.0f, tray.feltDark);

    // Subtle tray skin label in top-left corner of felt
    C2D_Text lbl;
    char trayInfo[64];
    std::snprintf(trayInfo, sizeof(trayInfo), "%s  |  %s", skin.name, tray.name);
    C2D_TextParse(&lbl, buf, trayInfo);
    C2D_TextOptimize(&lbl);
    C2D_DrawText(&lbl, C2D_WithColor, 26.0f, 24.0f, 0.55f, 0.32f, 0.32f, theme::GOLD_BRIGHT);

    // Render 3D polyhedral die(s)
    const float cx = 134.0f;
    const float cy = 124.0f;
    const float scale = 72.0f;

    bool dimA = false, dimB = false;
    if (!app.dice.isRolling() && app.dice.dieB().active && app.dice.currentShape() == DIE_D20) {
        if (app.dice.currentAdvMode() == ROLL_ADVANTAGE) {
            dimA = (app.dice.dieA().result < app.dice.dieB().result);
            dimB = (app.dice.dieB().result < app.dice.dieA().result);
        } else if (app.dice.currentAdvMode() == ROLL_DISADVANTAGE) {
            dimA = (app.dice.dieA().result > app.dice.dieB().result);
            dimB = (app.dice.dieB().result > app.dice.dieA().result);
        }
    }

    drawDie3D(buf, app.dice.dieA(), skin, cx, cy, scale, dimA);
    drawDie3D(buf, app.dice.dieB(), skin, cx, cy, scale, dimB);

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
    C2D_DrawText(&totTxt, C2D_WithColor, 268.0f + (132.0f - tw) * 0.5f, 36.0f, 0.55f, 1.15f, 1.15f, theme::CRIMSON_DARK);

    if (!app.dice.history().empty() && !app.dice.isRolling()) {
        C2D_Text bk;
        C2D_TextParse(&bk, buf, app.dice.history()[0].label.c_str());
        C2D_TextOptimize(&bk);
        C2D_DrawText(&bk, C2D_WithColor, 274.0f, 78.0f, 0.55f, 0.34f, 0.34f, theme::INK);
    }

    C2D_DrawRectSolid(274.0f, 98.0f, 0.2f, 120.0f, 1.0f, theme::PARCHMENT_EDGE);
    C2D_Text hTitle;
    C2D_TextParse(&hTitle, buf, "Recent Rolls:");
    C2D_TextOptimize(&hTitle);
    C2D_DrawText(&hTitle, C2D_WithColor, 274.0f, 104.0f, 0.55f, 0.36f, 0.36f, theme::INK_MUTED);

    float hy = 122.0f;
    for (size_t i = 0; i < app.dice.history().size() && i < 6; ++i) {
        C2D_Text ht;
        C2D_TextParse(&ht, buf, app.dice.history()[i].label.c_str());
        C2D_TextOptimize(&ht);
        C2D_DrawText(&ht, C2D_WithColor, 274.0f, hy, 0.55f, 0.33f, 0.33f,
                     (i == 0) ? theme::INK : theme::INK_MUTED);
        hy += 17.0f;
    }
}

} // namespace dmtk
