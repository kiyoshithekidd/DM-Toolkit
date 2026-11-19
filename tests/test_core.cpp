#include "srd_db.h"
#include "combat_tracker.h"
#include "campaign_io.h"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace dmtk;

static void testSrdLoadAndSearch(SrdDatabase& db) {
    assert(db.loadFromFile("romfs/data/srd.bin"));
    assert(db.isLoaded());
    assert(db.srdMonsterCount() == 334);
    assert(db.ruleCount() >= 45); // 15 conditions + combat rules

    // Verify all 15 SRD conditions exist
    for (int i = 0; i < CONDITION_COUNT; ++i) {
        RuleView rv{};
        assert(db.getConditionRule(i, rv));
        assert(rv.isCondition);
        assert(rv.name[0] != '\0');
        assert(rv.body[0] != '\0');
    }

    // Verify instant search ranking: "gob" -> Goblin #1
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
    assert(mv.actionCount >= 3); // Nimble Escape, Scimitar, Shortbow
    ActionView av{};
    assert(db.getMonsterAction(mv.id, 0, av));
    assert(av.name[0] != '\0');

    // Verify word-boundary search: "dragon" matches dragons
    db.searchMonsters("dragon", MT_DRAGON, CR_FILTER_11_PLUS, hits, 32);
    assert(hits.size() >= 10);

    // Verify Type + CR filter: Undead CR 0-1 includes Skeleton & Zombie
    db.searchMonsters("", MT_UNDEAD, CR_FILTER_0_1, hits, 32);
    bool foundSkeleton = false, foundZombie = false;
    for (const auto& h : hits) {
        MonsterView um{};
        assert(db.getMonster(h.recordId, um));
        assert(um.type == MT_UNDEAD);
        assert(um.crIndex <= 4);
        if (std::string(um.name) == "Skeleton") foundSkeleton = true;
        if (std::string(um.name) == "Zombie") foundZombie = true;
    }
    assert(foundSkeleton && foundZombie);

    // Verify Combat Rules search
    db.searchRules("prone", false, hits, 10);
    assert(!hits.empty());

    std::printf("[PASS] SRD load, filters, and search (334 monsters, query 'gob' in %lld us)\n",
                static_cast<long long>(us));
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
    assert(tracker.roundNumber() == 1);
    assert(tracker.activeTurnIndex() == 0);

    // Verify descending initiative order
    for (size_t i = 1; i < tracker.combatants().size(); ++i) {
        assert(tracker.combatants()[i - 1].initRoll >= tracker.combatants()[i].initRoll);
    }

    // Advance 2 turns and record active combatant instanceId
    tracker.nextTurn();
    tracker.nextTurn();
    uint32_t activeIdBefore = tracker.combatants()[static_cast<size_t>(tracker.activeTurnIndex())].instanceId;

    // Add a reinforcement mid-fight and verify active turn stays on the same combatant
    int wolfIdx = tracker.addMonsterMidFight(wolfId, 1, db, rng);
    assert(wolfIdx >= 0);
    assert(tracker.combatants().size() == 6);
    uint32_t activeIdAfter = tracker.combatants()[static_cast<size_t>(tracker.activeTurnIndex())].instanceId;
    assert(activeIdBefore == activeIdAfter);

    // Apply damage and conditions to the wolf
    int maxHp = tracker.combatants()[static_cast<size_t>(wolfIdx)].maxHp;
    tracker.applyHpDelta(wolfIdx, -5);
    assert(tracker.combatants()[static_cast<size_t>(wolfIdx)].currentHp == maxHp - 5);
    tracker.applyHpDelta(wolfIdx, -999);
    assert(tracker.combatants()[static_cast<size_t>(wolfIdx)].currentHp == 0);
    tracker.applyHpDelta(wolfIdx, +999);
    assert(tracker.combatants()[static_cast<size_t>(wolfIdx)].currentHp == maxHp);

    tracker.toggleCondition(wolfIdx, 11); // Prone
    assert((tracker.combatants()[static_cast<size_t>(wolfIdx)].conditionsMask & COND_PRONE) != 0);
    tracker.toggleCondition(wolfIdx, 11);
    assert((tracker.combatants()[static_cast<size_t>(wolfIdx)].conditionsMask & COND_PRONE) == 0);

    // Advance through a full round and confirm roundNumber increments
    int startRound = tracker.roundNumber();
    for (size_t i = 0; i < tracker.combatants().size(); ++i) {
        tracker.nextTurn();
    }
    assert(tracker.roundNumber() == startRound + 1);

    tracker.clear();
    assert(!tracker.isInCombat());
    std::printf("[PASS] CombatTracker (initiative, mid-fight add, HP clamping, conditions, rounds)\n");
}

