#pragma once

#include "combat_tracker.h"
#include <string>
#include <vector>

namespace dmtk {

struct UserSettings {
    int                   diceSkinIdx  = 0;
    int                   traySkinIdx  = 0;
    bool                  soundEnabled = true;
    bool                  autoSkipDead = true;
    std::string           lastCampaignSlug;
    std::vector<uint32_t> pinnedEntries;
};

struct CampaignNote {
    uint16_t    sessionNum = 1;
    std::string text;
};

struct SavedNpc {
    std::string name;
    std::string ancestry;
    std::string occupation;
    std::string quirk;
    std::string note;
};

struct CampaignData {
    std::string                 name;
    std::string                 slug;
    uint8_t                     partyLevel    = 1;
    uint16_t                    sessionNumber = 1;
    uint16_t                    dayNumber     = 1;
    uint8_t                     timeSlot      = 1; // 0=Dawn, 1=Morning, 2=Noon, 3=Dusk, 4=Night
    uint8_t                     weather       = 0; // 0=Clear, 1=Overcast, 2=Rain, 3=Fog, 4=Windy, 5=Storm
    std::vector<PartyMember>    party;
    std::vector<SavedEncounter> encounters;
    std::vector<CampaignNote>   notes;
    std::vector<SavedNpc>       savedNpcs;
};

struct CampaignSummary {
    std::string name;
    std::string slug;
    std::string filePath;
};

class CampaignManager {
public:
    explicit CampaignManager(const std::string& baseDir = "sdmc:/3ds/DM_Toolkit");

    bool ensureDirectories() const;
    std::string makeSlug(const std::string& name) const;
    std::string campaignPathForSlug(const std::string& slug) const;
    std::string settingsPath() const { return baseDir_ + "/settings.json"; }
    std::string homebrewDir() const { return baseDir_ + "/homebrew"; }
    std::string partyImportDir() const { return baseDir_ + "/party"; }

    bool ensureDefaultCampaign(const SrdDatabase& db);

    std::vector<CampaignSummary> listCampaigns() const;
    bool loadCampaign(const std::string& slug, CampaignData& outCampaign, CombatTracker* outFight = nullptr) const;
    bool saveCampaign(const CampaignData& campaign, const CombatTracker* activeFight = nullptr) const;
    bool createCampaign(const std::string& name, CampaignData& outCampaign) const;
    int  importPartyCharacters(CampaignData& campaign) const;

    bool loadSettings(UserSettings& outSettings) const;
    bool saveSettings(const UserSettings& settings) const;

private:
    std::string baseDir_;
    std::string campaignsDir_;

    static void mkdirRecursive(const std::string& path);
    static std::string escapeJson(const std::string& s);
};

} // namespace dmtk
