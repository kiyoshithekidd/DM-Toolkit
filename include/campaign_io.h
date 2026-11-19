#pragma once

#include "combat_tracker.h"
#include <string>
#include <vector>

namespace dmtk {

struct CampaignData {
    std::string                 name;
    std::string                 slug;
    uint8_t                     partyLevel = 1;
    std::vector<PartyMember>    party;
    std::vector<SavedEncounter> encounters;
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
    std::string homebrewDir() const { return baseDir_ + "/homebrew"; }

    // Creates starter_campaign.json if no campaigns exist yet
    bool ensureDefaultCampaign(const SrdDatabase& db);

    std::vector<CampaignSummary> listCampaigns() const;
    bool loadCampaign(const std::string& slug, CampaignData& outCampaign, CombatTracker* outFight = nullptr) const;
    bool saveCampaign(const CampaignData& campaign, const CombatTracker* activeFight = nullptr) const;
    bool createCampaign(const std::string& name, CampaignData& outCampaign) const;

private:
    std::string baseDir_;
    std::string campaignsDir_;

    static void mkdirRecursive(const std::string& path);
    static std::string escapeJson(const std::string& s);
};

} // namespace dmtk
