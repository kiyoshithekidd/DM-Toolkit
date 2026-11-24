#include "dice_roller.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace dmtk {

static uint32_t diceRng(uint32_t& state) {
    if (state == 0) state = 0xCAFE1234u;
    uint32_t x = state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state = x;
    return x;
}

static float randFloat(uint32_t& state, float lo, float hi) {
    float u = static_cast<float>(diceRng(state) & 0xFFFFu) / 65535.0f;
    return lo + u * (hi - lo);
}

int DiceRoller::sidesForShape(DieShape s) {
    static const int kSides[DIE_SHAPE_COUNT] = {4, 6, 8, 10, 12, 20, 100};
    if (s < DIE_SHAPE_COUNT) return kSides[s];
    return 20;
}

const char* DiceRoller::shapeLabel(DieShape s) {
    static const char* kLabels[DIE_SHAPE_COUNT] = {"d4", "d6", "d8", "d10", "d12", "d20", "d100"};
    if (s < DIE_SHAPE_COUNT) return kLabels[s];
    return "d20";
}

Quat DiceRoller::normalizeQuat(const Quat& q) {
    float len = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    if (len < 1e-6f) return {1.0f, 0.0f, 0.0f, 0.0f};
    float inv = 1.0f / len;
    return {q.w * inv, q.x * inv, q.y * inv, q.z * inv};
}

static Quat mulQuat(const Quat& a, const Quat& b) {
    return {
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w
    };
}

Vec3 DiceRoller::rotateVec(const Quat& q, const Vec3& v) {
    Quat qv{0.0f, v.x, v.y, v.z};
    Quat qConj{q.w, -q.x, -q.y, -q.z};
    Quat res = mulQuat(mulQuat(q, qv), qConj);
    return {res.x, res.y, res.z};
}

Quat DiceRoller::slerpQuat(const Quat& a, const Quat& b, float t) {
    float dot = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
    Quat bb = b;
    if (dot < 0.0f) {
        dot = -dot;
        bb = {-b.w, -b.x, -b.y, -b.z};
    }
    if (dot > 0.9995f) {
        return normalizeQuat({
            a.w + t * (bb.w - a.w),
            a.x + t * (bb.x - a.x),
            a.y + t * (bb.y - a.y),
            a.z + t * (bb.z - a.z)
        });
    }
    float theta0 = std::acos(std::clamp(dot, -1.0f, 1.0f));
    float theta = theta0 * t;
    float sinT = std::sin(theta);
    float sinT0 = std::sin(theta0);
    float s0 = std::cos(theta) - dot * sinT / sinT0;
    float s1 = sinT / sinT0;
    return normalizeQuat({
        s0 * a.w + s1 * bb.w,
        s0 * a.x + s1 * bb.x,
        s0 * a.y + s1 * bb.y,
        s0 * a.z + s1 * bb.z
    });
}

Quat DiceRoller::rotationBetween(const Vec3& from, const Vec3& to) {
    float dot = from.x * to.x + from.y * to.y + from.z * to.z;
    if (dot > 0.9999f) return {1.0f, 0.0f, 0.0f, 0.0f};
    if (dot < -0.9999f) {
        Vec3 ortho{1.0f, 0.0f, 0.0f};
        if (std::fabs(from.x) > 0.8f) ortho = {0.0f, 1.0f, 0.0f};
        Vec3 axis{
            from.y * ortho.z - from.z * ortho.y,
            from.z * ortho.x - from.x * ortho.z,
            from.x * ortho.y - from.y * ortho.x
        };
        return normalizeQuat({0.0f, axis.x, axis.y, axis.z});
    }
    Vec3 c{
        from.y * to.z - from.z * to.y,
        from.z * to.x - from.x * to.z,
        from.x * to.y - from.y * to.x
    };
    return normalizeQuat({1.0f + dot, c.x, c.y, c.z});
}

static Vec3 normVec(float x, float y, float z) {
    float l = std::sqrt(x * x + y * y + z * z);
    if (l < 1e-6f) return {0.0f, 0.0f, 1.0f};
    return {x / l, y / l, z / l};
}

