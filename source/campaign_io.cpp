#include "campaign_io.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <sys/stat.h>

namespace dmtk {

CampaignManager::CampaignManager(const std::string& baseDir)
    : baseDir_(baseDir), campaignsDir_(baseDir + "/campaigns") {}

void CampaignManager::mkdirRecursive(const std::string& path) {
    std::string cur;
    for (size_t i = 0; i < path.size(); ++i) {
        cur.push_back(path[i]);
        if (path[i] == '/' && cur.size() > 1 && cur.back() == '/') {
            if (cur.find(":/") != cur.size() - 2) {
                mkdir(cur.c_str(), 0777);
            }
        }
    }
    if (!cur.empty()) {
        mkdir(cur.c_str(), 0777);
    }
}

bool CampaignManager::ensureDirectories() const {
    mkdirRecursive(baseDir_);
    mkdirRecursive(campaignsDir_);
    mkdirRecursive(homebrewDir());
    return true;
}

std::string CampaignManager::makeSlug(const std::string& name) const {
    std::string out;
    bool lastUnderscore = false;
    for (unsigned char c : name) {
        if (std::isalnum(c)) {
            out.push_back(static_cast<char>(std::tolower(c)));
            lastUnderscore = false;
        } else if (!lastUnderscore && !out.empty()) {
            out.push_back('_');
            lastUnderscore = true;
        }
        if (out.size() >= 28) break;
    }
    while (!out.empty() && out.back() == '_') out.pop_back();
    if (out.empty()) out = "campaign";
    return out;
}

std::string CampaignManager::campaignPathForSlug(const std::string& slug) const {
    return campaignsDir_ + "/" + slug + ".json";
}

std::string CampaignManager::escapeJson(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') continue;
        else out.push_back(c);
    }
    return out;
}

static std::string jStr(const std::string& obj, const char* key, const char* defVal = "") {
    std::string pat = std::string("\"") + key + "\"";
    size_t pos = obj.find(pat);
    if (pos == std::string::npos) return defVal;
    pos = obj.find(':', pos + pat.size());
    if (pos == std::string::npos) return defVal;
    pos = obj.find('"', pos + 1);
    if (pos == std::string::npos) return defVal;
    std::string out;
    for (size_t i = pos + 1; i < obj.size(); ++i) {
        if (obj[i] == '\\' && i + 1 < obj.size()) {
            char next = obj[++i];
            if (next == 'n') out.push_back('\n');
            else out.push_back(next);
        } else if (obj[i] == '"') {
            break;
        } else {
            out.push_back(obj[i]);
        }
    }
    return out;
}

static int jInt(const std::string& obj, const char* key, int defVal = 0) {
    std::string pat = std::string("\"") + key + "\"";
    size_t pos = obj.find(pat);
    if (pos == std::string::npos) return defVal;
    pos = obj.find(':', pos + pat.size());
    if (pos == std::string::npos) return defVal;
    ++pos;
    while (pos < obj.size() && (obj[pos] == ' ' || obj[pos] == '\t')) ++pos;
    if (pos >= obj.size()) return defVal;
    return std::atoi(obj.c_str() + pos);
}

static bool jBool(const std::string& obj, const char* key, bool defVal = true) {
    std::string pat = std::string("\"") + key + "\"";
    size_t pos = obj.find(pat);
    if (pos == std::string::npos) return defVal;
    pos = obj.find(':', pos + pat.size());
    if (pos == std::string::npos) return defVal;
    ++pos;
    while (pos < obj.size() && (obj[pos] == ' ' || obj[pos] == '\t')) ++pos;
    if (obj.compare(pos, 4, "true") == 0) return true;
    if (obj.compare(pos, 5, "false") == 0) return false;
    return defVal;
}

static std::string extractBracketSlice(const std::string& text, const char* key, char openCh, char closeCh) {
    std::string pat = std::string("\"") + key + "\"";
    size_t pos = text.find(pat);
    if (pos == std::string::npos) return "";
    pos = text.find(openCh, pos + pat.size());
    if (pos == std::string::npos) return "";
    int depth = 0;
    bool inStr = false;
    size_t start = pos;
    for (; pos < text.size(); ++pos) {
        char c = text[pos];
        if (c == '\\' && inStr) { ++pos; continue; }
        if (c == '"') inStr = !inStr;
        if (!inStr) {
            if (c == openCh) ++depth;
            else if (c == closeCh) {
                --depth;
                if (depth == 0) return text.substr(start, pos - start + 1);
            }
        }
    }
    return "";
}

