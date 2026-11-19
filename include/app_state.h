#pragma once

#include "campaign_io.h"
#include "combat_tracker.h"
#include "dice_roller.h"
#include "generators.h"
#include "srd_db.h"
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace dmtk {

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
    uint32_t                     rngState = 0x5EEDBEEFu;

    // Dice & Tray skin customization (persisted in settings.json)
    int               diceSkinIdx = 0;
    int               traySkinIdx = 0;
    DieShape          diceShape   = DIE_D20;
    int               diceMod     = 0;
    RollAdvantageMode diceAdvMode = ROLL_NORMAL;

    // Custom HP Numpad buffer (Issue #5)
    int customHpValue = 0;

    // Encounter Builder draft (Issue #3)
    SavedEncounter draftEncounter;
    int            draftSelectedIdx = 0;
    bool           encBuilderView   = false; // Toggle between Random Gen and Prep Builder in Tools[0]

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
    bool combatRulesDrawerOpen = false;
    int  drawerRuleIndex       = 0;

    // Tools & Campaign state (0 = Enc Gen/Prep, 1 = NPC/Loot, 2 = Party, 3 = Saves, 4 = Legal)
    int                 toolsSubTab      = 0;
    int                 partySelectedIdx = 0;
    TerrainType         genTerrain       = TERRAIN_FOREST;
    EncounterDifficulty genDifficulty    = DIFF_MEDIUM;
    SavedEncounter      lastGenEncounter;
    GeneratedNpc        lastGenNpc;
    GeneratedLoot       lastGenLoot;
    bool                hasGenEncounter  = false;
    bool                hasGenNpc        = false;
    bool                hasGenLoot       = false;

    std::string statusBanner;
    int         statusFrames = 0;

    void setStatus(const std::string& msg, int frames = 120) {
        statusBanner = msg;
        statusFrames = frames;
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
        s.lastCampaignSlug = campaign.slug;
        campaignMgr.saveSettings(s);
    }

    void refreshMonsterSearch() {
        db.searchMonsters(monsterQuery.c_str(), monsterTypeFilter, monsterCrFilter, monsterResults, 334);
        monsterListCursor = 0;
        monsterListScroll = 0;
        manualTopScroll   = 0;
    }

    void refreshRuleSearch() {
        db.searchReference(ruleQuery.c_str(), refTab, ruleResults, 128);
        ruleListCursor = 0;
        ruleListScroll = 0;
        ruleTopScroll  = 0;
    }
};

} // namespace dmtk