void DiceRoller::getFaceNormals(DieShape shape, std::vector<Vec3>& outNormals) {
    outNormals.clear();
    switch (shape) {
        case DIE_D4: {
            outNormals.push_back(normVec( 1,  1,  1));
            outNormals.push_back(normVec(-1, -1,  1));
            outNormals.push_back(normVec(-1,  1, -1));
            outNormals.push_back(normVec( 1, -1, -1));
            break;
        }
        case DIE_D6: {
            outNormals.push_back({ 0,  0,  1}); // 1
            outNormals.push_back({ 1,  0,  0}); // 2
            outNormals.push_back({ 0,  1,  0}); // 3
            outNormals.push_back({ 0, -1,  0}); // 4
            outNormals.push_back({-1,  0,  0}); // 5
            outNormals.push_back({ 0,  0, -1}); // 6
            break;
        }
        case DIE_D8: {
            for (int sx : {-1, 1})
                for (int sy : {-1, 1})
                    for (int sz : {-1, 1})
                        outNormals.push_back(normVec(static_cast<float>(sx), static_cast<float>(sy), static_cast<float>(sz)));
            break;
        }
        case DIE_D10:
        case DIE_D100: {
            const float pi = 3.14159265f;
            for (int i = 0; i < 5; ++i) {
                float a = (2.0f * pi * i) / 5.0f;
                outNormals.push_back(normVec(std::cos(a), std::sin(a),  0.55f));
            }
            for (int i = 0; i < 5; ++i) {
                float a = (2.0f * pi * i) / 5.0f + (pi / 5.0f);
                outNormals.push_back(normVec(std::cos(a), std::sin(a), -0.55f));
            }
            break;
        }
        case DIE_D12: {
            const float phi = 1.61803398875f;
            const float vals[12][3] = {
                {0, 1, phi}, {0, -1, phi}, {0, 1, -phi}, {0, -1, -phi},
                {1, phi, 0}, {-1, phi, 0}, {1, -phi, 0}, {-1, -phi, 0},
                {phi, 0, 1}, {-phi, 0, 1}, {phi, 0, -1}, {-phi, 0, -1}
            };
            for (auto& v : vals) outNormals.push_back(normVec(v[0], v[1], v[2]));
            break;
        }
        case DIE_D20:
        default: {
            // 20 icosahedron face normals using golden ratio vertices
            const float phi = 1.61803398875f;
            const Vec3 v[12] = {
                normVec(-1,  phi, 0), normVec( 1,  phi, 0), normVec(-1, -phi, 0), normVec( 1, -phi, 0),
                normVec( 0, -1,  phi), normVec( 0,  1,  phi), normVec( 0, -1, -phi), normVec( 0,  1, -phi),
                normVec( phi, 0, -1), normVec( phi, 0,  1), normVec(-phi, 0, -1), normVec(-phi, 0,  1)
            };
            const int faces[20][3] = {
                {0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},
                {1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},
                {3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},
                {4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1}
            };
            for (auto& f : faces) {
                Vec3 c{
                    (v[f[0]].x + v[f[1]].x + v[f[2]].x) / 3.0f,
                    (v[f[0]].y + v[f[1]].y + v[f[2]].y) / 3.0f,
                    (v[f[0]].z + v[f[1]].z + v[f[2]].z) / 3.0f
                };
                outNormals.push_back(normVec(c.x, c.y, c.z));
            }
            break;
        }
    }
}

void DiceRoller::spawnDie(DieBody& d, DieShape shape, float offsetX, float offsetY, uint32_t& rngState) {
    d.active        = true;
    d.settled       = false;
    d.shape         = shape;
    d.sides         = (shape == DIE_D100) ? 10 : sidesForShape(shape);
    d.result        = static_cast<int>(diceRng(rngState) % static_cast<uint32_t>(d.sides)) + 1;
    if (shape == DIE_D100) d.result = (d.result % 10) * 10; // 00, 10 .. 90

    d.pos           = {offsetX + randFloat(rngState, -0.15f, 0.15f),
                       offsetY + randFloat(rngState, -0.15f, 0.15f),
                       randFloat(rngState, 0.65f, 0.95f)};
    d.vel           = {randFloat(rngState, -2.4f, 2.4f), randFloat(rngState, -1.8f, 1.8f), randFloat(rngState, -0.4f, 0.8f)};
    d.angVel        = {randFloat(rngState, 6.0f, 14.0f), randFloat(rngState, 6.0f, 14.0f), randFloat(rngState, 4.0f, 10.0f)};
    d.rot           = normalizeQuat({randFloat(rngState, -1, 1), randFloat(rngState, -1, 1),
                                     randFloat(rngState, -1, 1), randFloat(rngState, -1, 1)});
    d.framesRolling = 0;

    std::vector<Vec3> normals;
    getFaceNormals(shape == DIE_D100 ? DIE_D10 : shape, normals);
    int faceIdx = std::clamp(
        (shape == DIE_D100 ? (d.result / 10) : (d.result - 1)),
        0, static_cast<int>(normals.size()) - 1
    );
    d.targetRot = rotationBetween(normals[static_cast<size_t>(faceIdx)], {0.0f, 0.0f, 1.0f});
}