static void testCampaignPersistenceAndHomebrew(SrdDatabase& db) {
    const std::string testRoot = "/tmp/dmtk_test_sd";
    CampaignManager mgr(testRoot);
    assert(mgr.ensureDefaultCampaign(db));

    auto list = mgr.listCampaigns();
    assert(!list.empty());

    CampaignData loaded;
    CombatTracker fight;
    assert(mgr.loadCampaign(list[0].slug, loaded, &fight));
    assert(loaded.name == "Starter Campaign");
    assert(loaded.party.size() == 4);
    assert(loaded.encounters.size() == 2);

    // Start fight, apply damage & condition, save active fight snapshot, reload
    uint32_t rng = 0xCAFEBABEu;
    fight.startEncounter(loaded.encounters[0], loaded.party, db, rng);
    fight.nextTurn();
    for (size_t i = 0; i < fight.combatants().size(); ++i) {
        if (!fight.combatants()[i].isPlayer) {
            fight.applyHpDelta(static_cast<int>(i), -3);
            fight.toggleCondition(static_cast<int>(i), 10); // Poisoned
            break;
        }
    }
    assert(mgr.saveCampaign(loaded, &fight));

    CampaignData reloaded;
    CombatTracker reloadedFight;
    assert(mgr.loadCampaign(loaded.slug, reloaded, &reloadedFight));
    assert(reloadedFight.isInCombat());
    assert(reloadedFight.combatants().size() == fight.combatants().size());
    assert(reloadedFight.activeTurnIndex() == fight.activeTurnIndex());

    // Test SD Homebrew Overlay
    std::string hbFile = mgr.homebrewDir() + "/custom_monsters.json";
    FILE* fp = std::fopen(hbFile.c_str(), "wb");
    assert(fp != nullptr);
    std::fprintf(fp,
                 "{\"monsters\": [{\"name\": \"Clockwork Badger\", \"ac\": 16, \"hp\": 42, "
                 "\"cr\": 3, \"dex\": 14, \"action_name\": \"Gear Bite\", "
                 "\"action\": \"+5 to hit, 1d10+3 piercing damage.\"}]}\n");
    std::fclose(fp);

    int added = db.loadHomebrewDir(mgr.homebrewDir().c_str());
    assert(added == 1);
    std::vector<SearchHit> hits;
    db.searchMonsters("clockwork", -1, CR_FILTER_ALL, hits, 5);
    assert(hits.size() == 1);
    assert(hits[0].isHomebrew);
    MonsterView mv{};
    assert(db.getMonster(hits[0].recordId, mv));
    assert(std::string(mv.name) == "Clockwork Badger");
    assert(mv.hitPoints == 42);
    assert(mv.armorClass == 16);

    std::printf("[PASS] CampaignManager atomic JSON save/load & SD homebrew overlay\n");
}

int main() {
    SrdDatabase db;
    testSrdLoadAndSearch(db);
    testCombatTracker(db);
    testCampaignPersistenceAndHomebrew(db);
    std::printf("ALL DM'S TOOLKIT CORE TESTS PASSED!\n");
    return 0;
}
