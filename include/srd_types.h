#pragma once

#include <cstdint>
#include <cstddef>

namespace dmtk {

constexpr uint32_t SRD_BIN_VERSION = 1;

enum EntryCategory : uint8_t {
    CAT_MONSTER   = 0,
    CAT_SPELL     = 1,
    CAT_RULE      = 2,
    CAT_CONDITION = 3,
    CAT_CLASS     = 4,
    CAT_RACE      = 5,
    CAT_EQUIPMENT = 6,
    CAT_SRD_GAP   = 7
};

enum MonsterType : uint8_t {
    MT_ABERRATION  = 0,
    MT_BEAST       = 1,
    MT_CELESTIAL   = 2,
    MT_CONSTRUCT   = 3,
    MT_DRAGON      = 4,
    MT_ELEMENTAL   = 5,
    MT_FEY         = 6,
    MT_FIEND       = 7,
    MT_GIANT       = 8,
    MT_HUMANOID    = 9,
    MT_MONSTROSITY = 10,
    MT_OOZE        = 11,
    MT_PLANT       = 12,
    MT_UNDEAD      = 13,
    MT_COUNT       = 14
};

enum ConditionBit : uint16_t {
    COND_BLINDED       = (1u << 0),
    COND_CHARMED       = (1u << 1),
    COND_DEAFENED      = (1u << 2),
    COND_EXHAUSTION    = (1u << 3),
    COND_FRIGHTENED    = (1u << 4),
    COND_GRAPPLED      = (1u << 5),
    COND_INCAPACITATED = (1u << 6),
    COND_INVISIBLE     = (1u << 7),
    COND_PARALYZED     = (1u << 8),
    COND_PETRIFIED     = (1u << 9),
    COND_POISONED      = (1u << 10),
    COND_PRONE         = (1u << 11),
    COND_RESTRAINED    = (1u << 12),
    COND_STUNNED       = (1u << 13),
    COND_UNCONSCIOUS   = (1u << 14)
};

constexpr int CONDITION_COUNT = 15;

#pragma pack(push, 1)

struct SrdBinaryHeader {
    char     magic[4];         // "DMTK"
    uint32_t version;          // SRD_BIN_VERSION
    uint32_t monsterCount;
    uint32_t actionCount;
    uint32_t ruleCount;
    uint32_t searchEntryCount;
    uint32_t monstersOffset;
    uint32_t actionsOffset;
    uint32_t rulesOffset;
    uint32_t searchIndexOffset;
    uint32_t stringPoolOffset;
    uint32_t stringPoolSize;
};

struct MonsterActionRef {
    uint32_t nameOffset;
    uint32_t summaryOffset;
    uint32_t fullDescOffset;
    int8_t   attackBonus;      // -128 if not an attack roll
    uint8_t  sectionType;      // 0 = Trait, 1 = Action, 2 = Reaction, 3 = Legendary
    uint16_t reserved;
};

struct MonsterRecord {
    uint16_t id;
    uint8_t  type;             // MonsterType
    uint8_t  crIndex;          // 0=0, 1=1/8, 2=1/4, 3=1/2, 4=1 .. 33=30
    uint32_t nameOffset;
    uint32_t sizeAndAlignOffset;
    uint32_t acNoteOffset;
    uint32_t hitDiceOffset;
    uint32_t speedOffset;
    uint32_t savesOffset;
    uint32_t skillsOffset;
    uint32_t defensesOffset;
    uint32_t sensesLangOffset;
    uint32_t xp;
    uint16_t armorClass;
    uint16_t hitPoints;
    uint8_t  strScore;
    uint8_t  dexScore;
    uint8_t  conScore;
    uint8_t  intScore;
    uint8_t  wisScore;
    uint8_t  chaScore;
    int8_t   initBonus;
    uint8_t  actionCount;
    uint16_t firstActionIdx;
    uint16_t terrainMask;
};

struct RuleRecord {
    uint16_t id;
    uint8_t  isCondition;      // 1 = SRD Condition (0..14), 0 = Combat/General Rule
    uint8_t  conditionIndex;   // 0..14 if isCondition, else 0xFF
    uint32_t nameOffset;
    uint32_t categoryOffset;
    uint32_t bodyOffset;
};

struct SearchIndexEntry {
    char     normalizedName[32]; // Lowercase ASCII, null-terminated
    uint32_t charBitmask;        // 26-bit mask of letters a..z present in name
    uint16_t recordId;           // Index in MonsterRecord or RuleRecord
    uint8_t  category;           // EntryCategory
    uint8_t  crOrLevel;          // crIndex for monsters, 0 for rules
    uint8_t  subType;            // MonsterType for monsters, isCondition for rules
    uint8_t  reserved[3];
};

#pragma pack(pop)

static_assert(sizeof(SrdBinaryHeader) == 48, "SrdBinaryHeader size mismatch");
static_assert(sizeof(MonsterActionRef) == 16, "MonsterActionRef size mismatch");
static_assert(sizeof(MonsterRecord) == 60, "MonsterRecord size mismatch");
static_assert(sizeof(RuleRecord) == 16, "RuleRecord size mismatch");
static_assert(sizeof(SearchIndexEntry) == 44, "SearchIndexEntry size mismatch");

inline int abilityModifier(uint8_t score) {
    return (static_cast<int>(score) - 10 >= 0)
        ? (static_cast<int>(score) - 10) / 2
        : (static_cast<int>(score) - 11) / 2;
}

inline const char* crIndexToString(uint8_t crIndex) {
    static const char* kCrLabels[] = {
        "0", "1/8", "1/4", "1/2",
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "10",
        "11", "12", "13", "14", "15", "16", "17", "18", "19", "20",
        "21", "22", "23", "24", "25", "26", "27", "28", "29", "30"
    };
    if (crIndex < 34) return kCrLabels[crIndex];
    return "?";
}

inline const char* monsterTypeToString(uint8_t t) {
    static const char* kTypes[] = {
        "Aberration", "Beast", "Celestial", "Construct", "Dragon",
        "Elemental", "Fey", "Fiend", "Giant", "Humanoid",
        "Monstrosity", "Ooze", "Plant", "Undead"
    };
    if (t < MT_COUNT) return kTypes[t];
    return "Unknown";
}

inline const char* conditionIndexToName(int idx) {
    static const char* kNames[CONDITION_COUNT] = {
        "Blinded", "Charmed", "Deafened", "Exhaustion", "Frightened",
        "Grappled", "Incapacitated", "Invisible", "Paralyzed", "Petrified",
        "Poisoned", "Prone", "Restrained", "Stunned", "Unconscious"
    };
    if (idx >= 0 && idx < CONDITION_COUNT) return kNames[idx];
    return "Unknown";
}

inline const char* conditionIndexToShort(int idx) {
    static const char* kShort[CONDITION_COUNT] = {
        "BLD", "CHM", "DFN", "EXH", "FRT",
        "GRP", "INC", "INV", "PAR", "PTR",
        "PSN", "PRN", "RST", "STN", "UNC"
    };
    if (idx >= 0 && idx < CONDITION_COUNT) return kShort[idx];
    return "???";
}

} // namespace dmtk
