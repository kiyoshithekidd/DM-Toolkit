#include "dice_view.h"
#include "ui_theme.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace dmtk {

namespace {

struct PolyFace {
    std::vector<int> verts;
};

struct PolyMesh {
    std::vector<Vec3>     verts;
    std::vector<PolyFace> faces;
};

static Vec3 vNorm(float x, float y, float z) {
    float l = std::sqrt(x * x + y * y + z * z);
    if (l < 1e-6f) return {0.0f, 0.0f, 1.0f};
    return {x / l, y / l, z / l};
}

static void buildDieMesh(DieShape shape, PolyMesh& mesh) {
    mesh.verts.clear();
    mesh.faces.clear();
    DieShape base = (shape == DIE_D100) ? DIE_D10 : shape;

    switch (base) {
        case DIE_D4: {
            mesh.verts = {
                vNorm( 1,  1,  1), vNorm(-1, -1,  1),
                vNorm(-1,  1, -1), vNorm( 1, -1, -1)
            };
            mesh.faces = {{{0,1,2}}, {{0,3,1}}, {{0,2,3}}, {{1,3,2}}};
            break;
        }
        case DIE_D6: {
            const float s = 0.60f;
            mesh.verts = {
                {-s,-s, s}, { s,-s, s}, { s, s, s}, {-s, s, s},
                {-s,-s,-s}, { s,-s,-s}, { s, s,-s}, {-s, s,-s}
            };
            mesh.faces = {
                {{0,1,2,3}}, // +Z
                {{5,4,7,6}}, // -Z
                {{1,5,6,2}}, // +X
                {{4,0,3,7}}, // -X
                {{3,2,6,7}}, // +Y
                {{4,5,1,0}}  // -Y
            };
            break;
        }
        case DIE_D8: {
            mesh.verts = {
                {0,0,1}, {1,0,0}, {0,1,0}, {-1,0,0}, {0,-1,0}, {0,0,-1}
            };
            mesh.faces = {
                {{0,1,2}}, {{0,2,3}}, {{0,3,4}}, {{0,4,1}},
                {{5,2,1}}, {{5,3,2}}, {{5,4,3}}, {{5,1,4}}
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
                mesh.faces.push_back({{0, a, b}});
                mesh.faces.push_back({{1, b, a}});
            }
            break;
        }
        case DIE_D12: {
            // Regular dodecahedron: 20 vertices, 12 pentagonal faces
            const float phi = 1.61803398875f;
            const float inv = 1.0f / phi;
            mesh.verts = {
                vNorm( 1,  1,  1), vNorm( 1,  1, -1), vNorm( 1, -1,  1), vNorm( 1, -1, -1), // 0..3
                vNorm(-1,  1,  1), vNorm(-1,  1, -1), vNorm(-1, -1,  1), vNorm(-1, -1, -1), // 4..7
                vNorm(0,  inv,  phi), vNorm(0,  inv, -phi), vNorm(0, -inv,  phi), vNorm(0, -inv, -phi), // 8..11
                vNorm( inv,  phi, 0), vNorm( inv, -phi, 0), vNorm(-inv,  phi, 0), vNorm(-inv, -phi, 0), // 12..15
                vNorm( phi, 0,  inv), vNorm( phi, 0, -inv), vNorm(-phi, 0,  inv), vNorm(-phi, 0, -inv)  // 16..19
            };
            mesh.faces = {
                {{0, 8, 10, 2, 16}},
                {{0, 16, 17, 1, 12}},
                {{0, 12, 14, 4, 8}},
                {{4, 14, 5, 19, 18}},
                {{4, 18, 6, 10, 8}},
                {{2, 10, 6, 15, 13}},
                {{2, 13, 3, 17, 16}},
                {{1, 17, 3, 11, 9}},
                {{1, 9, 5, 14, 12}},
                {{7, 11, 3, 13, 15}},
                {{7, 15, 6, 18, 19}},
                {{7, 19, 5, 9, 11}}
            };
            break;
        }
        case DIE_D20:
        default: {
            const float phi = 1.61803398875f;
            mesh.verts = {
                vNorm(-1,  phi, 0), vNorm( 1,  phi, 0), vNorm(-1, -phi, 0), vNorm( 1, -phi, 0),
                vNorm( 0, -1,  phi), vNorm( 0,  1,  phi), vNorm( 0, -1, -phi), vNorm( 0,  1, -phi),
                vNorm( phi, 0, -1), vNorm( phi, 0,  1), vNorm(-phi, 0, -1), vNorm(-phi, 0,  1)
            };
            mesh.faces = {
                {{0,11,5}}, {{0,5,1}}, {{0,1,7}}, {{0,7,10}}, {{0,10,11}},
                {{1,5,9}}, {{5,11,4}}, {{11,10,2}}, {{10,7,6}}, {{7,1,8}},
                {{3,9,4}}, {{3,4,2}}, {{3,2,6}}, {{3,6,8}}, {{3,8,9}},
                {{4,9,5}}, {{2,4,11}}, {{6,2,10}}, {{8,6,7}}, {{9,8,1}}
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
    float radiusPx = 35.0f * heightScale;

    // Soft drop shadow on felt underneath the die
    float shadowOff = 6.0f + die.pos.z * 14.0f;
    float shadowRad = 32.0f * (1.0f - std::min(0.35f, die.pos.z * 0.25f));
    C2D_DrawEllipseSolid(sx - shadowRad + shadowOff * 0.5f,
                         sy - shadowRad * 0.85f + shadowOff,
                         0.25f, shadowRad * 2.0f, shadowRad * 1.7f,
                         rgba32(0, 0, 0, 105));

    PolyMesh mesh;
    buildDieMesh(die.shape, mesh);

    std::vector<Vec3> rotVerts(mesh.verts.size());
    for (size_t i = 0; i < mesh.verts.size(); ++i) {
        rotVerts[i] = DiceRoller::rotateVec(die.rot, mesh.verts[i]);
    }

    struct DrawFace {
        std::vector<float> px;
        std::vector<float> py;
        float avgZ;
        uint32_t col;
    };
    std::vector<DrawFace> faces;
    Vec3 lightDir = vNorm(-0.35f, 0.45f, 0.82f);

    for (const auto& f : mesh.faces) {
        if (f.verts.size() < 3) continue;
        const Vec3& a = rotVerts[static_cast<size_t>(f.verts[0])];
        const Vec3& b = rotVerts[static_cast<size_t>(f.verts[1])];
        const Vec3& c = rotVerts[static_cast<size_t>(f.verts[2])];

        Vec3 ab{b.x - a.x, b.y - a.y, b.z - a.z};
        Vec3 ac{c.x - a.x, c.y - a.y, c.z - a.z};
        Vec3 n{
            ab.y * ac.z - ab.z * ac.y,
            ab.z * ac.x - ab.x * ac.z,
            ab.x * ac.y - ab.y * ac.x
        };
        Vec3 cent{0.0f, 0.0f, 0.0f};
        for (int idx : f.verts) {
            cent.x += rotVerts[static_cast<size_t>(idx)].x;
            cent.y += rotVerts[static_cast<size_t>(idx)].y;
            cent.z += rotVerts[static_cast<size_t>(idx)].z;
        }
        float invN = 1.0f / static_cast<float>(f.verts.size());
        cent.x *= invN;
        cent.y *= invN;
        cent.z *= invN;

        if (n.x * cent.x + n.y * cent.y + n.z * cent.z < 0.0f) {
            n = {-n.x, -n.y, -n.z};
        }
        n = vNorm(n.x, n.y, n.z);

        // Back-face cull facets facing away from camera (+Z)
        if (n.z <= 0.02f) continue;

        float ndl = 0.25f + 0.75f * std::max(0.0f, n.x * lightDir.x + n.y * lightDir.y + n.z * lightDir.z);
        DrawFace df;
        df.avgZ = cent.z;
        df.col  = shadeSkin(skin, ndl, dimmed);
        for (int idx : f.verts) {
            const Vec3& v = rotVerts[static_cast<size_t>(idx)];
            df.px.push_back(sx + v.x * radiusPx);
            df.py.push_back(sy - v.y * radiusPx);
        }
        faces.push_back(std::move(df));
    }

    std::sort(faces.begin(), faces.end(), [](const DrawFace& a, const DrawFace& b) {
        return a.avgZ < b.avgZ;
    });

    for (const auto& df : faces) {
        // Fan-triangulate the convex polygon face without drawing internal diagonals
        for (size_t k = 1; k + 1 < df.px.size(); ++k) {
            C2D_DrawTriangle(df.px[0], df.py[0], df.col,
                             df.px[k], df.py[k], df.col,
                             df.px[k + 1], df.py[k + 1], df.col, 0.45f);
        }
        // Draw only outer polygon perimeter edges
        for (size_t k = 0; k < df.px.size(); ++k) {
            size_t nxt = (k + 1) % df.px.size();
            C2D_DrawLine(df.px[k], df.py[k], skin.edgeColor,
                         df.px[nxt], df.py[nxt], skin.edgeColor, 1.2f, 0.48f);
        }
    }

    // Engraved center medallion behind the number for crisp readability
    uint32_t medFill = dimmed ? rgba32(28, 22, 18, 210) : rgba32(22, 16, 12, 220);
    uint32_t medRim  = skin.edgeColor;
    if (die.settled && die.shape == DIE_D20 && die.result == 20 && !dimmed) {
        medRim = theme::GOLD_BRIGHT;
    } else if (die.settled && die.shape == DIE_D20 && die.result == 1 && !dimmed) {
        medRim = theme::CRIMSON;
    }
    C2D_DrawCircleSolid(sx, sy, 0.56f, 15.0f, medRim);
    C2D_DrawCircleSolid(sx, sy, 0.58f, 13.5f, medFill);

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
    float sc = die.settled ? 0.66f : 0.56f;
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

static void drawConvexPoly(const std::vector<float>& px, const std::vector<float>& py,
                           float z, uint32_t fillCol, uint32_t edgeCol, float edgeWidth = 1.5f) {
    if (px.size() < 3) return;
    for (size_t i = 1; i + 1 < px.size(); ++i) {
        C2D_DrawTriangle(px[0], py[0], fillCol,
                         px[i], py[i], fillCol,
                         px[i + 1], py[i + 1], fillCol, z);
    }
    for (size_t i = 0; i < px.size(); ++i) {
        size_t nxt = (i + 1) % px.size();
        C2D_DrawLine(px[i], py[i], edgeCol, px[nxt], py[nxt], edgeCol, edgeWidth, z + 0.02f);
    }
}

static void buildTokenPoly(DieShape shape, float cx, float cy, float r,
                           std::vector<float>& px, std::vector<float>& py) {
    px.clear();
    py.clear();
    const float pi = 3.14159265f;
    switch (shape) {
        case DIE_D4: {
            for (int i = 0; i < 3; ++i) {
                float a = -pi * 0.5f + (2.0f * pi * i) / 3.0f;
                px.push_back(cx + std::cos(a) * r * 1.08f);
                py.push_back(cy + std::sin(a) * r * 1.08f + r * 0.12f);
            }
            break;
        }
        case DIE_D6: {
            float h = r * 0.86f;
            float b = r * 0.18f; // chamfered corners
            px = {cx - h + b, cx + h - b, cx + h, cx + h, cx + h - b, cx - h + b, cx - h, cx - h};
            py = {cy - h, cy - h, cy - h + b, cy + h - b, cy + h, cy + h, cy + h - b, cy - h + b};
            break;
        }
        case DIE_D8: {
            px = {cx, cx + r * 0.92f, cx, cx - r * 0.92f};
            py = {cy - r * 1.05f, cy, cy + r * 1.05f, cy};
            break;
        }
        case DIE_D10:
        case DIE_D100: {
            px = {cx, cx + r * 0.90f, cx, cx - r * 0.90f};
            py = {cy - r * 1.05f, cy - r * 0.10f, cy + r * 1.02f, cy - r * 0.10f};
            break;
        }
        case DIE_D12: {
            for (int i = 0; i < 5; ++i) {
                float a = -pi * 0.5f + (2.0f * pi * i) / 5.0f;
                px.push_back(cx + std::cos(a) * r);
                py.push_back(cy + std::sin(a) * r);
            }
            break;
        }
        case DIE_D20:
        default: {
            for (int i = 0; i < 6; ++i) {
                float a = -pi * 0.5f + (2.0f * pi * i) / 6.0f;
                px.push_back(cx + std::cos(a) * r);
                py.push_back(cy + std::sin(a) * r);
            }
            break;
        }
    }
}

static void drawD6Pips(float cx, float cy, float r, int val, uint32_t pipCol) {
    float d = r * 0.38f;
    float pr = std::max(2.0f, r * 0.12f);
    auto pip = [&](float ox, float oy) {
        C2D_DrawCircleSolid(cx + ox, cy + oy, 0.65f, pr, pipCol);
    };
    if (val == 1 || val == 3 || val == 5) pip(0, 0);
    if (val >= 2) { pip(-d, -d); pip(d, d); }
    if (val >= 4) { pip(d, -d); pip(-d, d); }
    if (val == 6) { pip(-d, 0); pip(d, 0); }
}

static void drawDieToken25D(C2D_TextBuf buf, const DieBody& die, int idx,
                            const DiceSkinPalette& skin, float cx, float cy, float radius) {
    float jx = 0.0f, jy = 0.0f;
    if (!die.settled) {
        jx = std::sin(die.framesRolling * 0.55f + idx * 1.7f) * 3.5f;
        jy = std::cos(die.framesRolling * 0.65f + idx * 2.3f) * 3.5f;
    }
    float dx = cx + jx;
    float dy = cy + jy;

    // Drop shadow
    std::vector<float> spx, spy;
    buildTokenPoly(die.shape, dx + 3.0f, dy + 4.0f, radius, spx, spy);
    drawConvexPoly(spx, spy, 0.30f, rgba32(0, 0, 0, 95), rgba32(0, 0, 0, 0), 0.0f);

    uint32_t outerCol = shadeSkin(skin, 0.35f, false);
    uint32_t innerCol = shadeSkin(skin, 0.78f, false);
    uint32_t rimCol   = skin.edgeColor;
    if (die.settled && die.result == die.sides) {
        rimCol = theme::GOLD_BRIGHT;
    } else if (die.settled && die.result == 1) {
        rimCol = theme::CRIMSON;
    }

    // Outer beveled body
    std::vector<float> opx, opy;
    buildTokenPoly(die.shape, dx, dy, radius, opx, opy);
    drawConvexPoly(opx, opy, 0.40f, outerCol, rimCol, 1.5f);

    // Inner lit face
    std::vector<float> ipx, ipy;
    buildTokenPoly(die.shape, dx, dy - 1.0f, radius * 0.76f, ipx, ipy);
    drawConvexPoly(ipx, ipy, 0.46f, innerCol, rgba32(255, 255, 255, 45), 1.0f);

    int shownVal = die.settled
        ? die.result
        : (((die.framesRolling * 3 + die.result + idx) % std::max(1, die.sides)) + 1);

    if (die.shape == DIE_D6 && shownVal >= 1 && shownVal <= 6) {
        uint32_t pipCol = skin.numColor;
        if (die.settled && shownVal == 6) pipCol = theme::GOLD_BRIGHT;
        else if (die.settled && shownVal == 1) pipCol = theme::CRIMSON;
        drawD6Pips(dx, dy - 1.0f, radius, shownVal, pipCol);
    } else {
        char numStr[8];
        std::snprintf(numStr, sizeof(numStr), "%d", shownVal);
        C2D_Text txt;
        C2D_TextParse(&txt, buf, numStr);
        C2D_TextOptimize(&txt);
        float sc = (radius >= 24.0f) ? 0.56f : 0.46f;
        float tw = 0.0f, th = 0.0f;
        C2D_TextGetDimensions(&txt, sc, sc, &tw, &th);
        uint32_t ncol = skin.numColor;
        if (die.settled && shownVal == die.sides) ncol = theme::GOLD_BRIGHT;
        else if (die.settled && shownVal == 1) ncol = theme::CRIMSON;
        float yOff = (die.shape == DIE_D4) ? 2.0f : -1.0f;
        C2D_DrawText(&txt, C2D_WithColor, dx - tw * 0.5f, dy - th * 0.5f + yOff, 0.65f, sc, sc, ncol);
    }
}

} // namespace

void renderTopDiceTray(C2D_TextBuf buf, const AppState& app) {
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

    // Subtle tray skin label in top-left corner of felt
    C2D_Text lbl;
    char trayInfo[64];
    std::snprintf(trayInfo, sizeof(trayInfo), "%s  |  %s", skin.name, tray.name);
    C2D_TextParse(&lbl, buf, trayInfo);
    C2D_TextOptimize(&lbl);
    C2D_DrawText(&lbl, C2D_WithColor, 26.0f, 24.0f, 0.55f, 0.32f, 0.32f, theme::GOLD_BRIGHT);

    int activeN = app.dice.activeDieCount();
    if (activeN <= 2) {
        // Hero Mode (1-2 dice): upgraded 3D polyhedral renderer
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

        drawDie3D(buf, app.dice.dieA(), skin, cx, cy, scale, dimA);
        if (activeN == 2) {
            drawDie3D(buf, app.dice.dieB(), skin, cx, cy, scale, dimB);
        }
    } else {
        // Grid Token Mode (3-12 dice): crisp 2.5D stylized polyhedral tokens
        int cols = (activeN <= 4) ? 2 : (activeN <= 6 ? 3 : 4);
        int rows = (activeN + cols - 1) / cols;
        float radius = (activeN <= 4) ? 28.0f : (activeN <= 6 ? 25.0f : 21.0f);
        float cellW = 212.0f / static_cast<float>(cols);
        float cellH = 170.0f / static_cast<float>(rows);
        float startX = 28.0f + cellW * 0.5f;
        float startY = 44.0f + cellH * 0.5f;

        for (int i = 0; i < activeN; ++i) {
            int c = i % cols;
            int r = i / cols;
            float dx = startX + c * cellW;
            float dy = startY + r * cellH;
            drawDieToken25D(buf, app.dice.dieAt(i), i, skin, dx, dy, radius);
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
        C2D_DrawText(&bk, C2D_WithColor, 274.0f, 74.0f, 0.55f, 0.33f, 0.33f, theme::INK);

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
            C2D_DrawText(&rl, C2D_WithColor, 274.0f, 89.0f, 0.55f, 0.29f, 0.29f, theme::INK_MUTED);
        }
    }

    C2D_DrawRectSolid(274.0f, 106.0f, 0.2f, 120.0f, 1.0f, theme::PARCHMENT_EDGE);
    C2D_Text hTitle;
    C2D_TextParse(&hTitle, buf, "Recent Rolls:");
    C2D_TextOptimize(&hTitle);
    C2D_DrawText(&hTitle, C2D_WithColor, 274.0f, 111.0f, 0.55f, 0.35f, 0.35f, theme::INK_MUTED);

    float hy = 128.0f;
    for (size_t i = 0; i < app.dice.history().size() && i < 6; ++i) {
        C2D_Text ht;
        C2D_TextParse(&ht, buf, app.dice.history()[i].label.c_str());
        C2D_TextOptimize(&ht);
        C2D_DrawText(&ht, C2D_WithColor, 274.0f, hy, 0.55f, 0.32f, 0.32f,
                     (i == 0) ? theme::INK : theme::INK_MUTED);
        hy += 17.0f;
    }
}

} // namespace dmtk
