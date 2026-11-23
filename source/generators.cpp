#include "generators.h"

#include <algorithm>
#include <cstdio>

namespace dmtk {

uint32_t Generators::nextRand(uint32_t& rngState) {
    if (rngState == 0) rngState = 0x9E3779B9u;
    uint32_t x = rngState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rngState = x;
    return x;
}

int Generators::randRange(uint32_t& rngState, int lo, int hi) {
    if (hi <= lo) return lo;
    uint32_t span = static_cast<uint32_t>(hi - lo + 1);
    return lo + static_cast<int>(nextRand(rngState) % span);
}

int Generators::rollDice(uint32_t& rngState, int count, int sides) {
    int total = 0;
    for (int i = 0; i < count; ++i) {
        total += randRange(rngState, 1, sides);
    }
    return total;
}

const char* Generators::difficultyToString(EncounterDifficulty diff) {
    static const char* kDiffs[DIFF_COUNT] = {"Easy", "Medium", "Hard", "Deadly"};
    if (diff < DIFF_COUNT) return kDiffs[diff];
    return "Medium";
}

// SRD 5.1 XP thresholds per character by level (1..20)
static const uint16_t kXpThresholds[20][4] = {
    {  25,   50,   75,  100}, // Lv 1
    {  50,  100,  150,  200}, // Lv 2
    {  75,  150,  225,  400}, // Lv 3
    { 125,  250,  375,  500}, // Lv 4
    { 250,  500,  750, 1100}, // Lv 5
    { 300,  600,  900, 1400}, // Lv 6
    { 350,  750, 1100, 1700}, // Lv 7
    { 450,  900, 1400, 2100}, // Lv 8
    { 550, 1100, 1600, 2400}, // Lv 9
    { 600, 1200, 1900, 2800}, // Lv 10
    { 800, 1600, 2400, 3600}, // Lv 11
    {1000, 2000, 3000, 4500}, // Lv 12
    {1100, 2200, 3400, 5100}, // Lv 13
    {1250, 2500, 3800, 5700}, // Lv 14
    {1400, 2800, 4300, 6400}, // Lv 15
    {1600, 3200, 4800, 7200}, // Lv 16
    {2000, 3900, 5900, 8800}, // Lv 17
    {2100, 4200, 6300, 9500}, // Lv 18
    {2400, 4900, 7300,10900}, // Lv 19
    {2800, 5700, 8500,12700}  // Lv 20
};

EncounterDifficultyEval Generators::evaluateEncounterDifficulty(const SrdDatabase& db,
                                                                      const SavedEncounter& enc,
                                                                      const std::vector<PartyMember>& party,
                                                                      int fallbackLevel) {
    EncounterDifficultyEval ev;
    int partyCount = 0;
    for (const auto& p : party) {
        if (!p.active) continue;
        int lv = std::clamp(static_cast<int>(p.level), 1, 20);
        ev.easyThresh   += kXpThresholds[lv - 1][0];
        ev.mediumThresh += kXpThresholds[lv - 1][1];
        ev.hardThresh   += kXpThresholds[lv - 1][2];
        ev.deadlyThresh += kXpThresholds[lv - 1][3];
        ++partyCount;
    }
    if (partyCount == 0) {
        int lv = std::clamp(fallbackLevel, 1, 20);
        partyCount = 4;
        ev.easyThresh   = static_cast<uint32_t>(kXpThresholds[lv - 1][0]) * 4u;
        ev.mediumThresh = static_cast<uint32_t>(kXpThresholds[lv - 1][1]) * 4u;
        ev.hardThresh   = static_cast<uint32_t>(kXpThresholds[lv - 1][2]) * 4u;
        ev.deadlyThresh = static_cast<uint32_t>(kXpThresholds[lv - 1][3]) * 4u;
    }

    int totalMonsters = 0;
    for (const auto& s : enc.slots) {
        MonsterView mv{};
        if (db.getMonster(s.monsterId, mv)) {
            ev.baseXp += mv.xp * static_cast<uint32_t>(s.count);
            totalMonsters += static_cast<int>(s.count);
        }
    }

    static const float kMultSteps[7] = {0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 4.0f};
    int stepIdx = 1; // 1.0x
    if (totalMonsters >= 15)      stepIdx = 6;
    else if (totalMonsters >= 11) stepIdx = 5;
    else if (totalMonsters >= 7)  stepIdx = 4;
    else if (totalMonsters >= 3)  stepIdx = 3;
    else if (totalMonsters == 2)  stepIdx = 2;
    else                          stepIdx = 1;

    if (totalMonsters > 0) {
        if (partyCount <= 2)      stepIdx = std::min(6, stepIdx + 1);
        else if (partyCount >= 6) stepIdx = std::max(0, stepIdx - 1);
    }

    ev.multiplier = (totalMonsters > 0) ? kMultSteps[stepIdx] : 1.0f;
    ev.adjustedXp = static_cast<uint32_t>(static_cast<float>(ev.baseXp) * ev.multiplier + 0.5f);

    if (ev.adjustedXp >= ev.deadlyThresh && ev.deadlyThresh > 0)      ev.rating = DIFF_DEADLY;
    else if (ev.adjustedXp >= ev.hardThresh && ev.hardThresh > 0)   ev.rating = DIFF_HARD;
    else if (ev.adjustedXp >= ev.mediumThresh && ev.mediumThresh > 0) ev.rating = DIFF_MEDIUM;
    else                                                            ev.rating = DIFF_EASY;

    return ev;
}

SavedEncounter Generators::generateEncounter(const SrdDatabase& db,
                                           int partyLevel,
                                           int partySize,
                                           TerrainType terrain,
                                           EncounterDifficulty diff,
                                           uint32_t& rngState) {
    SavedEncounter enc;
    int lv = std::clamp(partyLevel, 1, 20);
    int ps = std::clamp(partySize, 1, 8);
    uint32_t budget = static_cast<uint32_t>(kXpThresholds[lv - 1][diff]) * static_cast<uint32_t>(ps);
    uint16_t terrainBit = static_cast<uint16_t>(1u << static_cast<uint8_t>(terrain));

    char titleBuf[64];
    std::snprintf(titleBuf, sizeof(titleBuf), "%s (%s, Lv %d)",
                  terrainToString(terrain), difficultyToString(diff), lv);
    enc.name = titleBuf;

    // Collect candidate monsters matching terrain and XP <= budget
    std::vector<uint16_t> candidates;
    std::vector<uint16_t> fallback;
    uint32_t count = db.srdMonsterCount();
    for (uint32_t i = 0; i < count; ++i) {
        MonsterView mv{};
        if (!db.getMonster(static_cast<uint16_t>(i), mv)) continue;
        if (mv.xp == 0 || mv.xp > budget) continue;
        if (mv.xp * 16 < budget && lv >= 4) continue; // Avoid 50 CR 0 rats at high level
        if ((mv.terrainMask & terrainBit) != 0) {
            candidates.push_back(mv.id);
        } else {
            fallback.push_back(mv.id);
        }
    }

    if (candidates.empty()) candidates = std::move(fallback);
    if (candidates.empty()) return enc;

    // Pick 1 primary monster type (and optionally a 2nd support type)
    uint32_t spent = 0;
    int attempts = 0;
    while (enc.slots.size() < 2 && attempts < 12) {
        ++attempts;
        uint16_t pickId = candidates[nextRand(rngState) % candidates.size()];
        MonsterView mv{};
        if (!db.getMonster(pickId, mv) || mv.xp == 0) continue;

        bool duplicate = false;
        for (const auto& s : enc.slots) {
            if (s.monsterId == pickId) duplicate = true;
        }
        if (duplicate) continue;

        uint32_t remaining = (budget > spent) ? (budget - spent) : 0;
        if (mv.xp > remaining && !enc.slots.empty()) continue;

        int maxQty = std::max(1, static_cast<int>(remaining / mv.xp));
        maxQty = std::min(maxQty, (enc.slots.empty() ? 4 : 2));
        int qty = randRange(rngState, 1, maxQty);
        // Factor in SRD encounter multiplier (~1.5x for 2 monsters, 2x for 3-6)
        uint32_t multNum = (qty >= 3) ? 2 : 1;
        while (qty > 1 && mv.xp * static_cast<uint32_t>(qty) * multNum > remaining + (budget / 4)) {
            --qty;
        }

        enc.slots.push_back({pickId, static_cast<uint8_t>(qty)});
        spent += mv.xp * static_cast<uint32_t>(qty);
        if (spent * 4 >= budget * 3) break;
    }

    return enc;
}

GeneratedNpc Generators::generateNpc(uint32_t& rngState) {
    static const char* kFirstNames[] = {
        "Aldric", "Bram", "Caelum", "Daria", "Elara", "Fenris",
        "Garrick", "Hilda", "Ivor", "Jora", "Kael", "Lirael",
        "Merek", "Nadia", "Orin", "Petra", "Quillan", "Rowan",
        "Soren", "Talia", "Ulric", "Vesper", "Wren", "Zara"
    };
    static const char* kSurnames[] = {
        "Ashford", "Blackwood", "Copperkettle", "Dunmore", "Emberfall",
        "Flintlock", "Greenbough", "Hawthorne", "Ironfoot", "Kestrel",
        "Larkspur", "Mossbank", "Nightbreeze", "Oakenshield", "Pennywhistle",
        "Ravenscroft", "Stonebridge", "Thornhill", "Underwood", "Vane"
    };
    static const char* kAncestries[] = {
        "Human", "Dwarf", "Elf", "Halfling", "Gnome", "Half-Orc", "Tiefling", "Dragonborn"
    };
    static const char* kOccupations[] = {
        "Innkeeper", "Blacksmith", "Herbalist", "Caravan Guard", "Scribe",
        "Dockmaster", "Bounty Hunter", "Apothecary", "Mapmaker", "Street Bard",
        "Watch Captain", "Relic Peddler", "Stablehand", "Lantern Lighter"
    };
    static const char* kQuirks[] = {
        "Speaks in hushed whispers and glances at doors.",
        "Collects bent copper coins for good luck.",
        "Polishes a dented pewter mug while talking.",
        "Laughs nervously before answering direct questions.",
        "Knows every rumor in town, for a small tip.",
        "Carries a pocket notebook full of half-drawn maps.",
        "Has a scarred knuckle and a surprisingly gentle voice.",
        "Always offers travelers spiced tea before business."
    };

    GeneratedNpc npc;
    const char* fn = kFirstNames[nextRand(rngState) % (sizeof(kFirstNames) / sizeof(kFirstNames[0]))];
    const char* sn = kSurnames[nextRand(rngState) % (sizeof(kSurnames) / sizeof(kSurnames[0]))];
    npc.name       = std::string(fn) + " " + sn;
    npc.ancestry   = kAncestries[nextRand(rngState) % (sizeof(kAncestries) / sizeof(kAncestries[0]))];
    npc.occupation = kOccupations[nextRand(rngState) % (sizeof(kOccupations) / sizeof(kOccupations[0]))];
    npc.quirk      = kQuirks[nextRand(rngState) % (sizeof(kQuirks) / sizeof(kQuirks[0]))];
    return npc;
}

GeneratedLoot Generators::generateLoot(const SrdDatabase& db,
                                       int tierCr,
                                       bool isHoard,
                                       uint32_t& rngState) {
    GeneratedLoot loot;
    int cr = std::clamp(tierCr, 0, 30);

    if (!isHoard) {
        if (cr <= 4) {
            loot.cp = static_cast<uint32_t>(rollDice(rngState, 3, 6));
            loot.sp = static_cast<uint32_t>(rollDice(rngState, 2, 6));
            if (randRange(rngState, 1, 4) == 1) loot.gp = static_cast<uint32_t>(rollDice(rngState, 1, 6));
        } else if (cr <= 10) {
            loot.sp = static_cast<uint32_t>(rollDice(rngState, 4, 6) * 5);
            loot.gp = static_cast<uint32_t>(rollDice(rngState, 2, 6) * 5);
        } else {
            loot.gp = static_cast<uint32_t>(rollDice(rngState, 3, 6) * 25);
            loot.pp = static_cast<uint32_t>(rollDice(rngState, 1, 6) * 5);
        }
    } else {
        if (cr <= 4) {
            loot.cp = static_cast<uint32_t>(rollDice(rngState, 6, 6) * 100);
            loot.sp = static_cast<uint32_t>(rollDice(rngState, 3, 6) * 100);
            loot.gp = static_cast<uint32_t>(rollDice(rngState, 2, 6) * 10);
        } else if (cr <= 10) {
            loot.sp = static_cast<uint32_t>(rollDice(rngState, 2, 6) * 100);
            loot.gp = static_cast<uint32_t>(rollDice(rngState, 6, 6) * 100);
            loot.pp = static_cast<uint32_t>(rollDice(rngState, 3, 6) * 10);
        } else {
            loot.gp = static_cast<uint32_t>(rollDice(rngState, 4, 6) * 1000);
            loot.pp = static_cast<uint32_t>(rollDice(rngState, 5, 6) * 100);
        }

        static const char* kGems[] = {
            "Azurite (10 gp)", "Quartz (50 gp)", "Bloodstone (50 gp)",
            "Amber (100 gp)", "Garnet (100 gp)", "Jade (100 gp)",
            "Black Pearl (500 gp)", "Ruby (5,000 gp)"
        };
        int gemIdx = (cr <= 4) ? randRange(rngState, 0, 2)
                   : (cr <= 10) ? randRange(rngState, 2, 5)
                                : randRange(rngState, 5, 7);
        loot.gemOrArt = kGems[gemIdx];

        // Pull 1-2 SRD magic items from the compiled database
        std::vector<SearchHit> gearHits;
        db.searchReference("", REF_TAB_GEAR, gearHits, 512);
        std::vector<uint16_t> magicIds;
        for (const auto& h : gearHits) {
            if (h.category == CAT_EQUIPMENT && h.subType == EQ_MAGIC_ITEM) {
                magicIds.push_back(h.recordId);
            }
        }
        if (!magicIds.empty()) {
            int numItems = randRange(rngState, 1, 2);
            for (int i = 0; i < numItems; ++i) {
                uint16_t rid = magicIds[nextRand(rngState) % magicIds.size()];
                ReferenceView rv{};
                if (db.getReference(rid, rv)) {
                    loot.items.push_back(std::string(rv.name) + " (" + rv.subtitle + ")");
                }
            }
        }
    }

    char buf[128];
    std::snprintf(buf, sizeof(buf), "%u pp, %u gp, %u sp, %u cp",
                  loot.pp, loot.gp, loot.sp, loot.cp);
    loot.summary = buf;
    return loot;
}

} // namespace dmtk