static std::vector<std::string> splitTopLevelObjects(const std::string& arrayStr) {
    std::vector<std::string> objs;
    if (arrayStr.size() < 2) return objs;
    size_t pos = 1;
    while (pos < arrayStr.size()) {
        while (pos < arrayStr.size() && arrayStr[pos] != '{' && arrayStr[pos] != ']') ++pos;
        if (pos >= arrayStr.size() || arrayStr[pos] == ']') break;
        int depth = 0;
        bool inStr = false;
        size_t start = pos;
        for (; pos < arrayStr.size(); ++pos) {
            char c = arrayStr[pos];
            if (c == '\\' && inStr) { ++pos; continue; }
            if (c == '"') inStr = !inStr;
            if (!inStr) {
                if (c == '{') ++depth;
                else if (c == '}') {
                    --depth;
                    if (depth == 0) {
                        objs.push_back(arrayStr.substr(start, pos - start + 1));
                        ++pos;
                        break;
                    }
                }
            }
        }
    }
    return objs;
}

bool CampaignManager::saveCampaign(const CampaignData& campaign, const CombatTracker* activeFight) const {
    std::string finalPath = campaignPathForSlug(campaign.slug);
    std::string tmpPath   = finalPath + ".tmp";

    FILE* fp = std::fopen(tmpPath.c_str(), "wb");
    if (!fp) return false;

    std::fprintf(fp, "{\n");
    std::fprintf(fp, "  \"campaign_name\": \"%s\",\n", escapeJson(campaign.name).c_str());
    std::fprintf(fp, "  \"slug\": \"%s\",\n", escapeJson(campaign.slug).c_str());
    std::fprintf(fp, "  \"party_level\": %u,\n", static_cast<unsigned>(campaign.partyLevel));

    std::fprintf(fp, "  \"party\": [\n");
    for (size_t i = 0; i < campaign.party.size(); ++i) {
        const auto& p = campaign.party[i];
        std::fprintf(fp, "    {\"name\": \"%s\", \"init_bonus\": %d, \"level\": %u, \"ac\": %u, \"active\": %s}%s\n",
                     escapeJson(p.name).c_str(), static_cast<int>(p.initBonus),
                     static_cast<unsigned>(p.level), static_cast<unsigned>(p.ac),
                     p.active ? "true" : "false",
                     (i + 1 < campaign.party.size()) ? "," : "");
    }
    std::fprintf(fp, "  ],\n");

    std::fprintf(fp, "  \"encounters\": [\n");
    for (size_t i = 0; i < campaign.encounters.size(); ++i) {
        const auto& enc = campaign.encounters[i];
        std::fprintf(fp, "    {\"name\": \"%s\", \"slots\": [", escapeJson(enc.name).c_str());
        for (size_t j = 0; j < enc.slots.size(); ++j) {
            std::fprintf(fp, "{\"monster_id\": %u, \"count\": %u}%s",
                         static_cast<unsigned>(enc.slots[j].monsterId),
                         static_cast<unsigned>(enc.slots[j].count),
                         (j + 1 < enc.slots.size()) ? ", " : "");
        }
        std::fprintf(fp, "]}%s\n", (i + 1 < campaign.encounters.size()) ? "," : "");
    }
    std::fprintf(fp, "  ]");

    if (activeFight && activeFight->isInCombat()) {
        std::fprintf(fp, ",\n  \"active_fight\": {\n");
        std::fprintf(fp, "    \"round\": %d,\n", activeFight->roundNumber());
        std::fprintf(fp, "    \"active_turn\": %d,\n", activeFight->activeTurnIndex());
        std::fprintf(fp, "    \"combatants\": [\n");
        const auto& list = activeFight->combatants();
        for (size_t i = 0; i < list.size(); ++i) {
            const auto& c = list[i];
            std::fprintf(fp,
                         "      {\"name\": \"%s\", \"is_player\": %s, \"monster_id\": %u, "
                         "\"init_roll\": %d, \"init_bonus\": %d, \"hp\": %d, \"max_hp\": %d, "
                         "\"ac\": %u, \"cond\": %u}%s\n",
                         escapeJson(c.displayName).c_str(),
                         c.isPlayer ? "true" : "false",
                         static_cast<unsigned>(c.monsterId),
                         c.initRoll,
                         static_cast<int>(c.initBonus),
                         c.currentHp,
                         c.maxHp,
                         static_cast<unsigned>(c.armorClass),
                         static_cast<unsigned>(c.conditionsMask),
                         (i + 1 < list.size()) ? "," : "");
        }
        std::fprintf(fp, "    ]\n  }\n");
    } else {
        std::fprintf(fp, "\n");
    }

    std::fprintf(fp, "}\n");
    std::fclose(fp);

    std::remove(finalPath.c_str());
    return std::rename(tmpPath.c_str(), finalPath.c_str()) == 0;
}

