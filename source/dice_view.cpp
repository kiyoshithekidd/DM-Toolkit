#include "dice_view.h"
#include "ui_theme.h"

#ifdef __3DS__
#include "dice_vsh_shbin.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace dmtk {

namespace {

struct DiceVertex {
    float x, y, z;
    float nx, ny, nz;
};

static Vec3 vNorm(float x, float y, float z) {
    float l = std::sqrt(x * x + y * y + z * z);
    if (l < 1e-6f) return {0.0f, 0.0f, 1.0f};
    return {x / l, y / l, z / l};
}

static void appendFaceTris(std::vector<DiceVertex>& verts, const std::vector<Vec3>& poly) {
    if (poly.size() < 3) return;
    Vec3 a = poly[0], b = poly[1], c = poly[2];
    Vec3 ab{b.x - a.x, b.y - a.y, b.z - a.z};
    Vec3 ac{c.x - a.x, c.y - a.y, c.z - a.z};
    Vec3 n{
        ab.y * ac.z - ab.z * ac.y,
        ab.z * ac.x - ab.x * ac.z,
        ab.x * ac.y - ab.y * ac.x
    };
    Vec3 cent{0.0f, 0.0f, 0.0f};
    for (const auto& p : poly) {
        cent.x += p.x; cent.y += p.y; cent.z += p.z;
    }
    float invN = 1.0f / static_cast<float>(poly.size());
    cent.x *= invN; cent.y *= invN; cent.z *= invN;

    bool flip = (n.x * cent.x + n.y * cent.y + n.z * cent.z < 0.0f);
    if (flip) n = {-n.x, -n.y, -n.z};
    n = vNorm(n.x, n.y, n.z);

    for (size_t k = 1; k + 1 < poly.size(); ++k) {
        const Vec3& p0 = poly[0];
        const Vec3& p1 = flip ? poly[k + 1] : poly[k];
        const Vec3& p2 = flip ? poly[k] : poly[k + 1];
        verts.push_back({p0.x, p0.y, p0.z, n.x, n.y, n.z});
        verts.push_back({p1.x, p1.y, p1.z, n.x, n.y, n.z});
        verts.push_back({p2.x, p2.y, p2.z, n.x, n.y, n.z});
    }
}

static void buildDie3DMesh(DieShape shape, std::vector<DiceVertex>& outVerts) {
    outVerts.clear();
    DieShape base = (shape == DIE_D100) ? DIE_D10 : shape;

    switch (base) {
        case DIE_D4: {
            std::vector<Vec3> v = {
                vNorm( 1,  1,  1), vNorm(-1, -1,  1),
                vNorm(-1,  1, -1), vNorm( 1, -1, -1)
            };
            int f[4][3] = {{0,1,2},{0,3,1},{0,2,3},{1,3,2}};
            for (auto& tri : f) appendFaceTris(outVerts, {v[tri[0]], v[tri[1]], v[tri[2]]});
            break;
        }
        case DIE_D6: {
            const float s = 0.62f;
            std::vector<Vec3> v = {
                {-s,-s, s}, { s,-s, s}, { s, s, s}, {-s, s, s},
                {-s,-s,-s}, { s,-s,-s}, { s, s,-s}, {-s, s,-s}
            };
            int f[6][4] = {{0,1,2,3},{5,4,7,6},{1,5,6,2},{4,0,3,7},{3,2,6,7},{4,5,1,0}};
            for (auto& q : f) appendFaceTris(outVerts, {v[q[0]], v[q[1]], v[q[2]], v[q[3]]});
            break;
        }
        case DIE_D8: {
            std::vector<Vec3> v = {
                {0,0,1}, {1,0,0}, {0,1,0}, {-1,0,0}, {0,-1,0}, {0,0,-1}
            };
            int f[8][3] = {
                {0,1,2},{0,2,3},{0,3,4},{0,4,1},
                {5,2,1},{5,3,2},{5,4,3},{5,1,4}
            };
            for (auto& tri : f) appendFaceTris(outVerts, {v[tri[0]], v[tri[1]], v[tri[2]]});
            break;
        }
        case DIE_D10: {
            const float pi = 3.14159265f;
            std::vector<Vec3> v;
            v.push_back({0.0f, 0.0f, 1.0f});
            v.push_back({0.0f, 0.0f, -1.0f});
            for (int i = 0; i < 10; ++i) {
                float a = (2.0f * pi * i) / 10.0f;
                float z = (i % 2 == 0) ? 0.18f : -0.18f;
                v.push_back(vNorm(std::cos(a), std::sin(a), z));
            }
            for (int i = 0; i < 10; ++i) {
                int a = 2 + i;
                int b = 2 + ((i + 1) % 10);
                appendFaceTris(outVerts, {v[0], v[a], v[b]});
                appendFaceTris(outVerts, {v[1], v[b], v[a]});
            }
            break;
        }
        case DIE_D12: {
            const float phi = 1.61803398875f;
            const float inv = 1.0f / phi;
            std::vector<Vec3> v = {
                vNorm( 1,  1,  1), vNorm( 1,  1, -1), vNorm( 1, -1,  1), vNorm( 1, -1, -1),
                vNorm(-1,  1,  1), vNorm(-1,  1, -1), vNorm(-1, -1,  1), vNorm(-1, -1, -1),
                vNorm(0,  inv,  phi), vNorm(0,  inv, -phi), vNorm(0, -inv,  phi), vNorm(0, -inv, -phi),
                vNorm( inv,  phi, 0), vNorm( inv, -phi, 0), vNorm(-inv,  phi, 0), vNorm(-inv, -phi, 0),
                vNorm( phi, 0,  inv), vNorm( phi, 0, -inv), vNorm(-phi, 0,  inv), vNorm(-phi, 0, -inv)
            };
            int f[12][5] = {
                {0, 8, 10, 2, 16}, {0, 16, 17, 1, 12}, {0, 12, 14, 4, 8},
                {4, 14, 5, 19, 18}, {4, 18, 6, 10, 8}, {2, 10, 6, 15, 13},
                {2, 13, 3, 17, 16}, {1, 17, 3, 11, 9}, {1, 9, 5, 14, 12},
                {7, 11, 3, 13, 15}, {7, 15, 6, 18, 19}, {7, 19, 5, 9, 11}
            };
            for (auto& pent : f) {
                appendFaceTris(outVerts, {v[pent[0]], v[pent[1]], v[pent[2]], v[pent[3]], v[pent[4]]});
            }
            break;
        }
        case DIE_D20:
        default: {
            const float phi = 1.61803398875f;
            std::vector<Vec3> v = {
                vNorm(-1,  phi, 0), vNorm( 1,  phi, 0), vNorm(-1, -phi, 0), vNorm( 1, -phi, 0),
                vNorm( 0, -1,  phi), vNorm( 0,  1,  phi), vNorm( 0, -1, -phi), vNorm( 0,  1, -phi),
                vNorm( phi, 0, -1), vNorm( phi, 0,  1), vNorm(-phi, 0, -1), vNorm(-phi, 0,  1)
            };
            int f[20][3] = {
                {0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},
                {1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},
                {3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},
                {4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1}
            };
            for (auto& tri : f) appendFaceTris(outVerts, {v[tri[0]], v[tri[1]], v[tri[2]]});
            break;
        }
    }
}

#ifdef __3DS__
struct Dice3DRenderer {
    bool          initialized = false;
    DVLB_s*       dvlb = nullptr;
    shaderProgram_s program{};
    int           uLocProj = -1;
    int           uLocMV   = -1;
    C3D_AttrInfo  attrInfo{};
    C3D_LightEnv  lightEnv{};
    C3D_Light     light{};
    C3D_LightLut  lutPhong{};
    C3D_Mtx       proj{};

