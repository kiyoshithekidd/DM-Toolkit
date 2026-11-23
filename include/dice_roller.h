#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace dmtk {

enum DieShape : uint8_t {
    DIE_D4   = 0,
    DIE_D6   = 1,
    DIE_D8   = 2,
    DIE_D10  = 3,
    DIE_D12  = 4,
    DIE_D20  = 5,
    DIE_D100 = 6,
    DIE_SHAPE_COUNT = 7
};

enum RollAdvantageMode : uint8_t {
    ROLL_NORMAL       = 0,
    ROLL_ADVANTAGE    = 1,
    ROLL_DISADVANTAGE = 2
};

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

struct Quat {
    float w = 1.0f, x = 0.0f, y = 0.0f, z = 0.0f;
};

struct DieBody {
    bool     active    = false;
    bool     settled   = false;
    DieShape shape     = DIE_D20;
    int      sides     = 20;
    int      result    = 20;
    Vec3     pos;
    Vec3     vel;
    Vec3     angVel;
    Quat     rot;
    Quat     targetRot;
    int      framesRolling = 0;
};

struct RollHistoryEntry {
    DieShape          shape;
    int               count;
    RollAdvantageMode advMode;
    int               modifier;
    int               dieA;
    int               dieB;
    int               total;
    std::vector<int>  rolls;
    std::string       label;
};

struct DiceExpr {
    bool     valid = false;
    int      count = 1;
    int      sides = 6;
    int      mod   = 0;
    DieShape shape = DIE_D6;
};

class DiceRoller {
public:
    static constexpr int MAX_POOL_DICE = 12;

    static int sidesForShape(DieShape s);
    static const char* shapeLabel(DieShape s);
    static DiceExpr parseDiceExpr(const char* text);
    static Vec3 rotateVec(const Quat& q, const Vec3& v);
    static Quat normalizeQuat(const Quat& q);
    static Quat slerpQuat(const Quat& a, const Quat& b, float t);
    static Quat rotationBetween(const Vec3& from, const Vec3& to);

    // Returns the face normals for a given polyhedral die shape
    static void getFaceNormals(DieShape shape, std::vector<Vec3>& outNormals);

    void roll(DieShape shape, int modifier, RollAdvantageMode advMode, uint32_t& rngState);
    void rollPool(DieShape shape, int count, int modifier, RollAdvantageMode advMode, uint32_t& rngState);
    void applyImpulse(float vx, float vy);
    void step(float dt = 1.0f / 60.0f);
    void finishImmediately();

    bool isRolling() const;
    DieShape currentShape() const { return activeShape_; }
    int currentPoolCount() const { return poolCount_; }
    int activeDieCount() const { return activeCount_; }
    RollAdvantageMode currentAdvMode() const { return activeAdvMode_; }
    int currentModifier() const { return activeModifier_; }
    int lastTotal() const { return lastTotal_; }
    const DieBody& dieA() const { return dice_[0]; }
    const DieBody& dieB() const { return dice_[1]; }
    const DieBody& dieAt(int idx) const { return dice_[static_cast<size_t>(idx)]; }
    const std::vector<RollHistoryEntry>& history() const { return history_; }

    // Tray bounds in normalized 3D tray coordinates
    static constexpr float TRAY_HALF_W = 1.45f;
    static constexpr float TRAY_HALF_H = 0.90f;

private:
    std::array<DieBody, MAX_POOL_DICE> dice_{};
    int                           activeCount_    = 0;
    int                           poolCount_      = 1;
    DieShape                      activeShape_    = DIE_D20;
    RollAdvantageMode             activeAdvMode_  = ROLL_NORMAL;
    int                           activeModifier_ = 0;
    int                           lastTotal_      = 20;
    bool                          recorded_       = true;
    std::vector<RollHistoryEntry> history_;

    void spawnDie(DieBody& d, DieShape shape, float offsetX, float offsetY, uint32_t& rngState);
    void stepDie(DieBody& d, float dt);
    void finalizeRoll();
};

} // namespace dmtk
