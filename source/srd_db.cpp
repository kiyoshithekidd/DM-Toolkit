#include "srd_db.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <dirent.h>

namespace dmtk {

bool SrdDatabase::loadFromFile(const char* path) {
    loaded_ = false;
    raw_.clear();
    hbMonsters_.clear();
    hbSearchEntries_.clear();

    FILE* fp = std::fopen(path, "rb");
    if (!fp) return false;

    std::fseek(fp, 0, SEEK_END);
    long sz = std::ftell(fp);
    if (sz < static_cast<long>(sizeof(SrdBinaryHeader))) {
        std::fclose(fp);
        return false;
    }
    std::fseek(fp, 0, SEEK_SET);

    raw_.resize(static_cast<size_t>(sz));
    size_t got = std::fread(raw_.data(), 1, raw_.size(), fp);
    std::fclose(fp);
    if (got != raw_.size()) return false;

    std::memcpy(&header_, raw_.data(), sizeof(SrdBinaryHeader));
    if (std::memcmp(header_.magic, "DMTK", 4) != 0 || header_.version != SRD_BIN_VERSION) {
        return false;
    }

    // Validate section bounds
    if (header_.stringPoolOffset + header_.stringPoolSize > raw_.size()) return false;

    monsters_   = reinterpret_cast<const MonsterRecord*>(raw_.data() + header_.monstersOffset);
    actions_    = reinterpret_cast<const MonsterActionRef*>(raw_.data() + header_.actionsOffset);
    rules_      = reinterpret_cast<const RuleRecord*>(raw_.data() + header_.rulesOffset);
    searchIdx_  = reinterpret_cast<const SearchIndexEntry*>(raw_.data() + header_.searchIndexOffset);
    stringPool_ = reinterpret_cast<const char*>(raw_.data() + header_.stringPoolOffset);

    loaded_ = true;
    return true;
}

const char* SrdDatabase::str(uint32_t offset) const {
    if (!loaded_ || offset >= header_.stringPoolSize) return "";
    return stringPool_ + offset;
}

uint32_t SrdDatabase::monsterCount() const {
    return header_.monsterCount + static_cast<uint32_t>(hbMonsters_.size());
}

bool SrdDatabase::getMonster(uint16_t monsterId, MonsterView& out) const {
    if (!loaded_) return false;

    if (monsterId >= HOMEBREW_MONSTER_ID_BASE) {
        size_t idx = static_cast<size_t>(monsterId - HOMEBREW_MONSTER_ID_BASE);
        if (idx >= hbMonsters_.size()) return false;
        const HomebrewMonster& hb = hbMonsters_[idx];
        out.id           = hb.id;
        out.isHomebrew   = true;
        out.name         = hb.name.c_str();
        out.sizeAndAlign = hb.sizeAndAlign.c_str();
        out.type         = hb.type;
        out.crIndex      = hb.crIndex;
        out.xp           = hb.xp;
        out.armorClass   = hb.armorClass;
        out.acNote       = hb.acNote.c_str();
        out.hitPoints    = hb.hitPoints;
        out.hitDice      = hb.hitDice.c_str();
        out.speed        = hb.speed.c_str();
        out.strScore     = hb.strScore;
        out.dexScore     = hb.dexScore;
        out.conScore     = hb.conScore;
        out.intScore     = hb.intScore;
        out.wisScore     = hb.wisScore;
        out.chaScore     = hb.chaScore;
        out.initBonus    = hb.initBonus;
        out.saves        = hb.saves.c_str();
        out.skills       = hb.skills.c_str();
        out.defenses     = hb.defenses.c_str();
        out.sensesLang   = hb.sensesLang.c_str();
        out.actionCount  = static_cast<int>(hb.actions.size());
        return true;
    }

    if (monsterId >= header_.monsterCount) return false;
    const MonsterRecord& m = monsters_[monsterId];
    out.id           = m.id;
    out.isHomebrew   = false;
    out.name         = str(m.nameOffset);
    out.sizeAndAlign = str(m.sizeAndAlignOffset);
    out.type         = m.type;
    out.crIndex      = m.crIndex;
    out.xp           = m.xp;
    out.armorClass   = m.armorClass;
    out.acNote       = str(m.acNoteOffset);
    out.hitPoints    = m.hitPoints;
    out.hitDice      = str(m.hitDiceOffset);
    out.speed        = str(m.speedOffset);
    out.strScore     = m.strScore;
    out.dexScore     = m.dexScore;
    out.conScore     = m.conScore;
    out.intScore     = m.intScore;
    out.wisScore     = m.wisScore;
    out.chaScore     = m.chaScore;
    out.initBonus    = m.initBonus;
    out.saves        = str(m.savesOffset);
    out.skills       = str(m.skillsOffset);
    out.defenses     = str(m.defensesOffset);
    out.sensesLang   = str(m.sensesLangOffset);
    out.actionCount  = m.actionCount;
    return true;
}

bool SrdDatabase::getMonsterAction(uint16_t monsterId, int actionIdx, ActionView& out) const {
    if (!loaded_ || actionIdx < 0) return false;

    if (monsterId >= HOMEBREW_MONSTER_ID_BASE) {
        size_t idx = static_cast<size_t>(monsterId - HOMEBREW_MONSTER_ID_BASE);
        if (idx >= hbMonsters_.size()) return false;
        const HomebrewMonster& hb = hbMonsters_[idx];
        if (static_cast<size_t>(actionIdx) >= hb.actions.size()) return false;
        const HomebrewAction& a = hb.actions[static_cast<size_t>(actionIdx)];
        out.name        = a.name.c_str();
        out.summary     = a.summary.c_str();
        out.fullDesc    = a.fullDesc.c_str();
        out.attackBonus = a.attackBonus;
        out.sectionType = a.sectionType;
        return true;
    }

    if (monsterId >= header_.monsterCount) return false;
    const MonsterRecord& m = monsters_[monsterId];
    if (actionIdx >= m.actionCount) return false;
    uint32_t globalIdx = static_cast<uint32_t>(m.firstActionIdx) + static_cast<uint32_t>(actionIdx);
    if (globalIdx >= header_.actionCount) return false;
    const MonsterActionRef& a = actions_[globalIdx];
    out.name        = str(a.nameOffset);
    out.summary     = str(a.summaryOffset);
    out.fullDesc    = str(a.fullDescOffset);
    out.attackBonus = a.attackBonus;
    out.sectionType = a.sectionType;
    return true;
}

bool SrdDatabase::getRule(uint16_t ruleId, RuleView& out) const {
    if (!loaded_ || ruleId >= header_.ruleCount) return false;
    const RuleRecord& r = rules_[ruleId];
    out.id             = r.id;
    out.isCondition    = (r.isCondition != 0);
    out.conditionIndex = r.conditionIndex;
    out.name           = str(r.nameOffset);
    out.category       = str(r.categoryOffset);
    out.body           = str(r.bodyOffset);
    return true;
}

bool SrdDatabase::getConditionRule(int conditionIndex, RuleView& out) const {
    if (!loaded_ || conditionIndex < 0 || conditionIndex >= CONDITION_COUNT) return false;
    for (uint32_t i = 0; i < header_.ruleCount; ++i) {
        if (rules_[i].isCondition && rules_[i].conditionIndex == static_cast<uint8_t>(conditionIndex)) {
            return getRule(static_cast<uint16_t>(i), out);
        }
    }
    return false;
}

bool SrdDatabase::matchesCrFilter(uint8_t crIndex, CrFilter filter) {
    switch (filter) {
        case CR_FILTER_ALL:     return true;
        case CR_FILTER_0_1:     return crIndex <= 4;
        case CR_FILTER_2_4:     return crIndex >= 5 && crIndex <= 7;
        case CR_FILTER_5_10:    return crIndex >= 8 && crIndex <= 13;
        case CR_FILTER_11_PLUS: return crIndex >= 14;
        default:                return true;
    }
}

void SrdDatabase::normalizeQuery(const char* in, char* out, size_t outSz, uint32_t& outMask) {
    outMask = 0;
    size_t w = 0;
    bool prevSpace = true;
    if (in) {
        for (size_t i = 0; in[i] != '\0' && w + 1 < outSz; ++i) {
            unsigned char ch = static_cast<unsigned char>(in[i]);
            if (std::isalnum(ch)) {
                char lower = static_cast<char>(std::tolower(ch));
                out[w++] = lower;
                if (lower >= 'a' && lower <= 'z') {
                    outMask |= (1u << (lower - 'a'));
                }
                prevSpace = false;
            } else if (!prevSpace) {
                out[w++] = ' ';
                prevSpace = true;
            }
        }
    }
    while (w > 0 && out[w - 1] == ' ') --w;
    out[w] = '\0';
}

int SrdDatabase::scoreNameMatch(const char* normName, uint32_t nameMask,
                                const char* normQuery, size_t qLen, uint32_t qMask) {
    if (qLen == 0) return 100;
    if ((nameMask & qMask) != qMask) return 0;

    size_t nameLen = std::strlen(normName);
    if (qLen > nameLen) return 0;

    // Tier 1: Exact prefix match
    if (std::strncmp(normName, normQuery, qLen) == 0) {
        return 1000 - static_cast<int>(nameLen);
    }

    // Tier 2: Word-boundary prefix match (e.g. "dragon" -> "adult red dragon")
    for (size_t i = 1; i + qLen <= nameLen; ++i) {
        if (normName[i - 1] == ' ' && std::strncmp(normName + i, normQuery, qLen) == 0) {
            return 500 - static_cast<int>(i);
        }
    }

    // Tier 3: Substring match
    const char* found = std::strstr(normName, normQuery);
    if (found) {
        int pos = static_cast<int>(found - normName);
        return 200 - pos;
    }

    return 0;
}

void SrdDatabase::searchMonsters(const char* query, int typeFilter, CrFilter crFilter,
                                 std::vector<SearchHit>& outResults, int maxResults) const {
    outResults.clear();
    if (!loaded_) return;

    char normQ[32];
    uint32_t qMask = 0;
    normalizeQuery(query, normQ, sizeof(normQ), qMask);
    size_t qLen = std::strlen(normQ);

    auto scanArray = [&](const SearchIndexEntry* arr, size_t count, bool isHb) {
        for (size_t i = 0; i < count; ++i) {
            const SearchIndexEntry& e = arr[i];
            if (e.category != CAT_MONSTER) continue;
            if (typeFilter >= 0 && typeFilter < MT_COUNT && e.subType != static_cast<uint8_t>(typeFilter)) {
                continue;
            }
            if (!matchesCrFilter(e.crOrLevel, crFilter)) continue;

            int sc = scoreNameMatch(e.normalizedName, e.charBitmask, normQ, qLen, qMask);
            if (sc > 0) {
                SearchHit hit;
                hit.recordId   = e.recordId;
                hit.category   = e.category;
                hit.crOrLevel  = e.crOrLevel;
                hit.subType    = e.subType;
                hit.isHomebrew = isHb;
                hit.score      = sc;
                outResults.push_back(hit);
            }
        }
    };

    scanArray(searchIdx_, header_.searchEntryCount, false);
    if (!hbSearchEntries_.empty()) {
        scanArray(hbSearchEntries_.data(), hbSearchEntries_.size(), true);
    }

    if (qLen > 0) {
        std::stable_sort(outResults.begin(), outResults.end(),
                         [](const SearchHit& a, const SearchHit& b) {
                             return a.score > b.score;
                         });
    }

    if (maxResults > 0 && static_cast<int>(outResults.size()) > maxResults) {
        outResults.resize(static_cast<size_t>(maxResults));
    }
}

void SrdDatabase::searchRules(const char* query, bool conditionsOnly,
                              std::vector<SearchHit>& outResults, int maxResults) const {
    outResults.clear();
    if (!loaded_) return;

    char normQ[32];
    uint32_t qMask = 0;
    normalizeQuery(query, normQ, sizeof(normQ), qMask);
    size_t qLen = std::strlen(normQ);

    for (uint32_t i = 0; i < header_.searchEntryCount; ++i) {
        const SearchIndexEntry& e = searchIdx_[i];
        if (conditionsOnly) {
            if (e.category != CAT_CONDITION) continue;
        } else {
            if (e.category != CAT_CONDITION && e.category != CAT_RULE) continue;
        }

        int sc = scoreNameMatch(e.normalizedName, e.charBitmask, normQ, qLen, qMask);
        if (sc > 0) {
            SearchHit hit;
            hit.recordId   = e.recordId;
            hit.category   = e.category;
            hit.crOrLevel  = e.crOrLevel;
            hit.subType    = e.subType;
            hit.isHomebrew = false;
            hit.score      = sc;
            outResults.push_back(hit);
        }
    }

    if (qLen > 0) {
        std::stable_sort(outResults.begin(), outResults.end(),
                         [](const SearchHit& a, const SearchHit& b) {
                             return a.score > b.score;
                         });
    }

    if (maxResults > 0 && static_cast<int>(outResults.size()) > maxResults) {
        outResults.resize(static_cast<size_t>(maxResults));
    }
}

// Lightweight helper to extract a JSON string or integer value from an object slice
static std::string extractJsonStr(const std::string& obj, const char* key, const char* defVal = "") {
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

static int extractJsonInt(const std::string& obj, const char* key, int defVal) {
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

void SrdDatabase::parseHomebrewJsonFile(const std::string& filePath) {
    FILE* fp = std::fopen(filePath.c_str(), "rb");
    if (!fp) return;
    std::fseek(fp, 0, SEEK_END);
    long sz = std::ftell(fp);
    if (sz <= 0 || sz > 512 * 1024) {
        std::fclose(fp);
        return;
    }
    std::fseek(fp, 0, SEEK_SET);
    std::string content(static_cast<size_t>(sz), '\0');
    if (std::fread(&content[0], 1, content.size(), fp) != content.size()) {
        std::fclose(fp);
        return;
    }
    std::fclose(fp);

    // Find "monsters" array in the JSON file
    size_t arrPos = content.find("\"monsters\"");
    if (arrPos == std::string::npos) return;
    arrPos = content.find('[', arrPos);
    if (arrPos == std::string::npos) return;

    size_t pos = arrPos + 1;
    while (pos < content.size() && hbMonsters_.size() < 256) {
        while (pos < content.size() && content[pos] != '{' && content[pos] != ']') ++pos;
        if (pos >= content.size() || content[pos] == ']') break;

        int depth = 0;
        size_t start = pos;
        bool inStr = false;
        for (; pos < content.size(); ++pos) {
            char c = content[pos];
            if (c == '\\' && inStr) { ++pos; continue; }
            if (c == '"') inStr = !inStr;
            if (!inStr) {
                if (c == '{') ++depth;
                else if (c == '}') {
                    --depth;
                    if (depth == 0) { ++pos; break; }
                }
            }
        }
        std::string obj = content.substr(start, pos - start);
        std::string name = extractJsonStr(obj, "name");
        if (name.empty()) continue;

        HomebrewMonster hb;
        hb.id           = static_cast<uint16_t>(HOMEBREW_MONSTER_ID_BASE + hbMonsters_.size());
        hb.name         = name;
        hb.sizeAndAlign = extractJsonStr(obj, "size_align", "Medium humanoid, unaligned");
        hb.armorClass   = static_cast<uint16_t>(std::max(1, extractJsonInt(obj, "ac", 12)));
        hb.hitPoints    = static_cast<uint16_t>(std::max(1, extractJsonInt(obj, "hp", 10)));
        hb.speed        = extractJsonStr(obj, "speed", "30 ft.");
        hb.strScore     = static_cast<uint8_t>(std::clamp(extractJsonInt(obj, "str", 10), 1, 30));
        hb.dexScore     = static_cast<uint8_t>(std::clamp(extractJsonInt(obj, "dex", 10), 1, 30));
        hb.conScore     = static_cast<uint8_t>(std::clamp(extractJsonInt(obj, "con", 10), 1, 30));
        hb.intScore     = static_cast<uint8_t>(std::clamp(extractJsonInt(obj, "int", 10), 1, 30));
        hb.wisScore     = static_cast<uint8_t>(std::clamp(extractJsonInt(obj, "wis", 10), 1, 30));
        hb.chaScore     = static_cast<uint8_t>(std::clamp(extractJsonInt(obj, "cha", 10), 1, 30));
        hb.initBonus    = static_cast<int8_t>(abilityModifier(hb.dexScore));
        int crInt       = extractJsonInt(obj, "cr", 1);
        hb.crIndex      = static_cast<uint8_t>(crInt <= 0 ? 0 : std::min(33, crInt + 3));
        hb.defenses     = extractJsonStr(obj, "defenses", "");
        hb.sensesLang   = extractJsonStr(obj, "senses", "");

        std::string actionText = extractJsonStr(obj, "action", "");
        if (!actionText.empty()) {
            HomebrewAction act;
            act.name        = extractJsonStr(obj, "action_name", "Attack");
            act.summary     = actionText.substr(0, 72);
            act.fullDesc    = actionText;
            act.sectionType = 1;
            hb.actions.push_back(act);
        }

        SearchIndexEntry se{};
        uint32_t mask = 0;
        normalizeQuery(hb.name.c_str(), se.normalizedName, sizeof(se.normalizedName), mask);
        se.charBitmask = mask;
        se.recordId    = hb.id;
        se.category    = CAT_MONSTER;
        se.crOrLevel   = hb.crIndex;
        se.subType     = hb.type;

        hbMonsters_.push_back(hb);
        hbSearchEntries_.push_back(se);
    }
}

int SrdDatabase::loadHomebrewDir(const char* dirPath) {
    DIR* dir = opendir(dirPath);
    if (!dir) return 0;

    int before = static_cast<int>(hbMonsters_.size());
    struct dirent* ent = nullptr;
    while ((ent = readdir(dir)) != nullptr) {
        std::string fname = ent->d_name;
        if (fname.size() > 5 && fname.substr(fname.size() - 5) == ".json") {
            std::string fullPath = std::string(dirPath) + "/" + fname;
            parseHomebrewJsonFile(fullPath);
        }
    }
    closedir(dir);
    return static_cast<int>(hbMonsters_.size()) - before;
}

} // namespace dmtk