    struct MeshVBO {
        DiceVertex* data = nullptr;
        int         vertCount = 0;
        C3D_BufInfo bufInfo{};
    };
    MeshVBO meshes[DIE_SHAPE_COUNT]{};

    void ensureInit() {
        if (initialized) return;
        initialized = true;

        dvlb = DVLB_ParseFile(reinterpret_cast<u32*>(const_cast<u8*>(dice_vsh_shbin)), dice_vsh_shbin_size);
        shaderProgramInit(&program);
        shaderProgramSetVsh(&program, &dvlb->DVLE[0]);
        uLocProj = shaderInstanceGetUniformLocation(program.vertexShader, "projection");
        uLocMV   = shaderInstanceGetUniformLocation(program.vertexShader, "modelView");

        AttrInfo_Init(&attrInfo);
        AttrInfo_AddLoader(&attrInfo, 0, GPU_FLOAT, 3); // position
        AttrInfo_AddLoader(&attrInfo, 1, GPU_FLOAT, 3); // normal

        Mtx_PerspTilt(&proj, C3D_AngleFromDegrees(36.0f), C3D_AspectRatioTop, 1.0f, 50.0f, false);

        C3D_LightEnvInit(&lightEnv);
        LightLut_Phong(&lutPhong, 28.0f);
        C3D_LightEnvLut(&lightEnv, GPU_LUT_D0, GPU_LUTINPUT_NH, false, &lutPhong);
        C3D_LightInit(&light, &lightEnv);
        C3D_LightColor(&light, 1.0f, 0.97f, 0.90f);
        C3D_FVec lightPos = FVec4_New(-2.5f, 3.2f, 2.0f, 0.0f);
        C3D_LightPosition(&light, &lightPos);

        std::vector<DiceVertex> tmp;
        for (int s = 0; s < DIE_SHAPE_COUNT; ++s) {
            buildDie3DMesh(static_cast<DieShape>(s), tmp);
            meshes[s].vertCount = static_cast<int>(tmp.size());
            size_t bytes = tmp.size() * sizeof(DiceVertex);
            meshes[s].data = static_cast<DiceVertex*>(linearAlloc(bytes));
            if (meshes[s].data) {
                std::memcpy(meshes[s].data, tmp.data(), bytes);
                BufInfo_Init(&meshes[s].bufInfo);
                BufInfo_Add(&meshes[s].bufInfo, meshes[s].data, sizeof(DiceVertex), 2, 0x10);
            }
        }
    }

