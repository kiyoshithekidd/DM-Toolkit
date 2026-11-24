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

void CombatTracker::detectBossMechanics(uint16_t monsterId, const SrdDatabase& db, Combatant& c) {
    MonsterView mv{};
    if (!db.getMonster(monsterId, mv)) return;
    bool hasLegendary = false;
    uint8_t rechMin = 0;
    for (int i = 0; i < mv.actionCount; ++i) {
        ActionView av{};
        if (!db.getMonsterAction(monsterId, i, av)) continue;
        if (av.sectionType == 3) {
            hasLegendary = true;
        }
        // Scan action name/desc for "Recharge 5-6" or "Recharge 6" or "Recharge 4-6"
        if (rechMin == 0 && av.name) {
            const char* p = av.name;
            while (*p) {
                if ((p[0] == 'R' || p[0] == 'r') &&
                    (p[1] == 'e' || p[1] == 'E') &&
                    (p[2] == 'c' || p[2] == 'C') &&
                    (p[3] == 'h' || p[3] == 'H')) {
                    const char* q = p + 4;
                    while (*q && (*q < '1' || *q > '6')) ++q;
                    if (*q >= '2' && *q <= '6') {
                        rechMin = static_cast<uint8_t>(*q - '0');
                        break;
                    }
                }
                ++p;
            }
        }
    }
    if (hasLegendary) {
        c.legendMax     = 3;
        c.legendActions = 3;
    }
    if (rechMin > 0) {
        c.rechargeMin   = rechMin;
        c.rechargeReady = true;
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
            detectBossMechanics(mv.id, db, c);
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
        detectBossMechanics(mv.id, db, c);
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

int CombatTracker::addLairAction() {
    for (size_t i = 0; i < combatants_.size(); ++i) {
        if (!combatants_[i].isPlayer && combatants_[i].monsterId == 0xFFFE) {
            selectedIndex_ = static_cast<int>(i);
            return static_cast<int>(i);
        }
    }
    Combatant c;
    c.instanceId  = nextInstanceId_++;
    c.displayName = "Lair Action";
    c.isPlayer    = false;
    c.monsterId   = 0xFFFE;
    c.initRoll    = 20;
    c.initBonus   = -10; // Loses initiative ties per SRD
    c.maxHp       = 1;
    c.currentHp   = 1;
    c.armorClass  = 0;
    combatants_.push_back(c);
    if (!inCombat_) {
        inCombat_ = true;
        roundNumber_ = 1;
        activeTurnIndex_ = 0;
    }
    sortPreservingActive();
    for (size_t i = 0; i < combatants_.size(); ++i) {
        if (combatants_[i].instanceId == c.instanceId) {
            selectedIndex_ = static_cast<int>(i);
            return static_cast<int>(i);
        }
    }
    return selectedIndex_;
}

int CombatTracker::concentrationCheckDc(int damageTaken) {
    return std::max(10, damageTaken / 2);
}

void CombatTracker::applyHpDelta(int index, int delta) {
    if (index < 0 || index >= static_cast<int>(combatants_.size())) return;
    Combatant& c = combatants_[static_cast<size_t>(index)];
    int prevHp   = c.currentHp;
    int prevTemp = c.tempHp;
    bool prevConc = c.concentrating;
    uint8_t prevSucc = c.deathSuccesses;
    uint8_t prevFail = c.deathFailures;
    bool prevStab = c.stabilized;

    if (delta < 0) {
        int dmg = -delta;
        if (c.tempHp > 0) {
            int absorbed = std::min(c.tempHp, dmg);
            c.tempHp -= absorbed;
            dmg -= absorbed;
        }
        if (dmg > 0) {
            c.currentHp = std::clamp(c.currentHp - dmg, 0, c.maxHp);
        }
    } else if (delta > 0) {
        c.currentHp = std::clamp(c.currentHp + delta, 0, c.maxHp);
    }

    if (c.currentHp != prevHp || c.tempHp != prevTemp) {
        lastHpEditId_     = c.instanceId;
        lastHpPrevValue_  = prevHp;
        lastHpPrevTempHp_ = prevTemp;
        lastHpPrevConc_   = prevConc;
        lastHpPrevDsSucc_ = prevSucc;
        lastHpPrevDsFail_ = prevFail;
        lastHpPrevStab_   = prevStab;
        lastHpDelta_      = (c.currentHp - prevHp) + (c.tempHp - prevTemp);
        if (c.currentHp <= 0) {
            c.concentrating = false;
        } else if (prevHp <= 0 && c.isPlayer) {
            c.deathSuccesses = 0;
            c.deathFailures  = 0;
            c.stabilized     = false;
        }
    }
}

void CombatTracker::setHp(int index, int hp) {
    if (index < 0 || index >= static_cast<int>(combatants_.size())) return;
    Combatant& c = combatants_[static_cast<size_t>(index)];
    int prevHp   = c.currentHp;
    int prevTemp = c.tempHp;
    bool prevConc = c.concentrating;
    uint8_t prevSucc = c.deathSuccesses;
    uint8_t prevFail = c.deathFailures;
    bool prevStab = c.stabilized;

    c.currentHp = std::clamp(hp, 0, c.maxHp);
    if (c.currentHp != prevHp) {
        lastHpEditId_     = c.instanceId;
        lastHpPrevValue_  = prevHp;
        lastHpPrevTempHp_ = prevTemp;
        lastHpPrevConc_   = prevConc;
        lastHpPrevDsSucc_ = prevSucc;
        lastHpPrevDsFail_ = prevFail;
        lastHpPrevStab_   = prevStab;
        lastHpDelta_      = c.currentHp - prevHp;
        if (c.currentHp <= 0) {
            c.concentrating = false;
        } else if (prevHp <= 0 && c.isPlayer) {
            c.deathSuccesses = 0;
            c.deathFailures  = 0;
            c.stabilized     = false;
        }
    }
}

void CombatTracker::setTempHp(int index, int amount) {
    if (index < 0 || index >= static_cast<int>(combatants_.size())) return;
    Combatant& c = combatants_[static_cast<size_t>(index)];
    int clamped = std::clamp(amount, 0, 999);
    int newTemp = (clamped == 0) ? 0 : std::max(c.tempHp, clamped);
    if (newTemp != c.tempHp) {
        lastHpEditId_     = c.instanceId;
        lastHpPrevValue_  = c.currentHp;
        lastHpPrevTempHp_ = c.tempHp;
        lastHpPrevConc_   = c.concentrating;
        lastHpPrevDsSucc_ = c.deathSuccesses;
        lastHpPrevDsFail_ = c.deathFailures;
        lastHpPrevStab_   = c.stabilized;
        lastHpDelta_      = newTemp - c.tempHp;
        c.tempHp          = newTemp;
    }
}

bool CombatTracker::undoLastHp() {
    if (lastHpEditId_ == 0) return false;
    for (size_t i = 0; i < combatants_.size(); ++i) {
        if (combatants_[i].instanceId == lastHpEditId_) {
            Combatant& c = combatants_[i];
            c.currentHp      = std::clamp(lastHpPrevValue_, 0, c.maxHp);
            c.tempHp         = std::max(0, lastHpPrevTempHp_);
            c.concentrating  = lastHpPrevConc_;
            c.deathSuccesses = lastHpPrevDsSucc_;
            c.deathFailures  = lastHpPrevDsFail_;
            c.stabilized     = lastHpPrevStab_;
            selectedIndex_   = static_cast<int>(i);
            lastHpEditId_    = 0;
            lastHpDelta_     = 0;
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

void CombatTracker::toggleConcentration(int index) {
    if (index < 0 || index >= static_cast<int>(combatants_.size())) return;
    Combatant& c = combatants_[static_cast<size_t>(index)];
    c.concentrating = !c.concentrating;
}

void CombatTracker::recordDeathSave(int index, bool success) {
    if (index < 0 || index >= static_cast<int>(combatants_.size())) return;
    Combatant& c = combatants_[static_cast<size_t>(index)];
    if (!c.isPlayer || c.currentHp > 0) return;
    if (success) {
        if (c.deathSuccesses < 3) ++c.deathSuccesses;
        if (c.deathSuccesses >= 3) c.stabilized = true;
    } else {
        if (c.deathFailures < 3) ++c.deathFailures;
        c.stabilized = false;
    }
}

void CombatTracker::stabilize(int index) {
    if (index < 0 || index >= static_cast<int>(combatants_.size())) return;
    Combatant& c = combatants_[static_cast<size_t>(index)];
    if (!c.isPlayer || c.currentHp > 0) return;
    c.deathSuccesses = 3;
    c.deathFailures  = 0;
    c.stabilized     = true;
}

void CombatTracker::useLegendaryAction(int index) {
    if (index < 0 || index >= static_cast<int>(combatants_.size())) return;
    Combatant& c = combatants_[static_cast<size_t>(index)];
    if (c.legendMax == 0) return;
    if (c.legendActions > 0) --c.legendActions;
    else c.legendActions = c.legendMax;
}

void CombatTracker::resetLegendaryActions(int index) {
    if (index < 0 || index >= static_cast<int>(combatants_.size())) return;
    Combatant& c = combatants_[static_cast<size_t>(index)];
    c.legendActions = c.legendMax;
}

int CombatTracker::rollRecharge(int index, uint32_t& rngState) {
    if (index < 0 || index >= static_cast<int>(combatants_.size())) return 0;
    Combatant& c = combatants_[static_cast<size_t>(index)];
    if (c.rechargeMin == 0) return 0;
    if (rngState == 0) rngState = 0x5EED1234u;
    uint32_t x = rngState;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    rngState = x;
    int d6 = static_cast<int>(x % 6u) + 1;
    if (d6 >= c.rechargeMin) {
        c.rechargeReady = true;
    }
    return d6;
}

void CombatTracker::toggleRechargeReady(int index) {
    if (index < 0 || index >= static_cast<int>(combatants_.size())) return;
    Combatant& c = combatants_[static_cast<size_t>(index)];
    if (c.rechargeMin == 0) return;
    c.rechargeReady = !c.rechargeReady;
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

uint32_t CombatTracker::computeDefeatedXp(const SrdDatabase& db) const {
    uint32_t total = 0;
    for (const auto& c : combatants_) {
        if (!c.isPlayer && c.monsterId < 0xFFFE && c.currentHp <= 0) {
            MonsterView mv{};
            if (db.getMonster(c.monsterId, mv)) {
                total += mv.xp;
            }
        }
    }
    return total;
}

void CombatTracker::remapDeletedHomebrewMonster(uint16_t deletedId) {
    for (auto& c : combatants_) {
        if (!c.isPlayer && c.monsterId >= HOMEBREW_MONSTER_ID_BASE && c.monsterId < 0xFFFE) {
            if (c.monsterId == deletedId) {
                c.monsterId = 0xFFFF;
            } else if (c.monsterId > deletedId) {
                --c.monsterId;
            }
        }
    }
}

void CombatTracker::nextTurn() {
    if (combatants_.empty()) return;
    const int n = static_cast<int>(combatants_.size());
    for (int step = 0; step < n; ++step) {
        ++activeTurnIndex_;
        if (activeTurnIndex_ >= n) {
            activeTurnIndex_ = 0;
            ++roundNumber_;
        }
        Combatant& cur = combatants_[static_cast<size_t>(activeTurnIndex_)];
        if (!autoSkipDead_ || cur.isPlayer || cur.currentHp > 0) {
            if (cur.legendMax > 0) {
                cur.legendActions = cur.legendMax;
            }
            break;
        }
    }
    selectedIndex_ = activeTurnIndex_;
}

void CombatTracker::prevTurn() {
    if (combatants_.empty()) return;
    const int n = static_cast<int>(combatants_.size());
    for (int step = 0; step < n; ++step) {
        --activeTurnIndex_;
        if (activeTurnIndex_ < 0) {
            activeTurnIndex_ = n - 1;
            if (roundNumber_ > 1) --roundNumber_;
        }
        const Combatant& cur = combatants_[static_cast<size_t>(activeTurnIndex_)];
        if (!autoSkipDead_ || cur.isPlayer || cur.currentHp > 0) {
            break;
        }
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
