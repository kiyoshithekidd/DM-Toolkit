#include "dice_view.h"
#include "ui_theme.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace dmtk {

namespace {

static uint32_t lerpSkinColor(const DiceSkinPalette& skin, float light, float mult) {
    // light in [0, 1]: 0..0.5 blends lo->mid, 0.5..1.0 blends mid->hi
    float r, g, b;
    if (light < 0.5f) {
        float t = light * 2.0f;
        r = skin.loR + (skin.midR - skin.loR) * t;
        g = skin.loG + (skin.midG - skin.loG) * t;
        b = skin.loB + (skin.midB - skin.loB) * t;
    } else {
        float t = (light - 0.5f) * 2.0f;
        r = skin.midR + (skin.hiR - skin.midR) * t;
        g = skin.midG + (skin.hiG - skin.midG) * t;
        b = skin.midB + (skin.hiB - skin.midB) * t;
    }
    uint8_t rb = static_cast<uint8_t>(std::clamp(r * mult, 0.0f, 255.0f));
    uint8_t gb = static_cast<uint8_t>(std::clamp(g * mult, 0.0f, 255.0f));
    uint8_t bb = static_cast<uint8_t>(std::clamp(b * mult, 0.0f, 255.0f));
    return rgba32(rb, gb, bb, 255);
}

struct Face3D {
    std::vector<Vec3> verts;
    Vec3              normal;
    int               value = 1;
};

static Vec3 norm3(float x, float y, float z) {
    float len = std::sqrt(x * x + y * y + z * z);
    if (len < 1e-6f) return {0.0f, 0.0f, 1.0f};
    return {x / len, y / len, z / len};
}

static float dot3(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vec3 cross3(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

static void buildPolyFaces(DieShape shape, std::vector<Face3D>& outFaces) {
    outFaces.clear();
    switch (shape) {
        case DIE_D4: {
            const Vec3 v[4] = {
                norm3(-1, -1, -1), norm3( 1,  1, -1), norm3( 1, -1,  1), norm3(-1,  1,  1)
            };
            const int idx[4][3] = {{1,2,3}, {0,2,3}, {0,1,3}, {0,1,2}};
            std::vector<Vec3> norms;
            DiceRoller::getFaceNormals(DIE_D4, norms);
            for (int f = 0; f < 4; ++f) {
                Face3D face;
                face.normal = norms[static_cast<size_t>(f)];
                face.value  = f + 1;
                face.verts  = {v[idx[f][0]], v[idx[f][1]], v[idx[f][2]]};
                outFaces.push_back(std::move(face));
            }
            break;
        }
        case DIE_D6: {
            const float s = 0.72f;
            const Vec3 v[8] = {
                {-s,-s, s}, { s,-s, s}, { s, s, s}, {-s, s, s}, // 0..3 front (+z)
                {-s,-s,-s}, { s,-s,-s}, { s, s,-s}, {-s, s,-s}  // 4..7 back  (-z)
            };
            const int idx[6][4] = {
                {0,1,2,3}, // +z (1)
                {5,4,7,6}, // -z (2)
                {3,2,6,7}, // +y (3)
                {4,5,1,0}, // -y (4)
                {1,5,6,2}, // +x (5)
                {4,0,3,7}  // -x (6)
            };
            std::vector<Vec3> norms;
            DiceRoller::getFaceNormals(DIE_D6, norms);
            for (int f = 0; f < 6; ++f) {
                Face3D face;
                face.normal = norms[static_cast<size_t>(f)];
                face.value  = f + 1;
                face.verts  = {v[idx[f][0]], v[idx[f][1]], v[idx[f][2]], v[idx[f][3]]};
                outFaces.push_back(std::move(face));
            }
            break;
        }
        case DIE_D8: {
            const Vec3 v[6] = {
                { 1, 0, 0}, {-1, 0, 0}, {0,  1, 0}, {0, -1, 0}, {0, 0,  1}, {0, 0, -1}
            };
            const int idx[8][3] = {
                {0,2,4}, {1,2,4}, {0,3,4}, {1,3,4},
                {0,2,5}, {1,2,5}, {0,3,5}, {1,3,5}
            };
            std::vector<Vec3> norms;
            DiceRoller::getFaceNormals(DIE_D8, norms);
            for (int f = 0; f < 8; ++f) {
                Face3D face;
                face.normal = norms[static_cast<size_t>(f)];
                face.value  = f + 1;
                face.verts  = {v[idx[f][0]], v[idx[f][1]], v[idx[f][2]]};
                outFaces.push_back(std::move(face));
            }
            break;
        }
        case DIE_D10:
        case DIE_D100: {
            std::vector<Vec3> norms;
            DiceRoller::getFaceNormals(DIE_D10, norms);
            for (int f = 0; f < 10; ++f) {
                const Vec3& n = norms[static_cast<size_t>(f)];
                Vec3 up = (std::fabs(n.z) < 0.9f) ? Vec3{0, 0, 1} : Vec3{1, 0, 0};
                Vec3 u = norm3(cross3(up, n).x, cross3(up, n).y, cross3(up, n).z);
                Vec3 v = cross3(n, u);
                Vec3 c = {n.x * 0.76f, n.y * 0.76f, n.z * 0.76f};
                Face3D face;
                face.normal = n;
                face.value  = (shape == DIE_D100) ? ((f % 10) * 10) : (f + 1);
                // Kite vertices around face center c
                face.verts = {
                    {c.x + v.x * 0.62f, c.y + v.y * 0.62f, c.z + v.z * 0.62f},
                    {c.x + u.x * 0.48f - v.x * 0.08f, c.y + u.y * 0.48f - v.y * 0.08f, c.z + u.z * 0.48f - v.z * 0.08f},
                    {c.x - v.x * 0.56f, c.y - v.y * 0.56f, c.z - v.z * 0.56f},
                    {c.x - u.x * 0.48f - v.x * 0.08f, c.y - u.y * 0.48f - v.y * 0.08f, c.z - u.z * 0.48f - v.z * 0.08f}
                };
                outFaces.push_back(std::move(face));
            }
            break;
        }
        case DIE_D12: {
            std::vector<Vec3> norms;
            DiceRoller::getFaceNormals(DIE_D12, norms);
            for (int f = 0; f < 12; ++f) {
                const Vec3& n = norms[static_cast<size_t>(f)];
                // Collect 5 adjacent face normals (dot > 0.35)
                std::vector<Vec3> adj;
                for (int j = 0; j < 12; ++j) {
                    if (j != f && dot3(n, norms[static_cast<size_t>(j)]) > 0.35f) {
                        adj.push_back(norms[static_cast<size_t>(j)]);
                    }
                }
                Vec3 up = (std::fabs(n.z) < 0.9f) ? Vec3{0, 0, 1} : Vec3{1, 0, 0};
                Vec3 u = cross3(up, n);
                u = norm3(u.x, u.y, u.z);
                Vec3 v = cross3(n, u);
                std::sort(adj.begin(), adj.end(), [&](const Vec3& a, const Vec3& b) {
                    return std::atan2(dot3(a, v), dot3(a, u)) < std::atan2(dot3(b, v), dot3(b, u));
                });
                Face3D face;
                face.normal = n;
                face.value  = f + 1;
                for (size_t k = 0; k < adj.size(); ++k) {
                    const Vec3& na = adj[k];
                    const Vec3& nb = adj[(k + 1) % adj.size()];
                    Vec3 vt = norm3(n.x + na.x + nb.x, n.y + na.y + nb.y, n.z + na.z + nb.z);
                    face.verts.push_back({vt.x * 0.98f, vt.y * 0.98f, vt.z * 0.98f});
                }
                outFaces.push_back(std::move(face));
            }
            break;
        }
        case DIE_D20:
        default: {
            const float phi = 1.61803398875f;
            const Vec3 v[12] = {
                norm3(-1,  phi, 0), norm3( 1,  phi, 0), norm3(-1, -phi, 0), norm3( 1, -phi, 0),
                norm3( 0, -1,  phi), norm3( 0,  1,  phi), norm3( 0, -1, -phi), norm3( 0,  1, -phi),
                norm3( phi, 0, -1), norm3( phi, 0,  1), norm3(-phi, 0, -1), norm3(-phi, 0,  1)
            };
            const int faces[20][3] = {
                {0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},
                {1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},
                {3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},
                {4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1}
            };
            std::vector<Vec3> norms;
            DiceRoller::getFaceNormals(DIE_D20, norms);
            for (int f = 0; f < 20; ++f) {
                Face3D face;
                face.normal = norms[static_cast<size_t>(f)];
                face.value  = f + 1;
                face.verts  = {v[faces[f][0]], v[faces[f][1]], v[faces[f][2]]};
                outFaces.push_back(std::move(face));
            }
            break;
        }
    }
}

static const std::vector<Face3D>& cachedFacesForShape(DieShape shape) {
    static std::vector<Face3D> s_cache[DIE_SHAPE_COUNT];
    static bool s_init[DIE_SHAPE_COUNT] = {};
    int idx = static_cast<int>(shape);
    if (idx < 0 || idx >= DIE_SHAPE_COUNT) idx = DIE_D20;
    if (!s_init[idx]) {
        buildPolyFaces(static_cast<DieShape>(idx), s_cache[idx]);
        s_init[idx] = true;
    }
    return s_cache[idx];
}

static void drawD6Pips3D(float sx, float sy, float R, float zBase,
                         const Vec3& fc, const Vec3& u, const Vec3& v,
                         int val, uint32_t pipCol) {
    const float d = 0.40f;
    const float pr = std::max(2.0f, R * 0.12f);
    auto pip = [&](float ox, float oy) {
        Vec3 p = {
            fc.x + u.x * ox + v.x * oy,
            fc.y + u.y * ox + v.y * oy,
            fc.z + u.z * ox + v.z * oy
        };
        float px = sx + p.x * R;
        float py = sy - p.y * R;
        C2D_DrawCircleSolid(px + 0.7f, py + 0.7f, zBase + 0.01f, pr, rgba32(0, 0, 0, 140));
        C2D_DrawCircleSolid(px, py, zBase + 0.02f, pr, pipCol);
    };
    if (val == 1 || val == 3 || val == 5) pip(0, 0);
    if (val >= 2) { pip(-d, -d); pip(d, d); }
    if (val >= 4) { pip(d, -d); pip(-d, d); }
    if (val == 6) { pip(-d, 0); pip(d, 0); }
}

static void drawDieSprite(C2D_TextBuf buf, C2D_SpriteSheet /*sheet*/, const DieBody& die, int /*idx*/,
                          const DiceSkinPalette& skin, float sx, float sy,
                          float targetDiameter, bool dimmed) {
    if (!die.active) return;

    float heightScale = die.settled ? 1.0f : (1.0f + die.pos.z * 0.28f);
    float drawDiam = targetDiameter * heightScale;
    float R = drawDiam * 0.48f;

    // Soft drop shadow on felt underneath the die
    float shadowOff = die.settled ? 4.0f : (5.0f + die.pos.z * 12.0f);
    float shadowRad = drawDiam * 0.40f;
    C2D_DrawEllipseSolid(sx - shadowRad + shadowOff * 0.45f,
                         sy - shadowRad * 0.85f + shadowOff,
                         0.25f, shadowRad * 2.0f, shadowRad * 1.7f,
                         rgba32(0, 0, 0, 110));

    float mult = dimmed ? 0.50f : 1.0f;
    uint32_t edgeCol = dimmed ? rgba32(30, 24, 20, 255) : skin.edgeColor;

    const auto& mesh = cachedFacesForShape(die.shape);

    struct DrawableFace {
        const Face3D*     face;
        Vec3              wn;
        Vec3              center3D;
        std::vector<Vec3> wVerts;
    };
    std::vector<DrawableFace> visible;
    visible.reserve(mesh.size());

    for (const auto& f : mesh) {
        Vec3 wn = DiceRoller::rotateVec(die.rot, f.normal);
        if (wn.z <= 0.02f) continue; // back-face cull
        DrawableFace df;
        df.face = &f;
        df.wn   = wn;
        Vec3 sum{0, 0, 0};
        df.wVerts.reserve(f.verts.size());
        for (const auto& lv : f.verts) {
            Vec3 wv = DiceRoller::rotateVec(die.rot, lv);
            df.wVerts.push_back(wv);
            sum.x += wv.x; sum.y += wv.y; sum.z += wv.z;
        }
        float invN = 1.0f / static_cast<float>(f.verts.size());
        df.center3D = {sum.x * invN, sum.y * invN, sum.z * invN};
        visible.push_back(std::move(df));
    }

    // Painter's sort back-to-front
    std::sort(visible.begin(), visible.end(), [](const DrawableFace& a, const DrawableFace& b) {
        return a.center3D.z < b.center3D.z;
    });

    for (size_t fi = 0; fi < visible.size(); ++fi) {
        const DrawableFace& df = visible[fi];
        float light = std::clamp(df.wn.x * -0.35f + df.wn.y * 0.45f + df.wn.z * 0.72f, 0.0f, 1.0f);
        uint32_t fcol = lerpSkinColor(skin, light, mult);
        float fz = 0.40f + static_cast<float>(fi) * 0.008f;

        // Project vertices to 2D
        float px[8], py[8];
        size_t nv = std::min<size_t>(df.wVerts.size(), 8);
        for (size_t k = 0; k < nv; ++k) {
            px[k] = sx + df.wVerts[k].x * R;
            py[k] = sy - df.wVerts[k].y * R;
        }
        for (size_t k = 1; k + 1 < nv; ++k) {
            C2D_DrawTriangle(px[0], py[0], fcol, px[k], py[k], fcol, px[k + 1], py[k + 1], fcol, fz);
        }
        for (size_t k = 0; k < nv; ++k) {
            size_t nxt = (k + 1) % nv;
            C2D_DrawLine(px[k], py[k], edgeCol, px[nxt], py[nxt], edgeCol, 1.25f, fz + 0.002f);
        }

        // Draw numeral / pips directly ON the 3D face when facing the viewer
        if (df.wn.z > 0.46f) {
            int val = df.face->value;
            if (die.settled && fi + 1 == visible.size()) {
                val = die.result; // Guarantee exact winning result on top settled face
            }
            if (die.shape == DIE_D6 && val >= 1 && val <= 6) {
                uint32_t pipCol = dimmed ? theme::INK_MUTED : skin.numColor;
                if (die.settled && fi + 1 == visible.size() && !dimmed) {
                    if (val == 6) pipCol = theme::GOLD_BRIGHT;
                    else if (val == 1) pipCol = theme::CRIMSON;
                }
                Vec3 u = norm3(df.wVerts[1].x - df.wVerts[0].x, df.wVerts[1].y - df.wVerts[0].y, df.wVerts[1].z - df.wVerts[0].z);
                Vec3 v = norm3(df.wVerts[3].x - df.wVerts[0].x, df.wVerts[3].y - df.wVerts[0].y, df.wVerts[3].z - df.wVerts[0].z);
                drawD6Pips3D(sx, sy, R, fz + 0.004f, df.center3D, u, v, val, pipCol);
            } else {
                char numStr[8];
                if (die.shape == DIE_D100) std::snprintf(numStr, sizeof(numStr), "%02d", val);
                else std::snprintf(numStr, sizeof(numStr), "%d", val);

                C2D_Text txt;
                C2D_TextParse(&txt, buf, numStr);
                C2D_TextOptimize(&txt);

                float baseSc = (drawDiam >= 64.0f) ? 0.62f : (drawDiam >= 48.0f ? 0.50f : 0.42f);
                // Foreshorten numeral along the face's 3D tilt so it stays glued to the face
                float scX = baseSc * std::max(0.28f, 1.0f - std::fabs(df.wn.x) * 0.70f);
                float scY = baseSc * std::max(0.28f, 1.0f - std::fabs(df.wn.y) * 0.70f);
                float tw = 0.0f, th = 0.0f;
                C2D_TextGetDimensions(&txt, scX, scY, &tw, &th);

                uint32_t ncol = dimmed ? theme::INK_MUTED : skin.numColor;
                if (die.settled && fi + 1 == visible.size() && !dimmed && die.shape != DIE_D100) {
                    if (val == die.sides) ncol = theme::GOLD_BRIGHT;
                    else if (val == 1)    ncol = theme::CRIMSON;
                }

                float fcx = sx + df.center3D.x * R;
                float fcy = sy - df.center3D.y * R;
                float tx = fcx - tw * 0.5f;
                float ty = fcy - th * 0.5f;
                C2D_DrawText(&txt, C2D_WithColor, tx + 0.8f, ty + 0.8f, fz + 0.003f, scX, scY, rgba32(0, 0, 0, 185));
                C2D_DrawText(&txt, C2D_WithColor, tx, ty, fz + 0.005f, scX, scY, ncol);
            }
        }
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