    void drawDie(const DieBody& die, const DiceSkinPalette& skin,
                 float worldX, float worldY, float worldZ, float scale, bool dimmed) {
        if (!die.active) return;
        MeshVBO& m = meshes[die.shape < DIE_SHAPE_COUNT ? die.shape : DIE_D20];
        if (!m.data || m.vertCount <= 0) return;

        float mult = dimmed ? 0.50f : 1.0f;
        float mr = (skin.midR / 255.0f) * mult;
        float mg = (skin.midG / 255.0f) * mult;
        float mb = (skin.midB / 255.0f) * mult;
        float hr = (skin.hiR  / 255.0f) * mult;
        float hg = (skin.hiG  / 255.0f) * mult;
        float hb = (skin.hiB  / 255.0f) * mult;

        C3D_Material mat = {
            { mr * 0.35f, mg * 0.35f, mb * 0.35f },
            { mr * 0.92f, mg * 0.92f, mb * 0.92f },
            { hr * 0.70f, hg * 0.70f, hb * 0.70f },
            { 0.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f }
        };
        C3D_LightEnvMaterial(&lightEnv, &mat);

        C3D_Mtx mv, rotMtx;
        Mtx_Identity(&mv);
        Mtx_Translate(&mv, worldX, worldY, worldZ, true);
        C3D_FQuat q = Quat_New(die.rot.x, die.rot.y, die.rot.z, die.rot.w);
        Mtx_FromQuat(&rotMtx, q);
        Mtx_Multiply(&mv, &mv, &rotMtx);
        Mtx_Scale(&mv, scale, scale, scale);

        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocMV, &mv);
        C3D_SetBufInfo(&m.bufInfo);
        C3D_DrawArrays(GPU_TRIANGLES, 0, m.vertCount);
    }

    void beginPass() {
        ensureInit();
        C3D_BindProgram(&program);
        C3D_SetAttrInfo(&attrInfo);
        C3D_LightEnvBind(&lightEnv);

        C3D_TexEnv* env = C3D_GetTexEnv(0);
        C3D_TexEnvInit(env);
        C3D_TexEnvSrc(env, C3D_Both, GPU_FRAGMENT_PRIMARY_COLOR, GPU_FRAGMENT_SECONDARY_COLOR, 0);
        C3D_TexEnvFunc(env, C3D_Both, GPU_ADD);

        C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
        C3D_CullFace(GPU_CULL_BACK_CCW);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProj, &proj);
    }
};

