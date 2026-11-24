#pragma once

#include "srd_types.h"
#include <cstdint>
#include <string>
#include <vector>

namespace dmtk {

enum CrFilter : uint8_t {
    CR_FILTER_ALL     = 0,
    CR_FILTER_0_1     = 1,  // CR 0 to 1 (crIndex 0..4)
    CR_FILTER_2_4     = 2,  // CR 2 to 4 (crIndex 5..7)
    CR_FILTER_5_10    = 3,  // CR 5 to 10 (crIndex 8..13)
    CR_FILTER_11_PLUS = 4,  // CR 11+ (crIndex 14..33)
    CR_FILTER_COUNT   = 5
};

enum RefTabFilter : uint8_t {
    REF_TAB_ALL       = 0,
    REF_TAB_SPELLS    = 1,
    REF_TAB_COMBAT    = 2, // Conditions + Combat Rules + Grappler Feat
    REF_TAB_CLASSES   = 3, // Classes + Races
    REF_TAB_GEAR      = 4, // Weapons + Armor + Gear + Magic Items
    REF_TAB_COUNT     = 5
};

struct SearchHit {
    uint16_t recordId;
    uint8_t  category;
    uint8_t  crOrLevel;
    uint8_t  subType;
    bool     isHomebrew;
    int      score;
};

struct HomebrewAction {
    std::string name;
    std::string summary;
    std::string fullDesc;
    int8_t      attackBonus = -128;
    uint8_t     sectionType = 1;
};

struct HomebrewMonster {
    uint16_t    id = 0;
    std::string name;
    std::string sizeAndAlign;
    uint8_t     type = MT_HUMANOID;
    uint8_t     crIndex = 4;
    uint32_t    xp = 200;
    uint16_t    armorClass = 12;
    std::string acNote;
    uint16_t    hitPoints = 10;
    std::string hitDice;
    std::string speed = "30 ft.";
    uint8_t     strScore = 10, dexScore = 10, conScore = 10;
    uint8_t     intScore = 10, wisScore = 10, chaScore = 10;
    int8_t      initBonus = 0;
    std::string saves;
    std::string skills;
    std::string defenses;
    std::string sensesLang;
    std::vector<HomebrewAction> actions;
    bool        fromOnDevice = false;
};

struct HomebrewSpell {
    uint16_t    id = 0;
    uint8_t     level = 1;
    uint8_t     flags = 0;
    std::string name;
    std::string school = "Evocation";
    std::string castingTime = "1 action";
    std::string range = "60 feet";
    std::string components = "V, S";
    std::string duration = "Instantaneous";
    std::string classes = "Wizard";
    std::string body;
    bool        fromOnDevice = false;
};

struct MonsterView {
    uint16_t    id;
    bool        isHomebrew;
    const char* name;
    const char* sizeAndAlign;
    uint8_t     type;
    uint8_t     crIndex;
    uint32_t    xp;
    uint16_t    armorClass;
    const char* acNote;
    uint16_t    hitPoints;
    const char* hitDice;
    const char* speed;
    uint8_t     strScore, dexScore, conScore, intScore, wisScore, chaScore;
    int8_t      initBonus;
    const char* saves;
    const char* skills;
    const char* defenses;
    const char* sensesLang;
    int         actionCount;
    uint16_t    terrainMask;
};

struct ActionView {
    const char* name;
    const char* summary;
    const char* fullDesc;
    int8_t      attackBonus;
    uint8_t     sectionType;
};

struct RuleView {
    uint16_t    id;
    bool        isCondition;
    uint8_t     conditionIndex;
    const char* name;
    const char* category;
    const char* body;
};

struct SpellView {
    uint16_t    id;
    bool        isHomebrew;
    uint8_t     level;
    bool        ritual;
    bool        concentration;
    const char* name;
    const char* school;
    const char* castingTime;
    const char* range;
    const char* components;
    const char* duration;
    const char* classes;
    const char* body;
};

struct ReferenceView {
    uint16_t    id;
    uint8_t     category;
    uint8_t     subType;
    const char* name;
    const char* subtitle;
    const char* body;
    uint16_t    linkedId;
};

constexpr uint16_t HOMEBREW_MONSTER_ID_BASE = 0x8000;
constexpr uint16_t HOMEBREW_SPELL_ID_BASE   = 0x8000;

class SrdDatabase {
public:
    bool loadFromFile(const char* path);
    int  loadHomebrewDir(const char* dirPath);
    int  reloadHomebrewDir(const char* dirPath);
    bool saveHomebrewMonster(const HomebrewMonster& mon, const char* homebrewDir, int editIdx = -1);
    bool saveHomebrewSpell(const HomebrewSpell& sp, const char* homebrewDir, int editIdx = -1);
    bool deleteHomebrewMonster(size_t hbIdx, const char* homebrewDir);
    bool deleteHomebrewSpell(size_t hbIdx, const char* homebrewDir);
    const std::vector<HomebrewMonster>& homebrewMonsters() const { return hbMonsters_; }
    const std::vector<HomebrewSpell>&   homebrewSpells() const { return hbSpells_; }