bool CampaignManager::loadCampaign(const std::string& slug, CampaignData& outCampaign,
                                   CombatTracker* outFight) const {
    std::string path = campaignPathForSlug(slug);
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) return false;

    std::fseek(fp, 0, SEEK_END);
    long sz = std::ftell(fp);
    if (sz <= 0 || sz > 256 * 1024) {
        std::fclose(fp);
        return false;
    }
    std::fseek(fp, 0, SEEK_SET);
    std::string content(static_cast<size_t>(sz), '\0');
    if (std::fread(&content[0], 1, content.size(), fp) != content.size()) {
        std::fclose(fp);
        return false;
    }
    std::fclose(fp);

    outCampaign = CampaignData{};
    outCampaign.name       = jStr(content, "campaign_name", slug.c_str());
    outCampaign.slug       = jStr(content, "slug", slug.c_str());
    outCampaign.partyLevel = static_cast<uint8_t>(std::clamp(jInt(content, "party_level", 1), 1, 20));

    std::string partyArr = extractBracketSlice(content, "party", '[', ']');
    for (const auto& obj : splitTopLevelObjects(partyArr)) {
        PartyMember p;
        p.name      = jStr(obj, "name", "Hero");
        p.initBonus = static_cast<int8_t>(std::clamp(jInt(obj, "init_bonus", 0), -10, 20));
        p.level     = static_cast<uint8_t>(std::clamp(jInt(obj, "level", outCampaign.partyLevel), 1, 20));
        p.ac        = static_cast<uint8_t>(std::clamp(jInt(obj, "ac", 12), 1, 30));
        p.active    = jBool(obj, "active", true);
        outCampaign.party.push_back(p);
    }

    std::string encArr = extractBracketSlice(content, "encounters", '[', ']');
    for (const auto& obj : splitTopLevelObjects(encArr)) {
        SavedEncounter enc;
        enc.name = jStr(obj, "name", "Encounter");
        std::string slotArr = extractBracketSlice(obj, "slots", '[', ']');
        for (const auto& sobj : splitTopLevelObjects(slotArr)) {
            EncounterSlot slot;
            slot.monsterId = static_cast<uint16_t>(jInt(sobj, "monster_id", 0));
            slot.count     = static_cast<uint8_t>(std::clamp(jInt(sobj, "count", 1), 1, 20));
            enc.slots.push_back(slot);
        }
        outCampaign.encounters.push_back(enc);
    }

    if (outFight) {
        outFight->clear();
        std::string fightObj = extractBracketSlice(content, "active_fight", '{', '}');
        if (!fightObj.empty()) {
            int roundNum   = jInt(fightObj, "round", 1);
            int activeTurn = jInt(fightObj, "active_turn", 0);
            std::string combArr = extractBracketSlice(fightObj, "combatants", '[', ']');
            std::vector<Combatant> list;
            for (const auto& cobj : splitTopLevelObjects(combArr)) {
                Combatant c;
                c.displayName    = jStr(cobj, "name", "Combatant");
                c.isPlayer       = jBool(cobj, "is_player", false);
                c.monsterId      = static_cast<uint16_t>(jInt(cobj, "monster_id", 0xFFFF));
                c.initRoll       = jInt(cobj, "init_roll", 10);
                c.initBonus      = static_cast<int8_t>(jInt(cobj, "init_bonus", 0));
                c.currentHp      = jInt(cobj, "hp", 0);
                c.maxHp          = jInt(cobj, "max_hp", 0);
                c.armorClass     = static_cast<uint16_t>(jInt(cobj, "ac", 10));
                c.conditionsMask = static_cast<uint16_t>(jInt(cobj, "cond", 0));
                list.push_back(c);
            }
            if (!list.empty()) {
                outFight->restoreState(roundNum, activeTurn, std::move(list));
            }
        }
    }

    return true;
}

std::vector<CampaignSummary> CampaignManager::listCampaigns() const {
    std::vector<CampaignSummary> result;
    DIR* dir = opendir(campaignsDir_.c_str());
    if (!dir) return result;

    struct dirent* ent = nullptr;
    while ((ent = readdir(dir)) != nullptr) {
        std::string fname = ent->d_name;
        if (fname.size() > 5 && fname.substr(fname.size() - 5) == ".json") {
            std::string slug = fname.substr(0, fname.size() - 5);
            CampaignData tmp;
            if (loadCampaign(slug, tmp, nullptr)) {
                CampaignSummary sum;
                sum.name     = tmp.name;
                sum.slug     = slug;
                sum.filePath = campaignPathForSlug(slug);
                result.push_back(sum);
            }
        }
    }
    closedir(dir);

    std::sort(result.begin(), result.end(),
              [](const CampaignSummary& a, const CampaignSummary& b) {
                  return a.name < b.name;
              });
    return result;
}

