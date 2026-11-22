#include "combat_tracker.h"

#include <algorithm>
#include <cstdio>

namespace dmtk {

int CombatTracker::rollD20(uint32_t& rngState) {
    if (rngState == 0) rngState = 0x5EED1234u;
    uint32_t x = rngState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rngState = x;
    return static_cast<int>(x % 20u) + 1;
}

void CombatTracker::clear() {
    inCombat_        = false;
    roundNumber_     = 1;
    activeTurnIndex_ = 0;
    selectedIndex_   = 0;
    lastHpEditId_    = 0;
    lastHpPrevValue_ = 0;
    lastHpDelta_     = 0;
    combatants_.clear();
}

void CombatTracker::setSelectedIndex(int idx) {
    if (combatants_.empty()) {
        selectedIndex_ = 0;
        return;
    }
    selectedIndex_ = std::clamp(idx, 0, static_cast<int>(combatants_.size()) - 1);
}

int CombatTracker::countExistingMonsters(uint16_t monsterId) const {
    int n = 0;
    for (const auto& c : combatants_) {
        if (!c.isPlayer && c.monsterId == monsterId) ++n;
    }
    return n;
}

void CombatTracker::sortPreservingActive() {
    if (combatants_.empty()) {
        activeTurnIndex_ = 0;
        selectedIndex_   = 0;
        return;
    }

    uint32_t activeId = 0;
    if (activeTurnIndex_ >= 0 && activeTurnIndex_ < static_cast<int>(combatants_.size())) {
        activeId = combatants_[static_cast<size_t>(activeTurnIndex_)].instanceId;
    }
    uint32_t selectedId = 0;
    if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(combatants_.size())) {
        selectedId = combatants_[static_cast<size_t>(selectedIndex_)].instanceId;
    }

    std::stable_sort(combatants_.begin(), combatants_.end(),
                     [](const Combatant& a, const Combatant& b) {
                         if (a.initRoll != b.initRoll) return a.initRoll > b.initRoll;
                         if (a.initBonus != b.initBonus) return a.initBonus > b.initBonus;
                         return a.instanceId < b.instanceId;
                     });

    if (activeId != 0) {
        for (size_t i = 0; i < combatants_.size(); ++i) {
            if (combatants_[i].instanceId == activeId) {
                activeTurnIndex_ = static_cast<int>(i);
                break;
            }
        }
    }
    if (selectedId != 0) {
        for (size_t i = 0; i < combatants_.size(); ++i) {
            if (combatants_[i].instanceId == selectedId) {
                selectedIndex_ = static_cast<int>(i);
                break;
            }
        }
    }
}

void CombatTracker::addPartyIfMissing(const std::vector<PartyMember>& party, uint32_t& rngState) {
    bool hasAnyPlayer = false;
    for (const auto& c : combatants_) {
        if (c.isPlayer) { hasAnyPlayer = true; break; }
    }
    if (hasAnyPlayer) return;

    for (const auto& p : party) {
        if (!p.active || p.name.empty()) continue;
        Combatant c;
        c.instanceId  = nextInstanceId_++;
        c.displayName = p.name;
        c.isPlayer    = true;
        c.monsterId   = 0xFFFF;
        c.initBonus   = p.initBonus;
        c.initRoll    = rollD20(rngState) + p.initBonus;
        c.maxHp       = std::max(1, p.maxHp);
        c.currentHp   = std::clamp(p.currentHp, 0, c.maxHp);
        c.armorClass  = p.ac;
        combatants_.push_back(c);
    }
}