void DiceRoller::roll(DieShape shape, int modifier, RollAdvantageMode advMode, uint32_t& rngState) {
    rollPool(shape, 1, modifier, advMode, rngState);
}

void DiceRoller::rollPool(DieShape shape, int count, int modifier, RollAdvantageMode advMode, uint32_t& rngState) {
    activeShape_    = shape;
    poolCount_      = std::clamp(count, 1, MAX_LOGICAL_DICE);
    activeModifier_ = std::clamp(modifier, -20, 50);
    activeAdvMode_  = (shape == DIE_D20 && poolCount_ == 1) ? advMode : ROLL_NORMAL;
    recorded_       = false;
    extraRolls_.clear();

    for (auto& d : dice_) {
        d.active  = false;
        d.settled = true;
    }

    if (shape == DIE_D100) {
        poolCount_   = 1;
        activeCount_ = 2;
        spawnDie(dice_[0], DIE_D100, -0.45f, 0.0f, rngState);
        spawnDie(dice_[1], DIE_D10,   0.45f, 0.0f, rngState);
    } else if (activeAdvMode_ != ROLL_NORMAL) {
        activeCount_ = 2;
        spawnDie(dice_[0], DIE_D20, -0.45f, 0.0f, rngState);
        spawnDie(dice_[1], DIE_D20,  0.45f, 0.0f, rngState);
    } else {
        activeCount_ = std::min(poolCount_, MAX_POOL_DICE);
        if (activeCount_ == 1) {
            spawnDie(dice_[0], shape, 0.0f, 0.0f, rngState);
        } else {
            int cols = (activeCount_ <= 2) ? 2 : (activeCount_ <= 6 ? 3 : 4);
            int rows = (activeCount_ + cols - 1) / cols;
            for (int i = 0; i < activeCount_; ++i) {
                int c = i % cols;
                int r = i / cols;
                float ox = (cols > 1) ? ((static_cast<float>(c) / (cols - 1)) - 0.5f) * 1.6f : 0.0f;
                float oy = (rows > 1) ? (0.5f - (static_cast<float>(r) / (rows - 1))) * 0.9f : 0.0f;
                spawnDie(dice_[static_cast<size_t>(i)], shape, ox, oy, rngState);
            }
        }
        int sides = sidesForShape(shape);
        for (int i = activeCount_; i < poolCount_; ++i) {
            extraRolls_.push_back(static_cast<int>(diceRng(rngState) % static_cast<uint32_t>(sides)) + 1);
        }
    }
}

DiceExpr DiceRoller::parseDiceExpr(const char* text) {
    DiceExpr out;
    if (!text) return out;

    const char* p = text;
    while (*p) {
        if (*p >= '0' && *p <= '9') {
            int count = 0;
            const char* q = p;
            while (*q >= '0' && *q <= '9' && count < 100) {
                count = count * 10 + (*q - '0');
                ++q;
            }
            if ((*q == 'd' || *q == 'D') && q[1] >= '0' && q[1] <= '9') {
                ++q;
                int sides = 0;
                while (*q >= '0' && *q <= '9' && sides < 1000) {
                    sides = sides * 10 + (*q - '0');
                    ++q;
                }
                DieShape sh = DIE_D6;
                bool known = true;
                switch (sides) {
                    case 4:   sh = DIE_D4; break;
                    case 6:   sh = DIE_D6; break;
                    case 8:   sh = DIE_D8; break;
                    case 10:  sh = DIE_D10; break;
                    case 12:  sh = DIE_D12; break;
                    case 20:  sh = DIE_D20; break;
                    case 100: sh = DIE_D100; break;
                    default:  known = false; break;
                }
                if (known && count >= 1 && count <= 30) {
                    int mod = 0;
                    const char* m = q;
                    while (*m == ' ') ++m;
                    if (*m == '+' || *m == '-') {
                        int sign = (*m == '-') ? -1 : 1;
                        ++m;
                        while (*m == ' ') ++m;
                        if (*m >= '0' && *m <= '9') {
                            int val = 0;
                            while (*m >= '0' && *m <= '9' && val < 1000) {
                                val = val * 10 + (*m - '0');
                                ++m;
                            }
                            mod = sign * val;
                        }
                    }
                    out.valid = true;
                    out.count = count;
                    out.sides = sides;
                    out.mod   = mod;
                    out.shape = sh;
                    return out;
                }
            }
        }
        ++p;
    }
    return out;
}