bool CampaignManager::createCampaign(const std::string& name, CampaignData& outCampaign) const {
    ensureDirectories();
    outCampaign = CampaignData{};
    outCampaign.name       = name.empty() ? "New Campaign" : name;
    outCampaign.slug       = makeSlug(outCampaign.name);
    outCampaign.partyLevel = 1;
    outCampaign.party = {
        {"Hero 1", 2, 1, 15, true},
        {"Hero 2", 3, 1, 14, true},
        {"Hero 3", 1, 1, 16, true},
        {"Hero 4", 0, 1, 12, true},
    };
    return saveCampaign(outCampaign, nullptr);
}

bool CampaignManager::ensureDefaultCampaign(const SrdDatabase& db) {
    ensureDirectories();
    auto existing = listCampaigns();
    if (!existing.empty()) return true;

    CampaignData starter;
    starter.name       = "Starter Campaign";
    starter.slug       = "starter_campaign";
    starter.partyLevel = 1;
    starter.party = {
        {"Bram",   1, 1, 16, true},
        {"Lirael", 3, 1, 14, true},
        {"Tovin",  2, 1, 15, true},
        {"Mira",   0, 1, 12, true},
    };

    // Look up Goblin, Wolf, and Skeleton IDs from SRD so starter encounters are valid
    auto findMonsterId = [&](const char* q) -> uint16_t {
        std::vector<SearchHit> hits;
        db.searchMonsters(q, -1, CR_FILTER_ALL, hits, 5);
        for (const auto& h : hits) {
            MonsterView mv{};
            if (db.getMonster(h.recordId, mv) && std::string(mv.name) == q) {
                return h.recordId;
            }
        }
        return hits.empty() ? 0 : hits[0].recordId;
    };

    SavedEncounter ambush;
    ambush.name  = "Roadside Ambush";
    ambush.slots = {{findMonsterId("Goblin"), 3}, {findMonsterId("Wolf"), 1}};

    SavedEncounter crypt;
    crypt.name  = "Old Crypt Watch";
    crypt.slots = {{findMonsterId("Skeleton"), 3}, {findMonsterId("Zombie"), 2}};

    starter.encounters.push_back(ambush);
    starter.encounters.push_back(crypt);

    return saveCampaign(starter, nullptr);
}

bool CampaignManager::saveSettings(const UserSettings& settings) const {
    ensureDirectories();
    std::string finalPath = settingsPath();
    std::string tmpPath   = finalPath + ".tmp";

    FILE* fp = std::fopen(tmpPath.c_str(), "wb");
    if (!fp) return false;

    std::fprintf(fp, "{\n");
    std::fprintf(fp, "  \"dice_skin\": %d,\n", settings.diceSkinIdx);
    std::fprintf(fp, "  \"tray_skin\": %d,\n", settings.traySkinIdx);
    std::fprintf(fp, "  \"last_campaign\": \"%s\"\n", escapeJson(settings.lastCampaignSlug).c_str());
    std::fprintf(fp, "}\n");
    std::fclose(fp);

    std::remove(finalPath.c_str());
    return std::rename(tmpPath.c_str(), finalPath.c_str()) == 0;
}

bool CampaignManager::loadSettings(UserSettings& outSettings) const {
    FILE* fp = std::fopen(settingsPath().c_str(), "rb");
    if (!fp) return false;

    std::fseek(fp, 0, SEEK_END);
    long sz = std::ftell(fp);
    if (sz <= 0 || sz > 16 * 1024) {
        std::fclose(fp);
        return false;
    }
    std::fseek(fp, 0, SEEK_SET);
    std::string content(static_cast<size_t>(sz), '\0');
    if (std::fread(&content[0], 1, content.size(), fp) != content.size()) {
        std::fclose(fp);
        return false;
    }
    std::fclose(fp);

    outSettings.diceSkinIdx      = std::clamp(jInt(content, "dice_skin", 0), 0, 16);
    outSettings.traySkinIdx      = std::clamp(jInt(content, "tray_skin", 0), 0, 16);
    outSettings.lastCampaignSlug = jStr(content, "last_campaign", "");
    return true;
}

} // namespace dmtk
