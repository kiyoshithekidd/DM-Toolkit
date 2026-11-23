#pragma once

#include "combat_tracker.h"
#include "srd_db.h"
#include <cstdint>
#include <string>
#include <vector>

namespace dmtk {

enum EncounterDifficulty : uint8_t {
    DIFF_EASY   = 0,
    DIFF_MEDIUM = 1,
    DIFF_HARD   = 2,
    DIFF_DEADLY = 3,
    DIFF_COUNT  = 4
};

struct GeneratedNpc {
    std::string name;
    std::string ancestry;
    std::string occupation;
    std::string quirk;
};

struct GeneratedLoot {
    uint32_t                 cp = 0;
    uint32_t                 sp = 0;
    uint32_t                 gp = 0;
    uint32_t                 pp = 0;
    std::string              gemOrArt;
    std::vector<std::string> items;
    std::string              summary;
};

struct EncounterDifficultyEval {
    uint32_t            baseXp       = 0;
    uint32_t            adjustedXp   = 0;
    float               multiplier   = 1.0f;
    uint32_t            easyThresh   = 0;
    uint32_t            mediumThresh = 0;
    uint32_t            hardThresh   = 0;
    uint32_t            deadlyThresh = 0;
    EncounterDifficulty rating       = DIFF_EASY;
};

class Generators {
public:
    static const char* difficultyToString(EncounterDifficulty diff);

    static EncounterDifficultyEval evaluateEncounterDifficulty(const SrdDatabase& db,
                                                               const SavedEncounter& enc,
                                                               const std::vector<PartyMember>& party,
                                                               int fallbackLevel = 1);

    static SavedEncounter generateEncounter(const SrdDatabase& db,
                                            int partyLevel,
                                            int partySize,
                                            TerrainType terrain,
                                            EncounterDifficulty diff,
                                            uint32_t& rngState);

    static GeneratedNpc generateNpc(uint32_t& rngState);

    static GeneratedLoot generateLoot(const SrdDatabase& db,
                                      int tierCr,
                                      bool isHoard,
                                      uint32_t& rngState);

private:
    static uint32_t nextRand(uint32_t& rngState);
    static int randRange(uint32_t& rngState, int lo, int hi);
    static int rollDice(uint32_t& rngState, int count, int sides);
};

} // namespace dmtk
