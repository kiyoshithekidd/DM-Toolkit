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

static void testV150Features(SrdDatabase& db) {
    // 1. Dice expression parser (#28) & impulse (#30)
    DiceExpr e1 = DiceRoller::parseDiceExpr("Melee Weapon Attack: +5 to hit, 2d6+4 slashing damage.");
    assert(e1.valid && e1.count == 2 && e1.sides == 6 && e1.mod == 4 && e1.shape == DIE_D6);
    DiceExpr e2 = DiceRoller::parseDiceExpr("1d8 - 2 cold damage");
    assert(e2.valid && e2.count == 1 && e2.sides == 8 && e2.mod == -2 && e2.shape == DIE_D8);
    DiceExpr e3 = DiceRoller::parseDiceExpr("No dice here");
    assert(!e3.valid);

    DiceRoller dr;
    uint32_t rng = 0x12345u;
    dr.rollPool(DIE_D20, 1, 0, ROLL_NORMAL, rng);
    dr.finishImmediately();
    assert(!dr.isRolling());
    dr.applyImpulse(1.5f, -1.0f);
    assert(dr.isRolling());

    // 2. CombatTracker QoL (#29): Concentration, Death Saves, Auto-Skip Dead
    std::vector<PartyMember> party = {{"Aria", "Wizard", 2, 3, 14, 22, 22, true}};
    std::vector<SearchHit> gobs;
    db.searchMonsters("goblin", -1, CR_FILTER_ALL, gobs, 5);
    assert(!gobs.empty());
    SavedEncounter enc;
    enc.name = "QoL Test";
    enc.slots.push_back({gobs[0].recordId, 1});

    CombatTracker tr;
    tr.startEncounter(enc, party, db, rng, true);
    assert(tr.combatants().size() == 2);
    int heroIdx = tr.combatants()[0].isPlayer ? 0 : 1;
    int monIdx  = 1 - heroIdx;

    tr.toggleConcentration(heroIdx);
    assert(tr.combatants()[static_cast<size_t>(heroIdx)].concentrating);
    assert(CombatTracker::concentrationCheckDc(8) == 10);
    assert(CombatTracker::concentrationCheckDc(28) == 14);

    // Drop hero to 0 HP -> concentration drops, death saves work
    tr.applyHpDelta(heroIdx, -100);
    assert(tr.combatants()[static_cast<size_t>(heroIdx)].currentHp == 0);
    assert(!tr.combatants()[static_cast<size_t>(heroIdx)].concentrating);
    tr.recordDeathSave(heroIdx, true);
    tr.recordDeathSave(heroIdx, false);
    assert(tr.combatants()[static_cast<size_t>(heroIdx)].deathSuccesses == 1);
    assert(tr.combatants()[static_cast<size_t>(heroIdx)].deathFailures == 1);
    tr.stabilize(heroIdx);
    assert(tr.combatants()[static_cast<size_t>(heroIdx)].stabilized);
    tr.applyHpDelta(heroIdx, 5);
    assert(!tr.combatants()[static_cast<size_t>(heroIdx)].stabilized);
    assert(tr.combatants()[static_cast<size_t>(heroIdx)].deathSuccesses == 0);

    // Dead monster auto-skip
    tr.applyHpDelta(monIdx, -100);
    assert(tr.combatants()[static_cast<size_t>(monIdx)].currentHp == 0);
    tr.setAutoSkipDead(true);
    tr.nextTurn();
    assert(tr.activeTurnIndex() == heroIdx);

    // 3. Encounter Difficulty Calculator (#31)
    SavedEncounter bigEnc;
    bigEnc.name = "Goblin Horde";
    bigEnc.slots.push_back({gobs[0].recordId, 6});
    std::vector<PartyMember> lv1Party = {
        {"P1", "Fighter", 1, 1, 16, 12, 12, true},
        {"P2", "Cleric",  0, 1, 16, 10, 10, true},
        {"P3", "Rogue",   3, 1, 14, 9,  9,  true},
        {"P4", "Wizard",  2, 1, 12, 8,  8,  true},
    };
    EncounterDifficultyEval ev = Generators::evaluateEncounterDifficulty(db, bigEnc, lv1Party, 1);
    assert(ev.baseXp == 300);      // 6 * 50 XP
    assert(ev.multiplier == 2.0f); // 3-6 monsters = 2.0x
    assert(ev.adjustedXp == 600);
    assert(ev.rating == DIFF_DEADLY); // Lv1 party x4 deadly = 400 XP

    // 4. Pinned Favorites persistence (#32) & Homebrew delete (#33)
    CampaignManager mgr("/tmp/dmtk_test_v150");
    UserSettings us;
    us.autoSkipDead = false;
    us.pinnedEntries = {0x00010005u, 0x00020010u};
    assert(mgr.saveSettings(us));
    UserSettings usLoaded;
    assert(mgr.loadSettings(usLoaded));
    assert(usLoaded.autoSkipDead == false);
    assert(usLoaded.pinnedEntries.size() == 2 && usLoaded.pinnedEntries[1] == 0x00020010u);

    HomebrewMonster tmpMon;
    tmpMon.name = "Temp Ooze";
    tmpMon.armorClass = 10;
    tmpMon.hitPoints = 20;
    assert(db.saveHomebrewMonster(tmpMon, mgr.homebrewDir().c_str()));
    uint32_t hbBefore = db.homebrewMonsterCount();
    assert(hbBefore >= 1);
    assert(db.deleteHomebrewMonster(hbBefore - 1, mgr.homebrewDir().c_str()));
    assert(db.homebrewMonsterCount() == hbBefore - 1);

    std::printf("[PASS] v1.5.0 Engine Features (#28-#33)\n");
}

