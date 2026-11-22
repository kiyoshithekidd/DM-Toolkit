#include "srd_db.h"
#include "combat_tracker.h"
#include "campaign_io.h"
#include "generators.h"
#include "dice_roller.h"
#include "sound_fx.h"

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
        {"Bram", "Fighter", 2, 1, 16, 24, 24, true},
        {"Lirael", "Wizard", 4, 1, 14, 16, 16, true}
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
    assert(tracker.canUndoHp());
    assert(tracker.undoLastHp());
    assert(!tracker.canUndoHp());
    tracker.applyHpDelta(wolfIdx, -5);
    tracker.toggleCondition(wolfIdx, 11);
    assert((tracker.combatants()[static_cast<size_t>(wolfIdx)].conditionsMask & COND_PRONE) != 0);

    // Verify Player HP tracking works in CombatTracker
    int playerIdx = -1;
    for (size_t i = 0; i < tracker.combatants().size(); ++i) {
        if (tracker.combatants()[i].isPlayer) {
            playerIdx = static_cast<int>(i);
            break;
        }
    }
    assert(playerIdx >= 0);
    int startPlayerHp = tracker.combatants()[static_cast<size_t>(playerIdx)].currentHp;
    tracker.applyHpDelta(playerIdx, -6);
    assert(tracker.combatants()[static_cast<size_t>(playerIdx)].currentHp == startPlayerHp - 6);

    std::printf("[PASS] CombatTracker (initiative, mid-fight add, Player & Monster HP, Undo HP, conditions)\n");
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

    // Test Multi-Dice Pool (8d6 + 4)
    roller.rollPool(DIE_D6, 8, 4, ROLL_NORMAL, rng);
    assert(roller.activeDieCount() == 8);
    assert(roller.currentPoolCount() == 8);
    roller.finishImmediately();
    assert(!roller.isRolling());
    int sum8d6 = 0;
    for (int i = 0; i < 8; ++i) {
        int r = roller.dieAt(i).result;
        assert(r >= 1 && r <= 6);
        sum8d6 += r;
    }
    assert(roller.lastTotal() == sum8d6 + 4);
    assert(roller.history()[0].rolls.size() == 8);
    assert(roller.history()[0].count == 8);

    // Test Max Pool (12d4)
    roller.rollPool(DIE_D4, 12, 0, ROLL_NORMAL, rng);
    assert(roller.activeDieCount() == 12);
    roller.finishImmediately();
    assert(roller.history()[0].rolls.size() == 12);

    std::printf("[PASS] DiceRoller 3D kinematics, quaternion face alignment & Multi-Dice Pools (1-12 dice)\n");
}