void CombatTracker::startEncounter(const SavedEncounter& encounter,
                                   const std::vector<PartyMember>& party,
                                   const SrdDatabase& db,
                                   uint32_t& rngState,
                                   bool groupIdenticalMonsters) {
    clear();
    addPartyIfMissing(party, rngState);

    for (const auto& slot : encounter.slots) {
        MonsterView mv{};
        if (!db.getMonster(slot.monsterId, mv)) continue;
        int groupRoll = rollD20(rngState) + mv.initBonus;
        int qty = std::clamp(static_cast<int>(slot.count), 1, 20);
        for (int i = 0; i < qty; ++i) {
            Combatant c;
            c.instanceId = nextInstanceId_++;
            if (qty > 1) {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%.36s %d", mv.name, i + 1);
                c.displayName = buf;
            } else {
                c.displayName = mv.name;
            }
            c.isPlayer   = false;
            c.monsterId  = mv.id;
            c.initBonus  = mv.initBonus;
            c.initRoll   = groupIdenticalMonsters ? groupRoll : (rollD20(rngState) + mv.initBonus);
            c.maxHp      = mv.hitPoints;
            c.currentHp  = mv.hitPoints;
            c.armorClass = mv.armorClass;
            combatants_.push_back(c);
        }
    }

    if (!combatants_.empty()) {
        inCombat_ = true;
        roundNumber_ = 1;
        // Sort fresh combat without preserving an old active turn
        std::stable_sort(combatants_.begin(), combatants_.end(),
                         [](const Combatant& a, const Combatant& b) {
                             if (a.initRoll != b.initRoll) return a.initRoll > b.initRoll;
                             if (a.initBonus != b.initBonus) return a.initBonus > b.initBonus;
                             return a.instanceId < b.instanceId;
                         });
        activeTurnIndex_ = 0;
        selectedIndex_   = 0;
    }
}

int CombatTracker::addMonsterMidFight(uint16_t monsterId, int count,
                                      const SrdDatabase& db,
                                      uint32_t& rngState) {
    MonsterView mv{};
    if (!db.getMonster(monsterId, mv)) return -1;

    int existing = countExistingMonsters(monsterId);
    int qty = std::clamp(count, 1, 20);
    int initRoll = rollD20(rngState) + mv.initBonus;
    uint32_t firstNewId = 0;

    for (int i = 0; i < qty; ++i) {
        Combatant c;
        c.instanceId = nextInstanceId_++;
        if (firstNewId == 0) firstNewId = c.instanceId;

        int num = existing + i + 1;
        if (existing > 0 || qty > 1) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%.36s %d", mv.name, num);
            c.displayName = buf;
        } else {
            c.displayName = mv.name;
        }
        c.isPlayer   = false;
        c.monsterId  = mv.id;
        c.initBonus  = mv.initBonus;
        c.initRoll   = initRoll;
        c.maxHp      = mv.hitPoints;
        c.currentHp  = mv.hitPoints;
        c.armorClass = mv.armorClass;
        combatants_.push_back(c);
    }

    bool wasEmpty = !inCombat_;
    inCombat_ = true;
    if (wasEmpty) {
        roundNumber_     = 1;
        activeTurnIndex_ = 0;
        selectedIndex_   = 0;
    }

    sortPreservingActive();

    for (size_t i = 0; i < combatants_.size(); ++i) {
        if (combatants_[i].instanceId == firstNewId) {
            selectedIndex_ = static_cast<int>(i);
            return static_cast<int>(i);
        }
    }
    return selectedIndex_;
}

void CombatTracker::applyHpDelta(int index, int delta) {
    if (index < 0 || index >= static_cast<int>(combatants_.size())) return;
    Combatant& c = combatants_[static_cast<size_t>(index)];
    int prev = c.currentHp;
    c.currentHp = std::clamp(c.currentHp + delta, 0, c.maxHp);
    if (c.currentHp != prev) {
        lastHpEditId_    = c.instanceId;
        lastHpPrevValue_ = prev;
        lastHpDelta_     = c.currentHp - prev;
    }
}

