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
    hbSpells_.clear();
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

    if (header_.stringPoolOffset + header_.stringPoolSize > raw_.size()) return false;

    monsters_   = reinterpret_cast<const MonsterRecord*>(raw_.data() + header_.monstersOffset);
    actions_    = reinterpret_cast<const MonsterActionRef*>(raw_.data() + header_.actionsOffset);
    rules_      = reinterpret_cast<const RuleRecord*>(raw_.data() + header_.rulesOffset);
    spells_     = reinterpret_cast<const SpellRecord*>(raw_.data() + header_.spellsOffset);
    refs_       = reinterpret_cast<const ReferenceRecord*>(raw_.data() + header_.refsOffset);
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
        out.terrainMask  = 0xFF;
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
    out.terrainMask  = m.terrainMask;
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

bool SrdDatabase::getSpell(uint16_t spellId, SpellView& out) const {
    if (!loaded_) return false;
    if (spellId >= HOMEBREW_SPELL_ID_BASE) {
        size_t idx = static_cast<size_t>(spellId - HOMEBREW_SPELL_ID_BASE);
        if (idx >= hbSpells_.size()) return false;
        const HomebrewSpell& hs = hbSpells_[idx];
        out.id            = hs.id;
        out.isHomebrew    = true;
        out.level         = hs.level;
        out.ritual        = (hs.flags & 1) != 0;
        out.concentration = (hs.flags & 2) != 0;
        out.name          = hs.name.c_str();
        out.school        = hs.school.c_str();
        out.castingTime   = hs.castingTime.c_str();
        out.range         = hs.range.c_str();
        out.components    = hs.components.c_str();
        out.duration      = hs.duration.c_str();
        out.classes       = hs.classes.c_str();
        out.body          = hs.body.c_str();
        return true;
    }

    if (spellId >= header_.spellCount) return false;
    const SpellRecord& s = spells_[spellId];
    out.id            = s.id;
    out.isHomebrew    = false;
    out.level         = s.level;
    out.ritual        = (s.flags & 1) != 0;
    out.concentration = (s.flags & 2) != 0;
    out.name          = str(s.nameOffset);
    out.school        = str(s.schoolOffset);
    out.castingTime   = str(s.castingTimeOffset);
    out.range         = str(s.rangeOffset);
    out.components    = str(s.componentsOffset);
    out.duration      = str(s.durationOffset);
    out.classes       = str(s.classesOffset);
    out.body          = str(s.bodyOffset);
    return true;
}

