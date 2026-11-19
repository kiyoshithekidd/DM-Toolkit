#include "srd_db.h"
#include "combat_tracker.h"
#include "campaign_io.h"
#include "generators.h"
#include "dice_roller.h"

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace dmtk;

static void testSrdLoadAndSearch(SrdDatabase& db) {
    assert(db.loadFromFile("romfs/data/srd.bin"));
    assert(db.isLoaded());
    assert(db.srdMonsterCount() == 334);
    assert(db.spellCount() == 319);
    assert(db.refCount() >= 650);
    assert(db.ruleCount() >= 45);

    for (int i = 0; i < CONDITION_COUNT; ++i) {
        RuleView rv{};
        assert(db.getConditionRule(i, rv));
        assert(rv.isCondition);
        assert(rv.name[0] != '\0');
        assert(rv.body[0] != '\0');
    }

    std::vector<SearchHit> hits;
    auto t0 = std::chrono::steady_clock::now();
    db.searchMonsters("gob", -1, CR_FILTER_ALL, hits, 20);
    auto t1 = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

    assert(!hits.empty());
    MonsterView mv{};
    assert(db.getMonster(hits[0].recordId, mv));
    assert(std::string(mv.name) == "Goblin");
    assert(mv.hitPoints == 7);
    assert(mv.armorClass == 15);
    assert(mv.actionCount >= 3);

    // Verify Spells search
    db.searchReference("fireball", REF_TAB_SPELLS, hits, 10);
    assert(!hits.empty());
    assert(hits[0].category == CAT_SPELL);
    SpellView sv{};
    assert(db.getSpell(hits[0].recordId, sv));
    assert(std::string(sv.name) == "Fireball");
    assert(sv.level == 3);

    // Verify Renamed SRD Alias ("Tiny Hut")
    db.searchReference("tiny hut", REF_TAB_SPELLS, hits, 10);
    assert(!hits.empty());

    // Verify Non-SRD Gap Stub ("Hex", "Sentinel")
    db.searchReference("hex", REF_TAB_SPELLS, hits, 10);
    assert(!hits.empty());
    assert(hits[0].category == CAT_SRD_GAP);
    ReferenceView rv{};
    assert(db.getReference(hits[0].recordId, rv));
    assert(rv.subType == GAP_NON_SRD);

    // Verify Classes, Races & Gear
    db.searchReference("wizard", REF_TAB_CLASSES, hits, 10);
    assert(!hits.empty() && hits[0].category == CAT_CLASS);
    db.searchReference("longsword", REF_TAB_GEAR, hits, 10);
    assert(!hits.empty() && hits[0].category == CAT_EQUIPMENT);

    std::printf("[PASS] SRD v2 load & search (334 monsters, 319 spells, %u refs/gaps, 'gob' in %lld us)\n",
                db.refCount(), static_cast<long long>(us));
}

static void testCombatTracker(const SrdDatabase& db) {
    CombatTracker tracker;
    uint32_t rng = 0x12345678u;

    std::vector<SearchHit> hits;
    db.searchMonsters("goblin", -1, CR_FILTER_ALL, hits, 5);
    uint16_t goblinId = hits[0].recordId;
    db.searchMonsters("wolf", -1, CR_FILTER_ALL, hits, 5);
    uint16_t wolfId = hits[0].recordId;

    SavedEncounter enc;
    enc.name  = "Test Ambush";
    enc.slots = {{goblinId, 3}};

    std::vector<PartyMember> party = {
        {"Bram", 2, 1, 16, true},
        {"Lirael", 4, 1, 14, true}
    };

    tracker.startEncounter(enc, party, db, rng, true);
    assert(tracker.isInCombat());
    assert(tracker.combatants().size() == 5);
    tracker.nextTurn();
    tracker.nextTurn();
    uint32_t activeIdBefore = tracker.combatants()[static_cast<size_t>(tracker.activeTurnIndex())].instanceId;

    int wolfIdx = tracker.addMonsterMidFight(wolfId, 1, db, rng);
    assert(wolfIdx >= 0);
    assert(tracker.combatants().size() == 6);
    assert(tracker.combatants()[static_cast<size_t>(tracker.activeTurnIndex())].instanceId == activeIdBefore);

    tracker.applyHpDelta(wolfIdx, -5);
    tracker.toggleCondition(wolfIdx, 11);
    assert((tracker.combatants()[static_cast<size_t>(wolfIdx)].conditionsMask & COND_PRONE) != 0);

    std::printf("[PASS] CombatTracker (initiative, mid-fight add, HP, conditions)\n");
}