static void testV160FixesAndFeatures(SrdDatabase& db) {
    uint32_t rng = 0xBEEF2026u;

    // 1. DM Screen Cheat Sheet cards compiled into SRD (#53)
    std::vector<SearchHit> hits;
    db.searchReference("difficulty classes", REF_TAB_COMBAT, hits, 5);
    assert(!hits.empty());
    RuleView dmRv{};
    assert(db.getRule(hits[0].recordId, dmRv));
    assert(std::string(dmRv.category) == "DM Screen");
    db.searchReference("exhaustion levels", REF_TAB_COMBAT, hits, 5);
    assert(!hits.empty());

    // 2. High-CR >12 dice pool support (#46)
    DiceRoller dr;
    dr.rollPool(DIE_D6, 26, 8, ROLL_NORMAL, rng); // Ancient Red Dragon Fire Breath (26d6+8)
    assert(dr.currentPoolCount() == 26);
    assert(dr.activeDieCount() == DiceRoller::MAX_POOL_DICE);
    dr.finishImmediately();
    assert(dr.history()[0].rolls.size() == 26);
    int sum26 = 0;
    for (int r : dr.history()[0].rolls) {
        assert(r >= 1 && r <= 6);
        sum26 += r;
    }
    assert(dr.lastTotal() == sum26 + 8);

    // 3. Temp HP absorption, full Undo snapshot, Boss Legendary/Recharge, Lair Action, Defeated XP, prevTurn (#42, #51, #52)
    db.searchMonsters("adult red dragon", -1, CR_FILTER_ALL, hits, 5);
    assert(!hits.empty());
    uint16_t dragonId = hits[0].recordId;
    db.searchMonsters("goblin", -1, CR_FILTER_ALL, hits, 5);
    assert(!hits.empty());
    uint16_t goblinId = hits[0].recordId;

    SavedEncounter bossEnc;
    bossEnc.name = "Boss Fight";
    bossEnc.slots = {{dragonId, 1}, {goblinId, 1}};
    std::vector<PartyMember> party = {
        {"Aria", "Wizard", 2, 10, 15, 60, 60, true, 14},
        {"Bram", "Fighter", 1, 10, 18, 90, 90, false, 11} // benched
    };

    CombatTracker tr;
    tr.startEncounter(bossEnc, party, db, rng, true);
    assert(tr.combatants().size() == 3); // Aria + Dragon + Goblin (Bram benched)

    int dragonIdx = -1, goblinIdx = -1, heroIdx = -1;
    for (size_t i = 0; i < tr.combatants().size(); ++i) {
        if (tr.combatants()[i].isPlayer) heroIdx = static_cast<int>(i);
        else if (tr.combatants()[i].monsterId == dragonId) dragonIdx = static_cast<int>(i);
        else if (tr.combatants()[i].monsterId == goblinId) goblinIdx = static_cast<int>(i);
    }
    assert(dragonIdx >= 0 && goblinIdx >= 0 && heroIdx >= 0);
    assert(tr.combatants()[static_cast<size_t>(dragonIdx)].legendMax == 3);
    assert(tr.combatants()[static_cast<size_t>(dragonIdx)].legendActions == 3);
    tr.useLegendaryAction(dragonIdx);
    assert(tr.combatants()[static_cast<size_t>(dragonIdx)].legendActions == 2);

    // Temp HP absorbs damage first, and Undo restores Temp HP + Concentration + Death Saves (#42, #52)
    tr.toggleConcentration(heroIdx);
    tr.setTempHp(heroIdx, 10);
    assert(tr.combatants()[static_cast<size_t>(heroIdx)].tempHp == 10);
    tr.applyHpDelta(heroIdx, -15); // 10 absorbed by temp, 5 to HP
    assert(tr.combatants()[static_cast<size_t>(heroIdx)].tempHp == 0);
    assert(tr.combatants()[static_cast<size_t>(heroIdx)].currentHp == 55);
    assert(tr.undoLastHp());
    assert(tr.combatants()[static_cast<size_t>(heroIdx)].tempHp == 10);
    assert(tr.combatants()[static_cast<size_t>(heroIdx)].currentHp == 60);

    // Drop concentrating hero to 0 -> concentration drops -> Undo restores concentration!
    tr.applyHpDelta(heroIdx, -100);
    assert(tr.combatants()[static_cast<size_t>(heroIdx)].currentHp == 0);
    assert(!tr.combatants()[static_cast<size_t>(heroIdx)].concentrating);
    assert(tr.undoLastHp());
    assert(tr.combatants()[static_cast<size_t>(heroIdx)].currentHp == 60);
    assert(tr.combatants()[static_cast<size_t>(heroIdx)].concentrating);

    // Kill goblin -> computeDefeatedXp == 50, prevTurn respects autoSkipDead
    tr.applyHpDelta(goblinIdx, -50);
    assert(tr.computeDefeatedXp(db) == 50);
    tr.setAutoSkipDead(true);
    tr.prevTurn();
    assert(tr.combatants()[static_cast<size_t>(tr.activeTurnIndex())].isPlayer ||
           tr.combatants()[static_cast<size_t>(tr.activeTurnIndex())].currentHp > 0);

    // Add Lair Action at Init 20
    int lairIdx = tr.addLairAction();
    assert(lairIdx >= 0 && tr.combatants()[static_cast<size_t>(lairIdx)].initRoll == 20);

    // 4. Campaign save/reload preserves conc, death saves, tempHp, legendActions, notes, npcs, world state, passive_perc (#42, #48, #49, #50)
    const std::string v160Root = "/tmp/dmtk_test_v160_fresh";
    CampaignManager mgr(v160Root);
    CampaignData camp;
    camp.name          = "Dragonspire";
    camp.slug          = "dragonspire";
    camp.partyLevel    = 10;
    camp.sessionNumber = 7;
    camp.dayNumber     = 14;
    camp.timeSlot      = 3;
    camp.weather       = 2;
    camp.party         = party;
    camp.notes.push_back({7, "Met the archmage at the tower."});
    camp.savedNpcs.push_back({"Bram Mossbank", "Dwarf", "Blacksmith", "Hums softly.", "Owes party a shield"});

    // Save directly without prior ensureDirectories() (#48)
    assert(mgr.saveCampaign(camp, &tr));

    CampaignData loaded;
    CombatTracker loadedFight;
    assert(mgr.loadCampaign("dragonspire", loaded, &loadedFight));
    assert(loaded.sessionNumber == 7 && loaded.dayNumber == 14 && loaded.timeSlot == 3 && loaded.weather == 2);
    assert(loaded.notes.size() == 1 && loaded.notes[0].sessionNum == 7);
    assert(loaded.savedNpcs.size() == 1 && loaded.savedNpcs[0].name == "Bram Mossbank");
    assert(loaded.party.size() == 2 && loaded.party[0].passivePerception == 14);
    assert(loadedFight.isInCombat());
    bool foundConcHero = false, foundBossLa = false;
    for (const auto& c : loadedFight.combatants()) {
        if (c.isPlayer && c.concentrating && c.tempHp == 10) foundConcHero = true;
        if (!c.isPlayer && c.monsterId == dragonId && c.legendMax == 3 && c.legendActions == 2) foundBossLa = true;
    }
    assert(foundConcHero && foundBossLa);

    // 5. importPartyCharacters does not overcount beyond 8-hero cap (#48)
    while (loaded.party.size() < 8) {
        loaded.party.push_back({"Filler" + std::to_string(loaded.party.size()), "Fighter", 0, 1, 14, 12, 12, true, 10});
    }
    std::string extraFile = mgr.partyImportDir() + "/overflow.json";
    FILE* ofp = std::fopen(extraFile.c_str(), "wb");
    assert(ofp != nullptr);
    std::fprintf(ofp, "{\"name\": \"NinthHero\", \"class\": \"Bard\", \"level\": 3}\n");
    std::fclose(ofp);
    int impCount = mgr.importPartyCharacters(loaded);
    assert(impCount == 0 && loaded.party.size() == 8);

    // 6. Homebrew rename updates in-place without duplicating & preserves type/xp (#44)
    HomebrewMonster hb;
    hb.name = "Frost Imp";
    hb.type = MT_FIEND;
    hb.armorClass = 13;
    hb.hitPoints = 22;
    hb.crIndex = 5; // CR 2
    assert(db.saveHomebrewMonster(hb, mgr.homebrewDir().c_str()));
    int editIdx = static_cast<int>(db.homebrewMonsterCount()) - 1;
    uint32_t beforeRename = db.homebrewMonsterCount();
    hb.name = "Frost Imp Lord";
    assert(db.saveHomebrewMonster(hb, mgr.homebrewDir().c_str(), editIdx));
    assert(db.homebrewMonsterCount() == beforeRename);
    assert(db.homebrewMonsters()[static_cast<size_t>(editIdx)].name == "Frost Imp Lord");
    assert(db.homebrewMonsters()[static_cast<size_t>(editIdx)].type == MT_FIEND);

    // 7. New Generators: Tavern, Quest Hook, Shop & exact SRD multiplier (#45, #53)
    assert(std::fabs(Generators::encounterMultiplier(1, 2) - 1.5f) < 0.01f); // small party +1 step
    assert(std::fabs(Generators::encounterMultiplier(3, 6) - 1.5f) < 0.01f); // large party -1 step
    GeneratedTavern tv = Generators::generateTavern(rng);
    assert(!tv.name.empty() && !tv.rumor.empty());
    GeneratedQuestHook qh = Generators::generateQuestHook(rng);
    assert(!qh.title.empty() && !qh.twist.empty());
    GeneratedShop sh = Generators::generateShop(db, 5, rng);
    assert(!sh.name.empty() && sh.stock.size() >= 4);

    std::printf("[PASS] v1.6.0 Bug Fixes (#42-#48) & Campaign Enhancements (#49-#53)\n");
}

int main() {
    SrdDatabase db;
    testSrdLoadAndSearch(db);
    testCombatTracker(db);
    testGenerators(db);
    testDiceRoller();
    testCampaignPersistenceAndHomebrew(db);
    testV150Features(db);
    testV160FixesAndFeatures(db);
    std::printf("ALL DM'S TOOLKIT v1.6.0 TESTS PASSED!\n");
    return 0;
}