    bool isLoaded() const { return loaded_; }
    uint32_t monsterCount() const;
    uint32_t srdMonsterCount() const { return header_.monsterCount; }
    uint32_t homebrewMonsterCount() const { return static_cast<uint32_t>(hbMonsters_.size()); }
    uint32_t homebrewSpellCount() const { return static_cast<uint32_t>(hbSpells_.size()); }
    uint32_t ruleCount() const { return header_.ruleCount; }
    uint32_t spellCount() const { return header_.spellCount + static_cast<uint32_t>(hbSpells_.size()); }
    uint32_t refCount() const { return header_.refCount; }
    uint32_t searchEntryCount() const { return static_cast<uint32_t>(header_.searchEntryCount + hbSearchEntries_.size()); }

    bool getMonster(uint16_t monsterId, MonsterView& out) const;
    bool getMonsterAction(uint16_t monsterId, int actionIdx, ActionView& out) const;
    bool getRule(uint16_t ruleId, RuleView& out) const;
    bool getConditionRule(int conditionIndex, RuleView& out) const;
    bool getSpell(uint16_t spellId, SpellView& out) const;
    bool getReference(uint16_t refId, ReferenceView& out) const;

    void searchMonsters(const char* query, int typeFilter, CrFilter crFilter,
                        std::vector<SearchHit>& outResults, int maxResults = 64) const;
    void searchRules(const char* query, bool conditionsOnly,
                     std::vector<SearchHit>& outResults, int maxResults = 64) const;
    void searchReference(const char* query, RefTabFilter tab,
                         std::vector<SearchHit>& outResults, int maxResults = 64) const;

    const char* str(uint32_t offset) const;

private:
    bool                     loaded_ = false;
    std::vector<uint8_t>     raw_;
    SrdBinaryHeader          header_{};
    const MonsterRecord*     monsters_ = nullptr;
    const MonsterActionRef*  actions_ = nullptr;
    const RuleRecord*        rules_ = nullptr;
    const SpellRecord*       spells_ = nullptr;
    const ReferenceRecord*   refs_ = nullptr;
    const SearchIndexEntry*  searchIdx_ = nullptr;
    const char*              stringPool_ = nullptr;

    std::vector<HomebrewMonster>  hbMonsters_;
    std::vector<HomebrewSpell>    hbSpells_;
    std::vector<SearchIndexEntry> hbSearchEntries_;

    static bool matchesCrFilter(uint8_t crIndex, CrFilter filter);
    static bool matchesRefTab(uint8_t category, RefTabFilter tab);
    static void normalizeQuery(const char* in, char* out, size_t outSz, uint32_t& outMask);
    static int  scoreNameMatch(const char* normName, uint32_t nameMask,
                               const char* normQuery, size_t qLen, uint32_t qMask);
    bool hasHomebrewOverride(const char* normName) const;
    void parseHomebrewJsonFile(const std::string& filePath, bool isOnDeviceFile);
};

} // namespace dmtk
