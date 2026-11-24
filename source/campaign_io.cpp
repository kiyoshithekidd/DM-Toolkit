#include "campaign_io.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
    mkdirRecursive(partyImportDir());
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

static size_t findTopLevelValuePos(const std::string& obj, const char* key) {
    int depth = 0;
    bool inStr = false;
    size_t keyLen = std::strlen(key);
    for (size_t i = 0; i < obj.size(); ++i) {
        char c = obj[i];
        if (inStr) {
            if (c == '\\' && i + 1 < obj.size()) {
                ++i;
            } else if (c == '"') {
                inStr = false;
            }
            continue;
        }
        if (c == '"') {
            if (depth == 1 && i + 1 + keyLen < obj.size() &&
                obj.compare(i + 1, keyLen, key) == 0 && obj[i + 1 + keyLen] == '"') {
                size_t p = i + 2 + keyLen;
                while (p < obj.size() && (obj[p] == ' ' || obj[p] == '\t' || obj[p] == '\r' || obj[p] == '\n')) ++p;
                if (p < obj.size() && obj[p] == ':') {
                    ++p;
                    while (p < obj.size() && (obj[p] == ' ' || obj[p] == '\t' || obj[p] == '\r' || obj[p] == '\n')) ++p;
                    return p;
                }
            }
            inStr = true;
        } else if (c == '{' || c == '[') {
            ++depth;
        } else if (c == '}' || c == ']') {
            --depth;
        }
    }
    return std::string::npos;
}