void DiceRoller::applyImpulse(float vx, float vy) {
    float mag = std::sqrt(vx * vx + vy * vy);
    for (int i = 0; i < activeCount_; ++i) {
        DieBody& d = dice_[static_cast<size_t>(i)];
        if (!d.active) continue;
        d.vel.x += vx;
        d.vel.y += vy;
        d.vel.z = std::max(d.vel.z, 1.2f + std::min(mag * 0.4f, 2.0f));
        d.angVel.x += vy * 2.5f;
        d.angVel.y -= vx * 2.5f;
        d.settled = false;
    }
}

void DiceRoller::stepDie(DieBody& d, float dt) {
    if (!d.active || d.settled) return;
    ++d.framesRolling;

    // Gravity toward felt surface (z = 0)
    d.vel.z -= 9.8f * dt;
    d.pos.x += d.vel.x * dt;
    d.pos.y += d.vel.y * dt;
    d.pos.z += d.vel.z * dt;

    // Felt floor bounce
    if (d.pos.z <= 0.0f) {
        d.pos.z = 0.0f;
        if (std::fabs(d.vel.z) > 0.35f) {
            d.vel.z = -d.vel.z * 0.45f;
        } else {
            d.vel.z = 0.0f;
        }
        d.vel.x *= 0.88f;
        d.vel.y *= 0.88f;
        d.angVel.x *= 0.86f;
        d.angVel.y *= 0.86f;
        d.angVel.z *= 0.86f;
    }

    // Walnut tray wall bounces
    const float maxX = TRAY_HALF_W - 0.25f;
    const float maxY = TRAY_HALF_H - 0.25f;
    if (d.pos.x < -maxX) { d.pos.x = -maxX; d.vel.x =  std::fabs(d.vel.x) * 0.6f; }
    if (d.pos.x >  maxX) { d.pos.x =  maxX; d.vel.x = -std::fabs(d.vel.x) * 0.6f; }
    if (d.pos.y < -maxY) { d.pos.y = -maxY; d.vel.y =  std::fabs(d.vel.y) * 0.6f; }
    if (d.pos.y >  maxY) { d.pos.y =  maxY; d.vel.y = -std::fabs(d.vel.y) * 0.6f; }

    // Integrate spin
    Quat spin{0.0f, d.angVel.x * 0.5f * dt, d.angVel.y * 0.5f * dt, d.angVel.z * 0.5f * dt};
    Quat dq = mulQuat(spin, d.rot);
    d.rot = normalizeQuat({d.rot.w + dq.w, d.rot.x + dq.x, d.rot.y + dq.y, d.rot.z + dq.z});

    // Corrective slerp toward winning face-up orientation as die slows down
    float speed = std::sqrt(d.vel.x * d.vel.x + d.vel.y * d.vel.y + d.vel.z * d.vel.z);
    if (d.framesRolling > 28 || speed < 0.75f) {
        d.rot = slerpQuat(d.rot, d.targetRot, 0.18f);
    }

    if (d.framesRolling >= 54 || (d.framesRolling > 34 && speed < 0.12f && d.pos.z <= 0.01f)) {
        d.pos.z   = 0.0f;
        d.vel     = {0.0f, 0.0f, 0.0f};
        d.angVel  = {0.0f, 0.0f, 0.0f};
        d.rot     = d.targetRot;
        d.settled = true;
    }
}