static Dice3DRenderer s_dice3d;
#endif

static void drawD6Pips(float cx, float cy, float r, int val, uint32_t pipCol) {
    float d = r * 0.42f;
    float pr = std::max(2.2f, r * 0.14f);
    auto pip = [&](float ox, float oy) {
        C2D_DrawCircleSolid(cx + ox, cy + oy, 0.72f, pr, pipCol);
    };
    if (val == 1 || val == 3 || val == 5) pip(0, 0);
    if (val >= 2) { pip(-d, -d); pip(d, d); }
    if (val >= 4) { pip(d, -d); pip(-d, d); }
    if (val == 6) { pip(-d, 0); pip(d, 0); }
}

static void drawDieLabelOverlay(C2D_TextBuf buf, const DieBody& die, const DiceSkinPalette& skin,
                                float sx, float sy, float scalePx, bool dimmed) {
    if (!die.active || !die.settled) return;

    if (die.shape == DIE_D6 && die.result >= 1 && die.result <= 6) {
        uint32_t pipCol = dimmed ? theme::INK_MUTED : skin.numColor;
        if (die.result == 6 && !dimmed) pipCol = theme::GOLD_BRIGHT;
        else if (die.result == 1 && !dimmed) pipCol = theme::CRIMSON;
        drawD6Pips(sx, sy, scalePx * 0.42f, die.result, pipCol);
        return;
    }

    char numStr[8];
    if (die.shape == DIE_D100) {
        std::snprintf(numStr, sizeof(numStr), "%02d", die.result);
    } else {
        std::snprintf(numStr, sizeof(numStr), "%d", die.result);
    }

    // Subtle dark backing disc so the number pops cleanly on the lit 3D face
    float medR = std::max(9.0f, scalePx * 0.24f);
    C2D_DrawCircleSolid(sx, sy, 0.68f, medR, rgba32(18, 12, 8, 175));

    C2D_Text txt;
    C2D_TextParse(&txt, buf, numStr);
    C2D_TextOptimize(&txt);
    float sc = (scalePx >= 52.0f) ? 0.66f : (scalePx >= 40.0f ? 0.52f : 0.42f);
    float tw = 0.0f, th = 0.0f;
    C2D_TextGetDimensions(&txt, sc, sc, &tw, &th);
    uint32_t ncol = dimmed ? theme::INK_MUTED : skin.numColor;
    if (die.result == die.sides && !dimmed && die.shape != DIE_D100) {
        ncol = theme::GOLD_BRIGHT;
    } else if (die.result == 1 && !dimmed && die.shape != DIE_D100) {
        ncol = theme::CRIMSON;
    }
    C2D_DrawText(&txt, C2D_WithColor, sx - tw * 0.5f, sy - th * 0.5f, 0.75f, sc, sc, ncol);
}

} // namespace

