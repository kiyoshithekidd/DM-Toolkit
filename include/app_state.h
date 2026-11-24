#pragma once

#include "campaign_io.h"
#include "combat_tracker.h"
#include "dice_roller.h"
#include "generators.h"
#include "sound_fx.h"
#include "srd_db.h"
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace dmtk {

constexpr const char* APP_VERSION = "v1.6.0";

enum AppMode : uint8_t {
    MODE_FIGHT  = 0,
    MODE_MANUAL = 1,
    MODE_DICE   = 2,
    MODE_RULES  = 3,
    MODE_TOOLS  = 4
};

enum FightSubPanel : uint8_t {
    FIGHT_PANEL_MAIN        = 0,
    FIGHT_PANEL_CONDITIONS  = 1,
    FIGHT_PANEL_END_CONFIRM = 2,
    FIGHT_PANEL_CUSTOM_HP   = 3
};

struct AppState {
    bool          showTitleSplash = true;
    AppMode       mode       = MODE_FIGHT;
    FightSubPanel fightPanel = FIGHT_PANEL_MAIN;

    SrdDatabase                  db;
    CombatTracker                tracker;
    CampaignManager              campaignMgr;
    CampaignData                 campaign;
    std::vector<CampaignSummary> campaigns;
    DiceRoller                   dice;
    SoundFx                      sfx;
    bool                         wasRollingDice = false;
    uint32_t                     rngState = 0x5EEDBEEFu;

    // Dice & Tray skin customization + Audio toggle (persisted in settings.json)
    int               diceSkinIdx  = 0;
    int               traySkinIdx  = 0;
    bool              soundEnabled = true;
    DieShape          diceShape    = DIE_D20;
    int               diceCount    = 1;
    int               diceMod      = 0;
    RollAdvantageMode diceAdvMode  = ROLL_NORMAL;

    // Custom HP Numpad buffer & Half/x2 multiplier (0=x1, 1=Half, 2=x2)
    int customHpValue = 0;
    int customHpMult  = 0;

    // Encounter Builder draft
    SavedEncounter draftEncounter;
    int            draftSelectedIdx    = 0;
    int            editingEncounterIdx = -1; // >= 0 when editing an existing saved encounter
    bool           encBuilderView      = false;

    // On-Device Homebrew Creator draft (Issue #9, #33, #43, #44)
    bool        hbIsSpell    = false;
    int         hbEditIdx    = -1; // -1 = new draft, >= 0 = index in hbMonsters_/hbSpells_ (on-device)
    std::string hbName       = "Shadow Hound";
    int         hbAc         = 14;
    int         hbHp         = 28;
    int         hbCr         = 2;
    int         hbSpellLevel = 1;
    std::string hbActionName = "Shadow Bite";
    std::string hbDesc       = "+5 to hit, 1d8+3 necrotic damage.";

    // Pinned Session Favorites (#32)
    std::vector<uint32_t> pinnedEntries;
    bool                  manualPinnedOnly = false;
    bool                  rulePinnedOnly   = false;

    // Monster Manual search & filters
    std::string            monsterQuery;
    int                    monsterTypeFilter = -1;
    CrFilter               monsterCrFilter   = CR_FILTER_ALL;
    std::vector<SearchHit> monsterResults;
    int                    monsterListCursor = 0;
    int                    monsterListScroll = 0;
    int                    manualTopScroll   = 0;

    // Pocket SRD Reference search & filters
    std::string            ruleQuery;
    RefTabFilter           refTab = REF_TAB_ALL;
    std::vector<SearchHit> ruleResults;
    int                    ruleListCursor = 0;
    int                    ruleListScroll = 0;
    int                    ruleTopScroll  = 0;

    // Fight Tracker top-screen state
    int  trackerRightScroll    = 0;
    int  fightActionIdx        = 0;
    bool combatRulesDrawerOpen = false;
    int  drawerRuleIndex       = 0;

    // Tools & Campaign state (0=Enc, 1=Gen, 2=Camp, 3=Brew, 4=Party, 5=Saves)
    int                 toolsSubTab      = 0;
    int                 partySelectedIdx = 0;
    int                 fightEncPage     = 0;
    int                 campaignListPage = 0;
    int                 campJournalMode  = 0; // 0 = Notes, 1 = Saved NPCs
    int                 campNoteCursor   = 0;
    int                 campNpcCursor    = 0;
    bool                showLegalInfo    = false;

    TerrainType         genTerrain       = TERRAIN_FOREST;
    EncounterDifficulty genDifficulty    = DIFF_MEDIUM;
    int                 genViewMode      = 0; // 0=NPC/Loot, 1=Tavern, 2=Hook, 3=Shop
    SavedEncounter      lastGenEncounter;
    GeneratedNpc        lastGenNpc;
    GeneratedLoot       lastGenLoot;
    GeneratedTavern     lastGenTavern;
    GeneratedQuestHook  lastGenHook;
    GeneratedShop       lastGenShop;
    bool                hasGenEncounter  = false;
    bool                hasGenNpc        = false;
    bool                hasGenLoot       = false;
    bool                hasGenTavern     = false;
    bool                hasGenHook       = false;
    bool                hasGenShop       = false;

    std::string statusBanner;
    int         statusFrames = 0;

    void setStatus(const std::string& msg, int frames = 120) {
        statusBanner = msg;
        statusFrames = frames;
    }

    int activePartyCount() const {
        int n = 0;
        for (const auto& p : campaign.party) {
            if (p.active) ++n;
        }
        return std::max(1, n);
    }

    void performLongRest() {
        for (auto& p : campaign.party) {
            if (p.active) {
                p.currentHp = p.maxHp;
            }
        }
        for (auto& c : tracker.combatantsMut()) {
            if (c.isPlayer) {
                c.currentHp      = c.maxHp;
                c.tempHp         = 0;
                c.conditionsMask = 0;
                c.concentrating  = false;
                c.deathSuccesses = 0;
                c.deathFailures  = 0;
                c.stabilized     = false;
            }
        }
    }

    void performShortRest() {
        for (auto& p : campaign.party) {
            if (p.active && p.currentHp < p.maxHp) {
                int heal = std::max(4, p.maxHp / 2);
                p.currentHp = std::min(p.maxHp, p.currentHp + heal);
            }
        }
    }

    void remapDeletedHomebrewId(uint8_t cat, uint16_t deletedId) {
        // Update pinnedEntries
        pinnedEntries.erase(
            std::remove_if(pinnedEntries.begin(), pinnedEntries.end(),
                           [cat, deletedId](uint32_t k) {
                               return (k >> 16) == cat && static_cast<uint16_t>(k & 0xFFFFu) == deletedId;
                           }),
            pinnedEntries.end());
        for (uint32_t& k : pinnedEntries) {
            if ((k >> 16) == cat) {
                uint16_t id = static_cast<uint16_t>(k & 0xFFFFu);
                if (id > deletedId) {
                    k = pinKey(cat, static_cast<uint16_t>(id - 1));
                }
            }
        }
        if (cat == CAT_MONSTER) {
            auto remapSlots = [deletedId](std::vector<EncounterSlot>& slots) {
                slots.erase(
                    std::remove_if(slots.begin(), slots.end(),
                                   [deletedId](const EncounterSlot& s) { return s.monsterId == deletedId; }),
                    slots.end());
                for (auto& s : slots) {
                    if (s.monsterId > deletedId && s.monsterId >= HOMEBREW_MONSTER_ID_BASE) {
                        --s.monsterId;
                    }
                }
            };
            for (auto& enc : campaign.encounters) remapSlots(enc.slots);
            remapSlots(draftEncounter.slots);
            tracker.remapDeletedHomebrewMonster(deletedId);
        }
    }

    static uint32_t pinKey(uint8_t cat, uint16_t id) {
        return (static_cast<uint32_t>(cat) << 16) | static_cast<uint32_t>(id);
    }

    bool isPinned(uint8_t cat, uint16_t id) const {
        uint32_t k = pinKey(cat, id);
        return std::find(pinnedEntries.begin(), pinnedEntries.end(), k) != pinnedEntries.end();
    }

    void togglePin(uint8_t cat, uint16_t id) {
        uint32_t k = pinKey(cat, id);
        auto it = std::find(pinnedEntries.begin(), pinnedEntries.end(), k);
        if (it != pinnedEntries.end()) {
            pinnedEntries.erase(it);
        } else if (pinnedEntries.size() < 64) {
            pinnedEntries.push_back(k);
        }
    }

    void addMonsterToDraft(uint16_t monsterId) {
        for (auto& s : draftEncounter.slots) {
            if (s.monsterId == monsterId) {
                if (s.count < 20) ++s.count;
                return;
            }
        }
        if (draftEncounter.slots.size() < 12) {
            draftEncounter.slots.push_back({monsterId, 1});
            draftSelectedIdx = static_cast<int>(draftEncounter.slots.size()) - 1;
        }
    }

    void saveUserSettings() const {
        UserSettings s;
        s.diceSkinIdx      = diceSkinIdx;
        s.traySkinIdx      = traySkinIdx;
        s.soundEnabled     = soundEnabled;
        s.autoSkipDead     = tracker.autoSkipDead();
        s.lastCampaignSlug = campaign.slug;
        s.pinnedEntries    = pinnedEntries;
        campaignMgr.saveSettings(s);
    }

    void refreshMonsterSearch() {
        db.searchMonsters(monsterQuery.c_str(), monsterTypeFilter, monsterCrFilter, monsterResults, 334);
        if (manualPinnedOnly) {
            monsterResults.erase(
                std::remove_if(monsterResults.begin(), monsterResults.end(),
                               [this](const SearchHit& h) { return !isPinned(CAT_MONSTER, h.recordId); }),
                monsterResults.end());
        }
        monsterListCursor = 0;
        monsterListScroll = 0;
        manualTopScroll   = 0;
    }

    void refreshRuleSearch() {
        db.searchReference(ruleQuery.c_str(), refTab, ruleResults, 128);
        if (rulePinnedOnly) {
            ruleResults.erase(
                std::remove_if(ruleResults.begin(), ruleResults.end(),
                               [this](const SearchHit& h) { return !isPinned(h.category, h.recordId); }),
                ruleResults.end());
        }
        ruleListCursor = 0;
        ruleListScroll = 0;
        ruleTopScroll  = 0;
    }
};

} // namespace dmtk