void DiceRoller::step(float dt) {
    for (int i = 0; i < activeCount_; ++i) {
        stepDie(dice_[static_cast<size_t>(i)], dt);
    }

    // Pairwise sphere-sphere repulsion when multiple dice are rolling
    const float minSep = (activeCount_ <= 2) ? 0.55f : 0.42f;
    for (int i = 0; i < activeCount_; ++i) {
        DieBody& a = dice_[static_cast<size_t>(i)];
        if (!a.active) continue;
        for (int j = i + 1; j < activeCount_; ++j) {
            DieBody& b = dice_[static_cast<size_t>(j)];
            if (!b.active || (a.settled && b.settled)) continue;
            float dx = b.pos.x - a.pos.x;
            float dy = b.pos.y - a.pos.y;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > 0.001f && dist < minSep) {
                float push = (minSep - dist) * 0.5f;
                float nx = dx / dist, ny = dy / dist;
                a.pos.x -= nx * push;
                a.pos.y -= ny * push;
                b.pos.x += nx * push;
                b.pos.y += ny * push;
            }
        }
    }

    if (!recorded_ && !isRolling()) {
        finalizeRoll();
    }
}

void DiceRoller::finishImmediately() {
    while (isRolling()) {
        step(1.0f / 60.0f);
    }
}

bool DiceRoller::isRolling() const {
    for (int i = 0; i < activeCount_; ++i) {
        const DieBody& d = dice_[static_cast<size_t>(i)];
        if (d.active && !d.settled) return true;
    }
    return false;
}

void DiceRoller::finalizeRoll() {
    recorded_ = true;
    std::vector<int> rolls;
    int raw = 0;

    if (activeShape_ == DIE_D100) {
        int tens = dice_[0].result; // 0, 10 .. 90
        int ones = dice_[1].result % 10; // 1..9 or 0
        raw = tens + ones;
        if (raw == 0) raw = 100;
        rolls.push_back(raw);
    } else if (activeAdvMode_ == ROLL_ADVANTAGE) {
        raw = std::max(dice_[0].result, dice_[1].result);
        rolls.push_back(dice_[0].result);
        rolls.push_back(dice_[1].result);
    } else if (activeAdvMode_ == ROLL_DISADVANTAGE) {
        raw = std::min(dice_[0].result, dice_[1].result);
        rolls.push_back(dice_[0].result);
        rolls.push_back(dice_[1].result);
    } else {
        for (int i = 0; i < activeCount_; ++i) {
            int r = dice_[static_cast<size_t>(i)].result;
            rolls.push_back(r);
            raw += r;
        }
        for (int r : extraRolls_) {
            rolls.push_back(r);
            raw += r;
        }
    }

    lastTotal_ = raw + activeModifier_;

    char lbl[80];
    if (activeShape_ == DIE_D100) {
        if (activeModifier_ != 0) {
            std::snprintf(lbl, sizeof(lbl), "d100 (%d)%+d = %d", raw, activeModifier_, lastTotal_);
        } else {
            std::snprintf(lbl, sizeof(lbl), "d100 = %d", lastTotal_);
        }
    } else if (activeAdvMode_ != ROLL_NORMAL) {
        std::snprintf(lbl, sizeof(lbl), "d20 (%d,%d %s)%+d = %d",
                      dice_[0].result, dice_[1].result,
                      activeAdvMode_ == ROLL_ADVANTAGE ? "Adv" : "Dis",
                      activeModifier_, lastTotal_);
    } else if (poolCount_ > 1) {
        if (activeModifier_ != 0) {
            std::snprintf(lbl, sizeof(lbl), "%d%s (%d)%+d = %d",
                          poolCount_, shapeLabel(activeShape_), raw, activeModifier_, lastTotal_);
        } else {
            std::snprintf(lbl, sizeof(lbl), "%d%s = %d",
                          poolCount_, shapeLabel(activeShape_), lastTotal_);
        }
    } else if (activeModifier_ != 0) {
        std::snprintf(lbl, sizeof(lbl), "%s (%d)%+d = %d",
                      shapeLabel(activeShape_), dice_[0].result, activeModifier_, lastTotal_);
    } else {
        std::snprintf(lbl, sizeof(lbl), "%s = %d", shapeLabel(activeShape_), lastTotal_);
    }

    RollHistoryEntry entry{
        activeShape_, poolCount_, activeAdvMode_, activeModifier_,
        dice_[0].result, dice_[1].result, lastTotal_, std::move(rolls), lbl
    };
    history_.insert(history_.begin(), std::move(entry));
    if (history_.size() > 8) history_.resize(8);
}

} // namespace dmtk