void renderTopDiceTray(C2D_TextBuf buf, C3D_RenderTarget* target, const AppState& app) {
    const TraySkinPalette& tray = getTraySkin(app.traySkinIdx);
    const DiceSkinPalette& skin = getDiceSkin(app.diceSkinIdx);

    // 1. Draw 2D Dice Tray + Drop Shadows with citro2d
    C2D_DrawRectSolid(0.0f, 0.0f, 0.05f, 268.0f, 240.0f, tray.frameOuter);
    C2D_DrawRectSolid(8.0f, 8.0f, 0.10f, 252.0f, 224.0f, tray.frameInner);
    C2D_DrawRectSolid(14.0f, 14.0f, 0.12f, 240.0f, 212.0f, tray.trimColor);
    C2D_DrawRectSolid(16.0f, 16.0f, 0.15f, 236.0f, 208.0f, tray.feltMain);
    C2D_DrawRectSolid(16.0f, 16.0f, 0.17f, 236.0f, 8.0f, tray.feltDark);
    C2D_DrawRectSolid(16.0f, 216.0f, 0.17f, 236.0f, 8.0f, tray.feltDark);
    C2D_DrawRectSolid(16.0f, 16.0f, 0.17f, 8.0f, 208.0f, tray.feltDark);
    C2D_DrawRectSolid(244.0f, 16.0f, 0.17f, 8.0f, 208.0f, tray.feltDark);

    // Right 132px parchment backdrop
    C2D_DrawRectSolid(268.0f, 0.0f, 0.10f, 132.0f, 240.0f, theme::PARCHMENT);
    C2D_DrawRectSolid(268.0f, 0.0f, 0.15f, 132.0f, 26.0f, theme::WALNUT);

    int activeN = app.dice.activeDieCount();
    const float cx = 134.0f;
    const float cy = 124.0f;
    const float trayScale2D = (activeN <= 2) ? 72.0f : 62.0f;

    // Draw soft felt drop shadows under each active die
    for (int i = 0; i < activeN; ++i) {
        const DieBody& d = app.dice.dieAt(i);
        if (!d.active) continue;
        float sx = cx + d.pos.x * trayScale2D;
        float sy = cy - d.pos.y * trayScale2D;
        float baseR = (activeN <= 2) ? 30.0f : (activeN <= 6 ? 22.0f : 17.0f);
        float sOff = 5.0f + d.pos.z * 12.0f;
        float sRad = baseR * (1.0f - std::min(0.35f, d.pos.z * 0.25f));
        C2D_DrawEllipseSolid(sx - sRad + sOff * 0.45f, sy - sRad * 0.85f + sOff,
                             0.25f, sRad * 2.0f, sRad * 1.7f, rgba32(0, 0, 0, 105));
    }

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

    // 2. Flush citro2d & render true hardware-lit 3D tumbling dice via citro3d
    C2D_Flush();
#ifdef __3DS__
    s_dice3d.beginPass();
    // Map felt center (134, 124) into perspective view space at z = -8.0f
    // At fovY=36 deg and z=-8.0f: visible height = 2 * 8 * tan(18) = 5.20, visible width = 8.66
    const float unitsPerPx = 5.20f / 240.0f;
    const float worldCenterX = (134.0f - 200.0f) * unitsPerPx;
    const float worldCenterY = (120.0f - 124.0f) * unitsPerPx;
    const float dieScale3D = (activeN <= 2) ? 0.68f : (activeN <= 6 ? 0.48f : 0.36f);

    std::vector<int> drawOrder(static_cast<size_t>(activeN));
    for (int i = 0; i < activeN; ++i) drawOrder[static_cast<size_t>(i)] = i;
    std::sort(drawOrder.begin(), drawOrder.end(), [&](int a, int b) {
        return app.dice.dieAt(a).pos.z < app.dice.dieAt(b).pos.z;
    });

    for (int idx : drawOrder) {
        const DieBody& d = app.dice.dieAt(idx);
        bool dim = (idx == 0 && dimA) || (idx == 1 && dimB);
        float wx = worldCenterX + (d.pos.x * trayScale2D) * unitsPerPx;
        float wy = worldCenterY + (d.pos.y * trayScale2D) * unitsPerPx;
        float wz = -8.0f + d.pos.z * 1.45f;
        s_dice3d.drawDie(d, skin, wx, wy, wz, dieScale3D, dim);
    }

    // Restore citro2d scene state for crisp text & settled die numbers
    C2D_SceneBegin(target);
#endif

    // 3. Draw settled face numbers/pips + UI text overlays
    for (int i = 0; i < activeN; ++i) {
        const DieBody& d = app.dice.dieAt(i);
        bool dim = (i == 0 && dimA) || (i == 1 && dimB);
        float sx = cx + d.pos.x * trayScale2D;
        float sy = cy - d.pos.y * trayScale2D;
        float pxSize = (activeN <= 2) ? 62.0f : (activeN <= 6 ? 44.0f : 34.0f);
        drawDieLabelOverlay(buf, d, skin, sx, sy, pxSize, dim);
    }

    C2D_Text lbl;
    char trayInfo[64];
    std::snprintf(trayInfo, sizeof(trayInfo), "%s  |  %s", skin.name, tray.name);
    C2D_TextParse(&lbl, buf, trayInfo);
    C2D_TextOptimize(&lbl);
    C2D_DrawText(&lbl, C2D_WithColor, 26.0f, 24.0f, 0.55f, 0.32f, 0.32f, theme::GOLD_BRIGHT);

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