bool SrdDatabase::getReference(uint16_t refId, ReferenceView& out) const {
    if (!loaded_ || refId >= header_.refCount) return false;
    const ReferenceRecord& r = refs_[refId];
    out.id       = r.id;
    out.category = r.category;
    out.subType  = r.subType;
    out.name     = str(r.nameOffset);
    out.subtitle = str(r.subtitleOffset);
    out.body     = str(r.bodyOffset);
    out.linkedId = r.linkedId;
    return true;
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

bool SrdDatabase::matchesRefTab(uint8_t category, RefTabFilter tab) {
    switch (tab) {
        case REF_TAB_ALL:
            return category != CAT_MONSTER;
        case REF_TAB_SPELLS:
            return category == CAT_SPELL || category == CAT_SRD_GAP;
        case REF_TAB_COMBAT:
            return category == CAT_RULE || category == CAT_CONDITION;
        case REF_TAB_CLASSES:
            return category == CAT_CLASS || category == CAT_RACE || category == CAT_SRD_GAP;
        case REF_TAB_GEAR:
            return category == CAT_EQUIPMENT;
        default:
            return category != CAT_MONSTER;
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

    if (std::strncmp(normName, normQuery, qLen) == 0) {
        return 1000 - static_cast<int>(nameLen);
    }

    for (size_t i = 1; i + qLen <= nameLen; ++i) {
        if (normName[i - 1] == ' ' && std::strncmp(normName + i, normQuery, qLen) == 0) {
            return 500 - static_cast<int>(i);
        }
    }

    const char* found = std::strstr(normName, normQuery);
    if (found) {
        int pos = static_cast<int>(found - normName);
        return 200 - pos;
    }

    return 0;
}

bool SrdDatabase::hasHomebrewOverride(const char* normName) const {
    for (const auto& hb : hbSearchEntries_) {
        if (std::strcmp(hb.normalizedName, normName) == 0) return true;
    }
    return false;
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
                SearchHit hit{e.recordId, e.category, e.crOrLevel, e.subType, isHb, sc};
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
            SearchHit hit{e.recordId, e.category, e.crOrLevel, e.subType, false, sc};
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

void SrdDatabase::searchReference(const char* query, RefTabFilter tab,
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
            if (!matchesRefTab(e.category, tab)) continue;
            // Hide Non-SRD gap stub if user provided a matching SD homebrew entry
            if (!isHb && e.category == CAT_SRD_GAP && hasHomebrewOverride(e.normalizedName)) {
                continue;
            }
            // Keep gap stubs out of empty-query browsing unless user is searching
            if (qLen == 0 && e.category == CAT_SRD_GAP) continue;

            int sc = scoreNameMatch(e.normalizedName, e.charBitmask, normQ, qLen, qMask);
            if (sc > 0) {
                // Slightly prioritize real SRD/Homebrew entries over gap stubs on ties
                if (e.category == CAT_SRD_GAP) sc -= 15;
                SearchHit hit{e.recordId, e.category, e.crOrLevel, e.subType, isHb, sc};
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

static size_t findTopLevelJsonValuePos(const std::string& obj, const char* key) {
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

static std::string extractJsonStr(const std::string& obj, const char* key, const char* defVal = "") {
    size_t pos = findTopLevelJsonValuePos(obj, key);
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

static int extractJsonInt(const std::string& obj, const char* key, int defVal) {
    size_t pos = findTopLevelJsonValuePos(obj, key);
    if (pos == std::string::npos || pos >= obj.size()) return defVal;
    return std::atoi(obj.c_str() + pos);
}

static void parseArrayObjects(const std::string& content, const char* arrayKey,
                              std::vector<std::string>& outObjs) {
    size_t arrPos = findTopLevelJsonValuePos(content, arrayKey);
    if (arrPos == std::string::npos || arrPos >= content.size() || content[arrPos] != '[') return;

    size_t pos = arrPos + 1;
    while (pos < content.size()) {
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
        outObjs.push_back(content.substr(start, pos - start));
    }
}

static int8_t parseAttackBonusFromText(const std::string& text) {
    for (size_t i = 0; i < text.size(); ++i) {
        if ((text[i] == '+' || text[i] == '-') && i + 1 < text.size() &&
            std::isdigit(static_cast<unsigned char>(text[i + 1]))) {
            int sign = (text[i] == '-') ? -1 : 1;
            size_t j = i + 1;
            int val = 0;
            while (j < text.size() && std::isdigit(static_cast<unsigned char>(text[j]))) {
                val = val * 10 + (text[j] - '0');
                ++j;
            }
            while (j < text.size() && text[j] == ' ') ++j;
            if (j + 6 <= text.size()) {
                std::string tail;
                for (size_t k = 0; k < 6; ++k) {
                    tail.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(text[j + k]))));
                }
                if (tail == "to hit") {
                    int bonus = sign * val;
                    return static_cast<int8_t>(std::clamp(bonus, -20, 30));
                }
            }
        }
    }
    return -128;
}

void SrdDatabase::parseHomebrewJsonFile(const std::string& filePath, bool isOnDeviceFile) {
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

    std::vector<std::string> monsterObjs;
    parseArrayObjects(content, "monsters", monsterObjs);
    for (const auto& obj : monsterObjs) {
        if (hbMonsters_.size() >= 256) break;
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
        hb.fromOnDevice = isOnDeviceFile;

        std::string actionText = extractJsonStr(obj, "action", "");
        if (!actionText.empty()) {
            HomebrewAction act;
            act.name        = extractJsonStr(obj, "action_name", "Attack");
            act.summary     = actionText.substr(0, 72);
            act.fullDesc    = actionText;
            act.attackBonus = parseAttackBonusFromText(actionText);
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

    std::vector<std::string> spellObjs;
    parseArrayObjects(content, "spells", spellObjs);
    for (const auto& obj : spellObjs) {
        if (hbSpells_.size() >= 256) break;
        std::string name = extractJsonStr(obj, "name");
        if (name.empty()) continue;

        HomebrewSpell hs;
        hs.id           = static_cast<uint16_t>(HOMEBREW_SPELL_ID_BASE + hbSpells_.size());
        hs.name         = name;
        hs.level        = static_cast<uint8_t>(std::clamp(extractJsonInt(obj, "level", 1), 0, 9));
        hs.school       = extractJsonStr(obj, "school", "Evocation");
        hs.castingTime  = extractJsonStr(obj, "casting_time", "1 action");
        hs.range        = extractJsonStr(obj, "range", "60 feet");
        hs.components   = extractJsonStr(obj, "components", "V, S");
        hs.duration     = extractJsonStr(obj, "duration", "Instantaneous");
        hs.classes      = extractJsonStr(obj, "classes", "Custom");
        hs.body         = extractJsonStr(obj, "desc", "Custom SD card spell entry.");
        hs.fromOnDevice = isOnDeviceFile;

        SearchIndexEntry se{};
        uint32_t mask = 0;
        normalizeQuery(hs.name.c_str(), se.normalizedName, sizeof(se.normalizedName), mask);
        se.charBitmask = mask;
        se.recordId    = hs.id;
        se.category    = CAT_SPELL;
        se.crOrLevel   = hs.level;
        se.subType     = 0;

        hbSpells_.push_back(hs);
        hbSearchEntries_.push_back(se);
    }
}

int SrdDatabase::loadHomebrewDir(const char* dirPath) {
    DIR* dir = opendir(dirPath);
    if (!dir) return 0;

    int before = static_cast<int>(hbMonsters_.size() + hbSpells_.size());
    struct dirent* ent = nullptr;
    while ((ent = readdir(dir)) != nullptr) {
        std::string fname = ent->d_name;
        if (fname.size() > 5 && fname.substr(fname.size() - 5) == ".json") {
            std::string fullPath = std::string(dirPath) + "/" + fname;
            bool isOnDevice = (fname == "on_device_homebrew.json");
            parseHomebrewJsonFile(fullPath, isOnDevice);
        }
    }
    closedir(dir);
    return static_cast<int>(hbMonsters_.size() + hbSpells_.size()) - before;
}

int SrdDatabase::reloadHomebrewDir(const char* dirPath) {
    hbMonsters_.clear();
    hbSpells_.clear();
    hbSearchEntries_.clear();
    return loadHomebrewDir(dirPath);
}

static std::string escJson(const std::string& s) {
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

static bool writeOnDeviceHomebrewFile(const char* homebrewDir,
                                      const std::vector<HomebrewMonster>& monsters,
                                      const std::vector<HomebrewSpell>& spells) {
    std::string finalPath = std::string(homebrewDir) + "/on_device_homebrew.json";
    std::string tmpPath   = finalPath + ".tmp";
    FILE* fp = std::fopen(tmpPath.c_str(), "wb");
    if (!fp) return false;

    std::vector<const HomebrewMonster*> onDevMonsters;
    for (const auto& m : monsters) {
        if (m.fromOnDevice) onDevMonsters.push_back(&m);
    }
    std::vector<const HomebrewSpell*> onDevSpells;
    for (const auto& s : spells) {
        if (s.fromOnDevice) onDevSpells.push_back(&s);
    }

    std::fprintf(fp, "{\n  \"monsters\": [\n");
    for (size_t i = 0; i < onDevMonsters.size(); ++i) {
        const auto& m = *onDevMonsters[i];
        int crInt = (m.crIndex <= 3) ? 0 : static_cast<int>(m.crIndex) - 3;
        const std::string& actName = m.actions.empty() ? std::string("Attack") : m.actions[0].name;
        const std::string& actDesc = m.actions.empty() ? std::string("") : m.actions[0].fullDesc;
        std::fprintf(fp,
                     "    {\"name\": \"%s\", \"size_align\": \"%s\", \"ac\": %u, \"hp\": %u, "
                     "\"cr\": %d, \"dex\": %u, \"action_name\": \"%s\", \"action\": \"%s\"}%s\n",
                     escJson(m.name).c_str(), escJson(m.sizeAndAlign).c_str(),
                     static_cast<unsigned>(m.armorClass), static_cast<unsigned>(m.hitPoints),
                     crInt, static_cast<unsigned>(m.dexScore),
                     escJson(actName).c_str(), escJson(actDesc).c_str(),
                     (i + 1 < onDevMonsters.size()) ? "," : "");
    }
    std::fprintf(fp, "  ],\n  \"spells\": [\n");
    for (size_t i = 0; i < onDevSpells.size(); ++i) {
        const auto& s = *onDevSpells[i];
        std::fprintf(fp,
                     "    {\"name\": \"%s\", \"level\": %u, \"school\": \"%s\", "
                     "\"casting_time\": \"%s\", \"range\": \"%s\", \"duration\": \"%s\", \"desc\": \"%s\"}%s\n",
                     escJson(s.name).c_str(), static_cast<unsigned>(s.level),
                     escJson(s.school).c_str(), escJson(s.castingTime).c_str(),
                     escJson(s.range).c_str(), escJson(s.duration).c_str(),
                     escJson(s.body).c_str(),
                     (i + 1 < onDevSpells.size()) ? "," : "");
    }
    std::fprintf(fp, "  ]\n}\n");
    std::fclose(fp);

    std::remove(finalPath.c_str());
    return std::rename(tmpPath.c_str(), finalPath.c_str()) == 0;
}

bool SrdDatabase::saveHomebrewMonster(const HomebrewMonster& mon, const char* homebrewDir) {
    if (mon.name.empty()) return false;
    HomebrewMonster copy = mon;
    copy.initBonus    = static_cast<int8_t>(abilityModifier(copy.dexScore));
    copy.fromOnDevice = true;
    for (auto& act : copy.actions) {
        if (act.attackBonus == -128 && !act.fullDesc.empty()) {
            act.attackBonus = parseAttackBonusFromText(act.fullDesc);
        }
    }

    SearchIndexEntry se{};
    uint32_t mask = 0;
    normalizeQuery(copy.name.c_str(), se.normalizedName, sizeof(se.normalizedName), mask);
    se.charBitmask = mask;
    se.category    = CAT_MONSTER;
    se.crOrLevel   = copy.crIndex;
    se.subType     = copy.type;

    for (size_t i = 0; i < hbSearchEntries_.size(); ++i) {
        if (hbSearchEntries_[i].category == CAT_MONSTER &&
            std::strcmp(hbSearchEntries_[i].normalizedName, se.normalizedName) == 0) {
            size_t idx = static_cast<size_t>(hbSearchEntries_[i].recordId - HOMEBREW_MONSTER_ID_BASE);
            if (idx < hbMonsters_.size() && hbMonsters_[idx].fromOnDevice) {
                copy.id = hbMonsters_[idx].id;
                se.recordId = copy.id;
                hbMonsters_[idx] = copy;
                hbSearchEntries_[i] = se;
                return writeOnDeviceHomebrewFile(homebrewDir, hbMonsters_, hbSpells_);
            }
        }
    }

    if (hbMonsters_.size() >= 256) return false;
    copy.id     = static_cast<uint16_t>(HOMEBREW_MONSTER_ID_BASE + hbMonsters_.size());
    se.recordId = copy.id;

    hbMonsters_.push_back(copy);
    hbSearchEntries_.push_back(se);
    return writeOnDeviceHomebrewFile(homebrewDir, hbMonsters_, hbSpells_);
}

bool SrdDatabase::saveHomebrewSpell(const HomebrewSpell& sp, const char* homebrewDir) {
    if (sp.name.empty()) return false;
    HomebrewSpell copy = sp;
    copy.fromOnDevice = true;

    SearchIndexEntry se{};
    uint32_t mask = 0;
    normalizeQuery(copy.name.c_str(), se.normalizedName, sizeof(se.normalizedName), mask);
    se.charBitmask = mask;
    se.category    = CAT_SPELL;
    se.crOrLevel   = copy.level;
    se.subType     = 0;

    for (size_t i = 0; i < hbSearchEntries_.size(); ++i) {
        if (hbSearchEntries_[i].category == CAT_SPELL &&
            std::strcmp(hbSearchEntries_[i].normalizedName, se.normalizedName) == 0) {
            size_t idx = static_cast<size_t>(hbSearchEntries_[i].recordId - HOMEBREW_SPELL_ID_BASE);
            if (idx < hbSpells_.size() && hbSpells_[idx].fromOnDevice) {
                copy.id = hbSpells_[idx].id;
                se.recordId = copy.id;
                hbSpells_[idx] = copy;
                hbSearchEntries_[i] = se;
                return writeOnDeviceHomebrewFile(homebrewDir, hbMonsters_, hbSpells_);
            }
        }
    }

    if (hbSpells_.size() >= 256) return false;
    copy.id     = static_cast<uint16_t>(HOMEBREW_SPELL_ID_BASE + hbSpells_.size());
    se.recordId = copy.id;

    hbSpells_.push_back(copy);
    hbSearchEntries_.push_back(se);
    return writeOnDeviceHomebrewFile(homebrewDir, hbMonsters_, hbSpells_);
}

} // namespace dmtk