static std::string jStr(const std::string& obj, const char* key, const char* defVal = "") {
    size_t pos = findTopLevelValuePos(obj, key);
    if (pos == std::string::npos || pos >= obj.size() || obj[pos] != '"') return defVal;
    std::string out;
    for (size_t i = pos + 1; i < obj.size(); ++i) {
        if (obj[i] == '\\' && i + 1 < obj.size()) {
            char next = obj[++i];
            if (next == 'n') out.push_back('\n');
            else if (next == 'r') out.push_back('\r');
            else if (next == 't') out.push_back('\t');
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
    size_t pos = findTopLevelValuePos(obj, key);
    if (pos == std::string::npos || pos >= obj.size()) return defVal;
    return std::atoi(obj.c_str() + pos);
}

static bool jBool(const std::string& obj, const char* key, bool defVal = true) {
    size_t pos = findTopLevelValuePos(obj, key);
    if (pos == std::string::npos || pos >= obj.size()) return defVal;
    if (obj.compare(pos, 4, "true") == 0) return true;
    if (obj.compare(pos, 5, "false") == 0) return false;
    return defVal;
}

static std::string extractBracketSlice(const std::string& text, const char* key, char openCh, char closeCh) {
    size_t pos = findTopLevelValuePos(text, key);
    if (pos == std::string::npos || pos >= text.size() || text[pos] != openCh) return "";
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
    ensureDirectories();
    std::string finalPath = campaignPathForSlug(campaign.slug);
    std::string tmpPath   = finalPath + ".tmp";

    FILE* fp = std::fopen(tmpPath.c_str(), "wb");
    if (!fp) return false;

    std::fprintf(fp, "{\n");
    std::fprintf(fp, "  \"campaign_name\": \"%s\",\n", escapeJson(campaign.name).c_str());
    std::fprintf(fp, "  \"slug\": \"%s\",\n", escapeJson(campaign.slug).c_str());
    std::fprintf(fp, "  \"party_level\": %u,\n", static_cast<unsigned>(campaign.partyLevel));
    std::fprintf(fp, "  \"session_num\": %u,\n", static_cast<unsigned>(campaign.sessionNumber));
    std::fprintf(fp, "  \"day_num\": %u,\n", static_cast<unsigned>(campaign.dayNumber));
    std::fprintf(fp, "  \"time_slot\": %u,\n", static_cast<unsigned>(campaign.timeSlot));
    std::fprintf(fp, "  \"weather\": %u,\n", static_cast<unsigned>(campaign.weather));

    std::fprintf(fp, "  \"party\": [\n");
    for (size_t i = 0; i < campaign.party.size(); ++i) {
        const auto& p = campaign.party[i];
        std::fprintf(fp,
                     "    {\"name\": \"%s\", \"class_name\": \"%s\", \"init_bonus\": %d, "
                     "\"level\": %u, \"ac\": %u, \"max_hp\": %d, \"current_hp\": %d, "
                     "\"passive_perc\": %u, \"active\": %s}%s\n",
                     escapeJson(p.name).c_str(), escapeJson(p.className).c_str(),
                     static_cast<int>(p.initBonus), static_cast<unsigned>(p.level),
                     static_cast<unsigned>(p.ac), p.maxHp, p.currentHp,
                     static_cast<unsigned>(p.passivePerception),
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
    std::fprintf(fp, "  ],\n");

    std::fprintf(fp, "  \"notes\": [\n");
    for (size_t i = 0; i < campaign.notes.size(); ++i) {
        const auto& n = campaign.notes[i];
        std::fprintf(fp, "    {\"session\": %u, \"text\": \"%s\"}%s\n",
                     static_cast<unsigned>(n.sessionNum),
                     escapeJson(n.text).c_str(),
                     (i + 1 < campaign.notes.size()) ? "," : "");
    }
    std::fprintf(fp, "  ],\n");

    std::fprintf(fp, "  \"npcs\": [\n");
    for (size_t i = 0; i < campaign.savedNpcs.size(); ++i) {
        const auto& npc = campaign.savedNpcs[i];
        std::fprintf(fp,
                     "    {\"name\": \"%s\", \"ancestry\": \"%s\", \"occupation\": \"%s\", "
                     "\"quirk\": \"%s\", \"note\": \"%s\"}%s\n",
                     escapeJson(npc.name).c_str(),
                     escapeJson(npc.ancestry).c_str(),
                     escapeJson(npc.occupation).c_str(),
                     escapeJson(npc.quirk).c_str(),
                     escapeJson(npc.note).c_str(),
                     (i + 1 < campaign.savedNpcs.size()) ? "," : "");
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
                         "\"init_roll\": %d, \"init_bonus\": %d, \"hp\": %d, \"max_hp\": %d, \"temp_hp\": %d, "
                         "\"ac\": %u, \"cond\": %u, \"conc\": %s, \"ds_succ\": %u, \"ds_fail\": %u, \"stab\": %s, "
                         "\"leg_act\": %u, \"leg_max\": %u, \"rech_rdy\": %s, \"rech_min\": %u}%s\n",
                         escapeJson(c.displayName).c_str(),
                         c.isPlayer ? "true" : "false",
                         static_cast<unsigned>(c.monsterId),
                         c.initRoll,
                         static_cast<int>(c.initBonus),
                         c.currentHp,
                         c.maxHp,
                         c.tempHp,
                         static_cast<unsigned>(c.armorClass),
                         static_cast<unsigned>(c.conditionsMask),
                         c.concentrating ? "true" : "false",
                         static_cast<unsigned>(c.deathSuccesses),
                         static_cast<unsigned>(c.deathFailures),
                         c.stabilized ? "true" : "false",
                         static_cast<unsigned>(c.legendActions),
                         static_cast<unsigned>(c.legendMax),
                         c.rechargeReady ? "true" : "false",
                         static_cast<unsigned>(c.rechargeMin),
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
    outCampaign.name          = jStr(content, "campaign_name", slug.c_str());
    outCampaign.slug          = jStr(content, "slug", slug.c_str());
    outCampaign.partyLevel    = static_cast<uint8_t>(std::clamp(jInt(content, "party_level", 1), 1, 20));
    outCampaign.sessionNumber = static_cast<uint16_t>(std::clamp(jInt(content, "session_num", 1), 1, 999));
    outCampaign.dayNumber     = static_cast<uint16_t>(std::clamp(jInt(content, "day_num", 1), 1, 9999));
    outCampaign.timeSlot      = static_cast<uint8_t>(std::clamp(jInt(content, "time_slot", 1), 0, 4));
    outCampaign.weather       = static_cast<uint8_t>(std::clamp(jInt(content, "weather", 0), 0, 5));

    std::string partyArr = extractBracketSlice(content, "party", '[', ']');
    for (const auto& obj : splitTopLevelObjects(partyArr)) {
        PartyMember p;
        p.name              = jStr(obj, "name", "Hero");
        p.className         = jStr(obj, "class_name", "Adventurer");
        p.initBonus         = static_cast<int8_t>(std::clamp(jInt(obj, "init_bonus", 0), -10, 20));
        p.level             = static_cast<uint8_t>(std::clamp(jInt(obj, "level", outCampaign.partyLevel), 1, 20));
        p.ac                = static_cast<uint8_t>(std::clamp(jInt(obj, "ac", 12), 1, 30));
        int defHp           = 10 + static_cast<int>(p.level) * 6;
        p.maxHp             = std::max(1, jInt(obj, "max_hp", defHp));
        p.currentHp         = std::clamp(jInt(obj, "current_hp", p.maxHp), 0, p.maxHp);
        p.passivePerception = static_cast<uint8_t>(std::clamp(jInt(obj, "passive_perc", 12), 1, 35));
        p.active            = jBool(obj, "active", true);
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

    std::string notesArr = extractBracketSlice(content, "notes", '[', ']');
    for (const auto& nobj : splitTopLevelObjects(notesArr)) {
        CampaignNote n;
        n.sessionNum = static_cast<uint16_t>(std::clamp(jInt(nobj, "session", 1), 1, 999));
        n.text       = jStr(nobj, "text", "");
        if (!n.text.empty()) outCampaign.notes.push_back(std::move(n));
    }

    std::string npcsArr = extractBracketSlice(content, "npcs", '[', ']');
    for (const auto& npobj : splitTopLevelObjects(npcsArr)) {
        SavedNpc npc;
        npc.name       = jStr(npobj, "name", "");
        npc.ancestry   = jStr(npobj, "ancestry", "Human");
        npc.occupation = jStr(npobj, "occupation", "Traveler");
        npc.quirk      = jStr(npobj, "quirk", "");
        npc.note       = jStr(npobj, "note", "");
        if (!npc.name.empty()) outCampaign.savedNpcs.push_back(std::move(npc));
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
                c.tempHp         = std::max(0, jInt(cobj, "temp_hp", 0));
                c.armorClass     = static_cast<uint16_t>(jInt(cobj, "ac", 10));
                c.conditionsMask = static_cast<uint16_t>(jInt(cobj, "cond", 0));
                c.concentrating  = jBool(cobj, "conc", false);
                c.deathSuccesses = static_cast<uint8_t>(std::clamp(jInt(cobj, "ds_succ", 0), 0, 3));
                c.deathFailures  = static_cast<uint8_t>(std::clamp(jInt(cobj, "ds_fail", 0), 0, 3));
                c.stabilized     = jBool(cobj, "stab", false);
                c.legendActions  = static_cast<uint8_t>(std::clamp(jInt(cobj, "leg_act", 0), 0, 9));
                c.legendMax      = static_cast<uint8_t>(std::clamp(jInt(cobj, "leg_max", 0), 0, 9));
                c.rechargeReady  = jBool(cobj, "rech_rdy", true);
                c.rechargeMin    = static_cast<uint8_t>(std::clamp(jInt(cobj, "rech_min", 0), 0, 6));
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
    std::string baseSlug   = makeSlug(outCampaign.name);
    std::string uniqueSlug = baseSlug;
    int suffix = 2;
    while (true) {
        FILE* check = std::fopen(campaignPathForSlug(uniqueSlug).c_str(), "rb");
        if (!check) break;
        std::fclose(check);
        std::string sufStr = "_" + std::to_string(suffix++);
        size_t maxBase = (baseSlug.size() + sufStr.size() > 28) ? (28 - sufStr.size()) : baseSlug.size();
        uniqueSlug = baseSlug.substr(0, maxBase) + sufStr;
    }
    outCampaign.slug       = uniqueSlug;
    outCampaign.partyLevel = 1;
    outCampaign.party = {
        {"Hero 1", "Fighter", 2, 1, 15, 16, 16, true},
        {"Hero 2", "Rogue",   3, 1, 14, 14, 14, true},
        {"Hero 3", "Cleric",  1, 1, 16, 15, 15, true},
        {"Hero 4", "Wizard",  0, 1, 12, 12, 12, true},
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
        {"Bram",   "Fighter", 1, 1, 16, 16, 16, true},
        {"Lirael", "Ranger",  3, 1, 14, 14, 14, true},
        {"Tovin",  "Cleric",  2, 1, 15, 15, 15, true},
        {"Mira",   "Wizard",  0, 1, 12, 12, 12, true},
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
    std::fprintf(fp, "  \"sound_enabled\": %s,\n", settings.soundEnabled ? "true" : "false");
    std::fprintf(fp, "  \"auto_skip_dead\": %s,\n", settings.autoSkipDead ? "true" : "false");
    std::fprintf(fp, "  \"last_campaign\": \"%s\",\n", escapeJson(settings.lastCampaignSlug).c_str());
    std::fprintf(fp, "  \"pinned_entries\": [");
    for (size_t i = 0; i < settings.pinnedEntries.size(); ++i) {
        std::fprintf(fp, "%s%u", (i > 0 ? "," : ""), static_cast<unsigned>(settings.pinnedEntries[i]));
    }
    std::fprintf(fp, "]\n");
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
    outSettings.soundEnabled     = jBool(content, "sound_enabled", true);
    outSettings.autoSkipDead     = jBool(content, "auto_skip_dead", true);
    outSettings.lastCampaignSlug = jStr(content, "last_campaign", "");
    outSettings.pinnedEntries.clear();
    size_t pPos = content.find("\"pinned_entries\"");
    if (pPos != std::string::npos) {
        size_t lb = content.find('[', pPos);
        size_t rb = (lb != std::string::npos) ? content.find(']', lb) : std::string::npos;
        if (lb != std::string::npos && rb != std::string::npos) {
            uint32_t cur = 0;
            bool inNum = false;
            for (size_t i = lb + 1; i < rb; ++i) {
                char c = content[i];
                if (c >= '0' && c <= '9') {
                    cur = cur * 10u + static_cast<uint32_t>(c - '0');
                    inNum = true;
                } else if (inNum) {
                    outSettings.pinnedEntries.push_back(cur);
                    cur = 0;
                    inNum = false;
                }
            }
            if (inNum) outSettings.pinnedEntries.push_back(cur);
        }
    }
    return true;
}

int CampaignManager::importPartyCharacters(CampaignData& campaign) const {
    DIR* dir = opendir(partyImportDir().c_str());
    if (!dir) return 0;

    int imported = 0;
    struct dirent* ent = nullptr;
    while ((ent = readdir(dir)) != nullptr) {
        std::string fname = ent->d_name;
        if (fname.size() <= 5 || fname.substr(fname.size() - 5) != ".json") continue;

        std::string fullPath = partyImportDir() + "/" + fname;
        FILE* fp = std::fopen(fullPath.c_str(), "rb");
        if (!fp) continue;
        std::fseek(fp, 0, SEEK_END);
        long sz = std::ftell(fp);
        if (sz <= 0 || sz > 128 * 1024) {
            std::fclose(fp);
            continue;
        }
        std::fseek(fp, 0, SEEK_SET);
        std::string content(static_cast<size_t>(sz), '\0');
        if (std::fread(&content[0], 1, content.size(), fp) != content.size()) {
            std::fclose(fp);
            continue;
        }
        std::fclose(fp);

        std::vector<std::string> charObjs;
        std::string arr = extractBracketSlice(content, "characters", '[', ']');
        if (arr.empty()) arr = extractBracketSlice(content, "party", '[', ']');
        if (!arr.empty()) {
            charObjs = splitTopLevelObjects(arr);
        } else if (content.find('{') != std::string::npos) {
            charObjs.push_back(content);
        }

        for (const auto& obj : charObjs) {
            std::string name = jStr(obj, "name", "");
            if (name.empty()) continue;

            PartyMember pm;
            pm.name = name;
            pm.className = jStr(obj, "class", "");
            if (pm.className.empty()) pm.className = jStr(obj, "class_name", "Adventurer");
            pm.level     = static_cast<uint8_t>(std::clamp(jInt(obj, "level", campaign.partyLevel), 1, 20));
            pm.ac        = static_cast<uint8_t>(std::clamp(jInt(obj, "ac", 12), 1, 30));
            pm.initBonus = static_cast<int8_t>(std::clamp(jInt(obj, "init_bonus", 0), -10, 20));
            int defHp    = 10 + static_cast<int>(pm.level) * 6;
            pm.maxHp     = std::max(1, jInt(obj, "max_hp", defHp));
            pm.currentHp = std::clamp(jInt(obj, "current_hp", pm.maxHp), 0, pm.maxHp);
            int ppVal    = jInt(obj, "passive_perception", -1);
            if (ppVal < 0) ppVal = jInt(obj, "passive_perc", 12);
            pm.passivePerception = static_cast<uint8_t>(std::clamp(ppVal, 1, 35));
            pm.active    = jBool(obj, "active", true);

            bool updated = false;
            for (auto& existing : campaign.party) {
                if (existing.name == pm.name) {
                    existing = pm;
                    updated = true;
                    break;
                }
            }
            if (updated) {
                ++imported;
            } else if (campaign.party.size() < 8) {
                campaign.party.push_back(pm);
                ++imported;
            }
        }
    }
    closedir(dir);
    return imported;
}

} // namespace dmtk