static void testCampaignPersistenceAndHomebrew(SrdDatabase& db) {
    const std::string testRoot = "/tmp/dmtk_test_sd_v2";
    CampaignManager mgr(testRoot);
    assert(mgr.ensureDefaultCampaign(db));

    // Clean any previous on-device homebrew file so test is deterministic
    std::string onDevicePath = mgr.homebrewDir() + "/on_device_homebrew.json";
    std::remove(onDevicePath.c_str());

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

    // Test On-Device Homebrew Monster creation & immediate searchability
    HomebrewMonster customMon;
    customMon.name         = "Ironfang Wyvern";
    customMon.sizeAndAlign = "Large dragon, unaligned";
    customMon.type         = MT_DRAGON;
    customMon.armorClass   = 17;
    customMon.hitPoints    = 95;
    customMon.crIndex      = 10; // CR 6
    HomebrewAction act;
    act.name     = "Tail Stinger";
    act.summary  = "+8 to hit, 2d6+4 piercing + poison";
    act.fullDesc = "Melee Weapon Attack: +8 to hit, reach 10 ft., one creature.";
    act.attackBonus = 8;
    customMon.actions.push_back(act);
    assert(db.saveHomebrewMonster(customMon, mgr.homebrewDir().c_str()));

    db.searchMonsters("ironfang", -1, CR_FILTER_ALL, hits, 5);
    assert(!hits.empty());
    assert(hits[0].isHomebrew);
    MonsterView mv{};
    assert(db.getMonster(hits[0].recordId, mv));
    assert(std::string(mv.name) == "Ironfang Wyvern");
    assert(mv.armorClass == 17 && mv.hitPoints == 95);

    // Test On-Device Homebrew Spell creation & immediate searchability
    HomebrewSpell customSpell;
    customSpell.name        = "Lantern Ward";
    customSpell.level       = 2;
    customSpell.school      = "Abjuration";
    customSpell.castingTime = "1 action";
    customSpell.range       = "30 feet";
    customSpell.components  = "V, S";
    customSpell.duration    = "10 minutes";
    customSpell.body        = "A warm floating lantern wards allies against ambush.";
    assert(db.saveHomebrewSpell(customSpell, mgr.homebrewDir().c_str()));

    db.searchReference("lantern ward", REF_TAB_SPELLS, hits, 5);
    assert(!hits.empty());
    assert(hits[0].isHomebrew);
    assert(hits[0].category == CAT_SPELL);
    SpellView sv{};
    assert(db.getSpell(hits[0].recordId, sv));
    assert(std::string(sv.name) == "Lantern Ward");
    assert(sv.level == 2);

    // Test full reloadHomebrewDir round-trip from disk (custom_entries.json + on_device_homebrew.json)
    int reloaded = db.reloadHomebrewDir(mgr.homebrewDir().c_str());
    assert(reloaded == 4);
    db.searchMonsters("ironfang", -1, CR_FILTER_ALL, hits, 5);
    assert(!hits.empty() && hits[0].isHomebrew);
    db.searchReference("lantern ward", REF_TAB_SPELLS, hits, 5);
    assert(!hits.empty() && hits[0].isHomebrew);

    // Test UserSettings persistence (settings.json including sound_enabled)
    UserSettings usOut;
    usOut.diceSkinIdx      = 3;
    usOut.traySkinIdx      = 2;
    usOut.soundEnabled     = false;
    usOut.lastCampaignSlug = "starter_campaign";
    assert(mgr.saveSettings(usOut));

    UserSettings usIn;
    assert(mgr.loadSettings(usIn));
    assert(usIn.diceSkinIdx == 3);
    assert(usIn.traySkinIdx == 2);
    assert(usIn.soundEnabled == false);
    assert(usIn.lastCampaignSlug == "starter_campaign");

    // Test SoundFx host stub lifecycle
    SoundFx sfx;
    sfx.init();
    sfx.setEnabled(false);
    assert(!sfx.isEnabled());
    sfx.play(SFX_DICE_ROLL);
    sfx.setEnabled(true);
    assert(sfx.isEnabled());
    sfx.play(SFX_NAT20);
    sfx.shutdown();

    // Test SD Card Party Character Importer (party/*.json)
    std::string partyFile = mgr.partyImportDir() + "/player_test.json";
    FILE* pfp = std::fopen(partyFile.c_str(), "wb");
    assert(pfp != nullptr);
    std::fprintf(pfp,
                 "{\"name\": \"Kaelen\", \"class\": \"Paladin\", \"level\": 4, "
                 "\"ac\": 18, \"max_hp\": 36, \"current_hp\": 36, \"init_bonus\": 1}\n");
    std::fclose(pfp);

    CampaignData camp;
    assert(mgr.loadCampaign("starter_campaign", camp));
    int imported = mgr.importPartyCharacters(camp);
    assert(imported == 1);
    bool foundKaelen = false;
    for (const auto& pm : camp.party) {
        if (pm.name == "Kaelen") {
            foundKaelen = true;
            assert(pm.className == "Paladin");
            assert(pm.level == 4);
            assert(pm.ac == 18);
            assert(pm.maxHp == 36);
        }
    }
    assert(foundKaelen);

    // Test on-device homebrew in-place update (no duplicates) & +X to hit auto-parsing
    uint32_t monCountBefore = db.homebrewMonsterCount();
    HomebrewMonster updatedMon = customMon;
    updatedMon.hitPoints = 110;
    updatedMon.actions[0].attackBonus = -128; // Should auto-parse +8 from fullDesc
    assert(db.saveHomebrewMonster(updatedMon, mgr.homebrewDir().c_str()));
    assert(db.homebrewMonsterCount() == monCountBefore);
    db.searchMonsters("ironfang", -1, CR_FILTER_ALL, hits, 5);
    assert(!hits.empty());
    assert(db.getMonster(hits[0].recordId, mv));
    assert(mv.hitPoints == 110);
    ActionView av{};
    assert(db.getMonsterAction(hits[0].recordId, 0, av));
    assert(av.attackBonus == 8);

    // Test campaign slug deduplication (creating same name does not overwrite starter_campaign)
    CampaignData dupA, dupB;
    assert(mgr.createCampaign("Duplicate Quest", dupA));
    assert(mgr.createCampaign("Duplicate Quest", dupB));
    assert(dupA.slug != dupB.slug);

    // Test depth-aware JSON parsing when nested objects have same keys as top-level
    std::string nestedPath = mgr.campaignPathForSlug("nested_test");
    FILE* nfp = std::fopen(nestedPath.c_str(), "wb");
    assert(nfp != nullptr);
    std::fprintf(nfp,
                 "{\"party\": [{\"name\": \"NestedHero\", \"level\": 9}], "
                 "\"campaign_name\": \"Top Level Name\", \"slug\": \"nested_test\", \"party_level\": 5}\n");
    std::fclose(nfp);
    CampaignData nestedLoaded;
    assert(mgr.loadCampaign("nested_test", nestedLoaded));
    assert(nestedLoaded.name == "Top Level Name");
    assert(nestedLoaded.partyLevel == 5);

    std::printf("[PASS] CampaignManager, UserSettings, SD Party Import, On-Device Homebrew & SoundFx\n");
}

int main() {
    SrdDatabase db;
    testSrdLoadAndSearch(db);
    testCombatTracker(db);
    testGenerators(db);
    testDiceRoller();
    testCampaignPersistenceAndHomebrew(db);
    std::printf("ALL DM'S TOOLKIT v1.2 TESTS PASSED!\n");
    return 0;
}
