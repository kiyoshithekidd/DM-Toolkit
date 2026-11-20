#pragma once

#include "srd_db.h"
#include <cstdint>
#include <string>
#include <vector>

namespace dmtk {

struct PartyMember {
    std::string name;
    std::string className = "Adventurer";
    int8_t      initBonus = 0;
    uint8_t     level     = 1;
    uint8_t     ac        = 12;
    int         maxHp     = 18;
    int         currentHp = 18;
    bool        active    = true;
};

struct EncounterSlot {
    uint16_t monsterId = 0;
    uint8_t  count     = 1;
};

struct SavedEncounter {
    std::string                name;
    std::vector<EncounterSlot> slots;
};

struct Combatant {
    uint32_t    instanceId     = 0;
    std::string displayName;
    bool        isPlayer       = false;
    uint16_t    monsterId      = 0xFFFF;
    int         initRoll       = 10;
    int8_t      initBonus      = 0;
    int         currentHp      = 0;
    int         maxHp          = 0;
    uint16_t    armorClass     = 10;
    uint16_t    conditionsMask = 0;
};

class CombatTracker {
public:
    // Deterministic Xorshift32 RNG helper (can be seeded with svcGetSystemTick on 3DS)
    static int rollD20(uint32_t& rngState);

    void clear();
    void startEncounter(const SavedEncounter& encounter,
                        const std::vector<PartyMember>& party,
                        const SrdDatabase& db,
                        uint32_t& rngState,
                        bool groupIdenticalMonsters = true);

    // Adds one or more monsters mid-fight, rolls initiative, inserts into sorted order,
    // and keeps activeTurnIndex pointing at the same combatant.
    int addMonsterMidFight(uint16_t monsterId, int count,
                           const SrdDatabase& db,
                           uint32_t& rngState);

    // Quick-start a fight with party + a single monster (when tracker is empty)
    void addPartyIfMissing(const std::vector<PartyMember>& party, uint32_t& rngState);

    void applyHpDelta(int index, int delta);
    void setHp(int index, int hp);
    bool canUndoHp() const { return lastHpEditId_ != 0; }
    bool undoLastHp();
    int  lastHpDelta() const { return lastHpDelta_; }
    uint32_t lastHpEditId() const { return lastHpEditId_; }

    void toggleCondition(int index, int conditionIdx);
    void clearConditions(int index);
    void setInitiative(int index, int newInit);
    void removeCombatant(int index);

    void nextTurn();
    void prevTurn();

    bool isInCombat() const { return inCombat_ && !combatants_.empty(); }
    int  roundNumber() const { return roundNumber_; }
    int  activeTurnIndex() const { return activeTurnIndex_; }
    int  selectedIndex() const { return selectedIndex_; }
    void setSelectedIndex(int idx);

    const std::vector<Combatant>& combatants() const { return combatants_; }
    std::vector<Combatant>&       combatantsMut() { return combatants_; }

    // Serialization helpers for active fight auto-save
    void restoreState(int roundNum, int activeIdx, std::vector<Combatant>&& list);

private:
    bool                   inCombat_        = false;
    int                    roundNumber_     = 1;
    int                    activeTurnIndex_ = 0;
    int                    selectedIndex_   = 0;
    uint32_t               nextInstanceId_  = 1;
    uint32_t               lastHpEditId_    = 0;
    int                    lastHpPrevValue_ = 0;
    int                    lastHpDelta_     = 0;
    std::vector<Combatant> combatants_;

    int countExistingMonsters(uint16_t monsterId) const;
    void sortPreservingActive();
};

} // namespace dmtk
