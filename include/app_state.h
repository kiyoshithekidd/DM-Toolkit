#pragma once

#include "campaign_io.h"
#include "combat_tracker.h"
#include "srd_db.h"
#include <cstdint>
#include <string>
#include <vector>

namespace dmtk {

enum AppMode : uint8_t {
    MODE_FIGHT    = 0,
    MODE_MANUAL   = 1,
    MODE_RULES    = 2,
    MODE_CAMPAIGN = 3
};

enum FightSubPanel : uint8_t {
    FIGHT_PANEL_MAIN       = 0,
    FIGHT_PANEL_CONDITIONS = 1,
    FIGHT_PANEL_END_CONFIRM = 2
};

struct AppState {
    AppMode       mode       = MODE_FIGHT;
    FightSubPanel fightPanel = FIGHT_PANEL_MAIN;

    SrdDatabase                  db;
    CombatTracker                tracker;
    CampaignManager              campaignMgr;
    CampaignData                 campaign;
    std::vector<CampaignSummary> campaigns;
    uint32_t                     rngState = 0x5EEDBEEFu;

    // Monster Manual search & filters
    std::string            monsterQuery;
    int                    monsterTypeFilter = -1; // -1 = All
    CrFilter               monsterCrFilter   = CR_FILTER_ALL;
    std::vector<SearchHit> monsterResults;
    int                    monsterListCursor = 0;
    int                    monsterListScroll = 0;
    int                    manualTopScroll   = 0;

    // Rules Reference search & filters
    std::string            ruleQuery;
    bool                   rulesConditionsOnly = false;
    std::vector<SearchHit> ruleResults;
    int                    ruleListCursor = 0;
    int                    ruleListScroll = 0;
    int                    ruleTopScroll  = 0;

    // Fight Tracker top-screen state
    int  trackerRightScroll     = 0;
    bool combatRulesDrawerOpen  = false;
    int  drawerRuleIndex        = 0; // Index into db rules (0..ruleCount-1)

    // Campaign screen state
    int  campaignSubTab         = 0; // 0 = Party & Encounters, 1 = Switch Campaign, 2 = Legal
    int  partySelectedIdx       = 0;

    // Brief status banner on bottom screen
    std::string statusBanner;
    int         statusFrames = 0;

    void setStatus(const std::string& msg, int frames = 120) {
        statusBanner = msg;
        statusFrames = frames;
    }

    void refreshMonsterSearch() {
        db.searchMonsters(monsterQuery.c_str(), monsterTypeFilter, monsterCrFilter, monsterResults, 334);
        monsterListCursor = 0;
        monsterListScroll = 0;
        manualTopScroll   = 0;
    }

    void refreshRuleSearch() {
        db.searchRules(ruleQuery.c_str(), rulesConditionsOnly, ruleResults, 64);
        ruleListCursor = 0;
        ruleListScroll = 0;
        ruleTopScroll  = 0;
    }
};

} // namespace dmtk
