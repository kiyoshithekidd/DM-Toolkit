#pragma once

#include "srd_db.h"
#include <cstdint>
#include <string>
#include <vector>

namespace dmtk {

struct PartyMember {
    std::string name;
    std::string className         = "Adventurer";
    int8_t      initBonus         = 0;
    uint8_t     level             = 1;
    uint8_t     ac                = 12;
    int         maxHp             = 18;
    int         currentHp         = 18;
    bool        active            = true;
    uint8_t     passivePerception = 12;
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
    int         tempHp         = 0;
    uint16_t    armorClass     = 10;
    uint16_t    conditionsMask = 0;
    bool        concentrating  = false;
    uint8_t     deathSuccesses = 0;
    uint8_t     deathFailures  = 0;
    bool        stabilized     = false;
    uint8_t     legendActions  = 0;
    uint8_t     legendMax      = 0;
    bool        rechargeReady  = true;
    uint8_t     rechargeMin    = 0; // 0 = none, 5 = Recharge 5-6, 6 = Recharge 6
};

class CombatTracker {
public:
    // Deterministic Xorshift32 RNG helper (can be seeded with svcGetSystemTick on 3DS)
    static int rollD20(uint32_t& rngState);
    static int concentrationCheckDc(int damageTaken);

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

    // Add a Lair Action marker at Initiative 20
    int addLairAction();

    void applyHpDelta(int index, int delta);
    void setHp(int index, int hp);
    void setTempHp(int index, int amount);
    bool canUndoHp() const { return lastHpEditId_ != 0; }
    bool undoLastHp();
    int  lastHpDelta() const { return lastHpDelta_; }
    uint32_t lastHpEditId() const { return lastHpEditId_; }

    void toggleCondition(int index, int conditionIdx);
    void clearConditions(int index);
    void toggleConcentration(int index);
    void recordDeathSave(int index, bool success);
    void stabilize(int index);
    void useLegendaryAction(int index);
    void resetLegendaryActions(int index);
    int  rollRecharge(int index, uint32_t& rngState);
    void toggleRechargeReady(int index);
    void setInitiative(int index, int newInit);
    void removeCombatant(int index);

    uint32_t computeDefeatedXp(const SrdDatabase& db) const;
    void     remapDeletedHomebrewMonster(uint16_t deletedId);

    void setAutoSkipDead(bool skip) { autoSkipDead_ = skip; }
    bool autoSkipDead() const { return autoSkipDead_; }

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
    bool                   inCombat_         = false;
    bool                   autoSkipDead_     = true;
    int                    roundNumber_      = 1;
    int                    activeTurnIndex_  = 0;
    int                    selectedIndex_    = 0;
    uint32_t               nextInstanceId_   = 1;
    uint32_t               lastHpEditId_     = 0;
    int                    lastHpPrevValue_  = 0;
    int                    lastHpPrevTempHp_ = 0;
    bool                   lastHpPrevConc_   = false;
    uint8_t                lastHpPrevDsSucc_ = 0;
    uint8_t                lastHpPrevDsFail_ = 0;
    bool                   lastHpPrevStab_   = false;
    int                    lastHpDelta_      = 0;
    std::vector<Combatant> combatants_;

    int countExistingMonsters(uint16_t monsterId) const;
    void sortPreservingActive();
    static void detectBossMechanics(uint16_t monsterId, const SrdDatabase& db, Combatant& c);
};

} // namespace dmtk