void CombatTracker::setHp(int index, int hp) {
    if (index < 0 || index >= static_cast<int>(combatants_.size())) return;
    Combatant& c = combatants_[static_cast<size_t>(index)];
    int prev = c.currentHp;
    c.currentHp = std::clamp(hp, 0, c.maxHp);
    if (c.currentHp != prev) {
        lastHpEditId_    = c.instanceId;
        lastHpPrevValue_ = prev;
        lastHpDelta_     = c.currentHp - prev;
    }
}

bool CombatTracker::undoLastHp() {
    if (lastHpEditId_ == 0) return false;
    for (size_t i = 0; i < combatants_.size(); ++i) {
        if (combatants_[i].instanceId == lastHpEditId_) {
            combatants_[i].currentHp = std::clamp(lastHpPrevValue_, 0, combatants_[i].maxHp);
            selectedIndex_ = static_cast<int>(i);
            lastHpEditId_  = 0;
            lastHpDelta_   = 0;
            return true;
        }
    }
    lastHpEditId_ = 0;
    lastHpDelta_  = 0;
    return false;
}

void CombatTracker::toggleCondition(int index, int conditionIdx) {
    if (index < 0 || index >= static_cast<int>(combatants_.size())) return;
    if (conditionIdx < 0 || conditionIdx >= CONDITION_COUNT) return;
    combatants_[static_cast<size_t>(index)].conditionsMask ^= static_cast<uint16_t>(1u << conditionIdx);
}

void CombatTracker::clearConditions(int index) {
    if (index < 0 || index >= static_cast<int>(combatants_.size())) return;
    combatants_[static_cast<size_t>(index)].conditionsMask = 0;
}

void CombatTracker::setInitiative(int index, int newInit) {
    if (index < 0 || index >= static_cast<int>(combatants_.size())) return;
    combatants_[static_cast<size_t>(index)].initRoll = std::clamp(newInit, -5, 40);
    sortPreservingActive();
}

void CombatTracker::removeCombatant(int index) {
    if (index < 0 || index >= static_cast<int>(combatants_.size())) return;
    combatants_.erase(combatants_.begin() + index);
    if (combatants_.empty()) {
        clear();
        return;
    }
    if (index < activeTurnIndex_) {
        --activeTurnIndex_;
    } else if (activeTurnIndex_ >= static_cast<int>(combatants_.size())) {
        activeTurnIndex_ = 0;
    }
    if (selectedIndex_ >= static_cast<int>(combatants_.size())) {
        selectedIndex_ = static_cast<int>(combatants_.size()) - 1;
    }
}

void CombatTracker::nextTurn() {
    if (combatants_.empty()) return;
    ++activeTurnIndex_;
    if (activeTurnIndex_ >= static_cast<int>(combatants_.size())) {
        activeTurnIndex_ = 0;
        ++roundNumber_;
    }
    selectedIndex_ = activeTurnIndex_;
}

void CombatTracker::prevTurn() {
    if (combatants_.empty()) return;
    --activeTurnIndex_;
    if (activeTurnIndex_ < 0) {
        activeTurnIndex_ = static_cast<int>(combatants_.size()) - 1;
        if (roundNumber_ > 1) --roundNumber_;
    }
    selectedIndex_ = activeTurnIndex_;
}

void CombatTracker::restoreState(int roundNum, int activeIdx, std::vector<Combatant>&& list) {
    combatants_ = std::move(list);
    inCombat_ = !combatants_.empty();
    roundNumber_ = std::max(1, roundNum);
    uint32_t maxId = 0;
    for (auto& c : combatants_) {
        if (c.instanceId == 0) c.instanceId = ++maxId;
        else if (c.instanceId > maxId) maxId = c.instanceId;
    }
    nextInstanceId_ = maxId + 1;
    if (combatants_.empty()) {
        activeTurnIndex_ = 0;
        selectedIndex_   = 0;
    } else {
        activeTurnIndex_ = std::clamp(activeIdx, 0, static_cast<int>(combatants_.size()) - 1);
        selectedIndex_   = activeTurnIndex_;
    }
}

} // namespace dmtk