static void testGenerators(const SrdDatabase& db) {
    uint32_t rng = 0xABCDEF01u;
    SavedEncounter enc = Generators::generateEncounter(db, 3, 4, TERRAIN_FOREST, DIFF_MEDIUM, rng);
    assert(!enc.slots.empty());
    for (const auto& s : enc.slots) {
        MonsterView mv{};
        assert(db.getMonster(s.monsterId, mv));
        assert(s.count >= 1);
    }

    GeneratedNpc npc = Generators::generateNpc(rng);
    assert(!npc.name.empty());
    assert(!npc.quirk.empty());

    GeneratedLoot loot = Generators::generateLoot(db, 6, true, rng);
    assert(!loot.summary.empty());
    assert(!loot.gemOrArt.empty());
    assert(!loot.items.empty());

    std::printf("[PASS] Generators (Random Encounter '%s', NPC '%s', Hoard Loot)\n",
                enc.name.c_str(), npc.name.c_str());
}

static void testDiceRoller() {
    DiceRoller roller;
    uint32_t rng = 0x77778888u;

    // Test every die shape settles cleanly with winning face pointing toward +Z
    for (int s = 0; s < DIE_SHAPE_COUNT; ++s) {
        DieShape shape = static_cast<DieShape>(s);
        roller.roll(shape, 3, ROLL_NORMAL, rng);
        assert(roller.isRolling());
        roller.finishImmediately();
        assert(!roller.isRolling());

        const DieBody& d = roller.dieA();
        assert(d.settled);
        std::vector<Vec3> normals;
        DiceRoller::getFaceNormals(shape == DIE_D100 ? DIE_D10 : shape, normals);
        int faceIdx = (shape == DIE_D100) ? (d.result / 10) : (d.result - 1);
        Vec3 worldNormal = DiceRoller::rotateVec(d.rot, normals[static_cast<size_t>(faceIdx)]);
        assert(worldNormal.z > 0.98f);
    }

    // Test Advantage & Disadvantage dual-die rolls
    roller.roll(DIE_D20, 5, ROLL_ADVANTAGE, rng);
    roller.finishImmediately();
    int expectedAdv = std::max(roller.dieA().result, roller.dieB().result) + 5;
    assert(roller.lastTotal() == expectedAdv);

    roller.roll(DIE_D20, -1, ROLL_DISADVANTAGE, rng);
    roller.finishImmediately();
    int expectedDis = std::min(roller.dieA().result, roller.dieB().result) - 1;
    assert(roller.lastTotal() == expectedDis);
    assert(!roller.history().empty());

    std::printf("[PASS] DiceRoller 3D kinematics & quaternion face alignment (d4..d100 + Adv/Dis)\n");
}

static void testCampaignPersistenceAndHomebrew(SrdDatabase& db) {
    const std::string testRoot = "/tmp/dmtk_test_sd_v2";
    CampaignManager mgr(testRoot);
    assert(mgr.ensureDefaultCampaign(db));

    std::string hbFile = mgr.homebrewDir() + "/custom_entries.json";
    FILE* fp = std::fopen(hbFile.c_str(), "wb");
    assert(fp != nullptr);
    std::fprintf(fp,
                 "{\"monsters\": [{\"name\": \"Clockwork Badger\", \"ac\": 16, \"hp\": 42, \"cr\": 3}],\n"
                 " \"spells\": [{\"name\": \"Hex\", \"level\": 1, \"school\": \"Enchantment\", \"desc\": \"Custom SD spell.\"}]}\n");
    std::fclose(fp);

    int added = db.loadHomebrewDir(mgr.homebrewDir().c_str());
    assert(added == 2);

    // Verify SD Homebrew spell "Hex" supersedes the [Non-SRD] "Hex" gap stub
    std::vector<SearchHit> hits;
    db.searchReference("hex", REF_TAB_SPELLS, hits, 5);
    assert(!hits.empty());
    assert(hits[0].isHomebrew);
    assert(hits[0].category == CAT_SPELL);
    for (const auto& h : hits) {
        if (h.category == CAT_SRD_GAP) {
            ReferenceView rv{};
            assert(db.getReference(h.recordId, rv));
            assert(std::string(rv.name) != "Hex");
        }
    }

    std::printf("[PASS] CampaignManager & SD Homebrew Gap Override\n");
}

int main() {
    SrdDatabase db;
    testSrdLoadAndSearch(db);
    testCombatTracker(db);
    testGenerators(db);
    testDiceRoller();
    testCampaignPersistenceAndHomebrew(db);
    std::printf("ALL DM'S TOOLKIT SLICE 2 TESTS PASSED!\n");
    return 0;
}
