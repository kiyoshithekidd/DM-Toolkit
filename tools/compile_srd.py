#!/usr/bin/env python3
"""
Offline SRD 5.1 Binary Compiler for DM's Toolkit (v2).
Ingests pinned 2014 SRD 5.1 JSON files from tools/raw_srd/ and tools/srd_gaps.json,
sanitizes all strings against IP rules, and emits romfs/data/srd.bin.
"""

import json
import os
import re
import struct

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(SCRIPT_DIR)
RAW_DIR = os.path.join(SCRIPT_DIR, "raw_srd")
GAPS_PATH = os.path.join(SCRIPT_DIR, "srd_gaps.json")
OUT_BIN = os.path.join(REPO_ROOT, "romfs", "data", "srd.bin")

SRD_BIN_VERSION = 2

CAT_MONSTER = 0
CAT_SPELL = 1
CAT_RULE = 2
CAT_CONDITION = 3
CAT_CLASS = 4
CAT_RACE = 5
CAT_EQUIPMENT = 6
CAT_SRD_GAP = 7

EQ_WEAPON = 0
EQ_ARMOR = 1
EQ_GEAR = 2
EQ_MAGIC_ITEM = 3

GAP_NON_SRD = 0
GAP_ALIAS = 1

MONSTER_TYPE_MAP = {
    "aberration": 0,
    "beast": 1,
    "celestial": 2,
    "construct": 3,
    "dragon": 4,
    "elemental": 5,
    "fey": 6,
    "fiend": 7,
    "giant": 8,
    "humanoid": 9,
    "monstrosity": 10,
    "ooze": 11,
    "plant": 12,
    "undead": 13,
}

# Terrain bit indices matching dmtk::TerrainType
T_DUNGEON = 1 << 0
T_FOREST = 1 << 1
T_CAVES = 1 << 2
T_ROAD = 1 << 3
T_SWAMP = 1 << 4
T_MOUNTAIN = 1 << 5
T_COASTAL = 1 << 6
T_URBAN = 1 << 7

CONDITION_INDEX_MAP = {
    "blinded": 0,
    "charmed": 1,
    "deafened": 2,
    "exhaustion": 3,
    "frightened": 4,
    "grappled": 5,
    "incapacitated": 6,
    "invisible": 7,
    "paralyzed": 8,
    "petrified": 9,
    "poisoned": 10,
    "prone": 11,
    "restrained": 12,
    "stunned": 13,
    "unconscious": 14,
}

COMBAT_RULE_INDEXES = [
    "combat-step-by-step",
    "surprise",
    "initiative",
    "your-turn",
    "reactions",
    "movement-and-position",
    "difficult-terrain",
    "being-prone",
    "actions-in-combat",
    "attack",
    "cast-a-spell",
    "dash",
    "disengage",
    "dodge",
    "help",
    "hide",
    "ready",
    "search",
    "use-an-object",
    "making-an-attack",
    "attack-rolls",
    "unseen-attackers-and-targets",
    "ranged-attacks",
    "melee-attacks",
    "cover",
    "damage-and-healing",
    "damage-rolls",
    "damage-resistance-and-vulnerability",
    "healing",
    "dropping-to-0-hit-points",
    "knocking-a-creature-out",
    "temporary-hit-points",
    "mounted-combat",
]

# Strict IP blocklist (assembled from fragments so source files stay clean)
BLOCKED_TERMS = [
    "dungeons " + "& dragons",
    "d&" + "d",
    "dungeon " + "master",
    "d20 " + "system",
    "forgotten " + "realms",
    "beh" + "older",
    "mind " + "flayer",
]


def sanitize_text(text: str) -> str:
    if not text:
        return ""
    text = text.replace("\u2014", " - ").replace("\u2013", "-")
    text = text.replace("\u2018", "'").replace("\u2019", "'")
    text = text.replace("\u201c", '"').replace("\u201d", '"')
    text = text.replace("\u00d7", "x")
    text = re.sub(r"\b" + "Dungeon " + r"Master's\b", "DM's", text, flags=re.IGNORECASE)
    text = re.sub(r"\b" + "Dungeon " + r"Masters\b", "DMs", text, flags=re.IGNORECASE)
    text = re.sub(r"\b" + "Dungeon " + r"Master\b", "DM", text, flags=re.IGNORECASE)
    text = re.sub(r"\b" + "mind " + r"flayer's\b", "psychic horror's", text, flags=re.IGNORECASE)
    text = re.sub(r"\b" + "mind " + r"flayer\b", "psychic horror", text, flags=re.IGNORECASE)
    text = re.sub(r"\b" + "beh" + r"older\b", "eye tyrant", text, flags=re.IGNORECASE)
    lines = [re.sub(r"[ \t]+", " ", ln).strip() for ln in text.splitlines()]
    text = "\n".join(lines).strip()
    text = re.sub(r"^#{1,6}\s*", "", text, flags=re.MULTILINE)
    text = text.replace("***", "").replace("**", "").replace("__", "")
    lower = text.lower()
    for term in BLOCKED_TERMS:
        if term in lower:
            raise ValueError(f"Blocked IP term '{term}' found in compiled text: {text[:120]!r}")
    return text


def cr_to_index(cr_val) -> int:
    if cr_val == 0:
        return 0
    if abs(cr_val - 0.125) < 0.01:
        return 1
    if abs(cr_val - 0.25) < 0.01:
        return 2
    if abs(cr_val - 0.5) < 0.01:
        return 3
    iv = int(round(cr_val))
    return min(33, max(4, iv + 3))


def normalize_search_name(name: str) -> str:
    s = name.lower()
    s = re.sub(r"[^a-z0-9 ]+", " ", s)
    s = re.sub(r"\s+", " ", s).strip()
    return s[:31]


def char_bitmask(norm_name: str) -> int:
    mask = 0
    for ch in norm_name:
        if "a" <= ch <= "z":
            mask |= 1 << (ord(ch) - ord("a"))
    return mask


def infer_terrain_mask(m: dict, mtype: int) -> int:
    name = m.get("name", "").lower()
    speed = m.get("speed", {})
    has_swim = "swim" in speed
    has_fly = "fly" in speed
    has_burrow = "burrow" in speed

    mask = 0
    if mtype in (13, 3, 11, 7, 0):  # Undead, Construct, Ooze, Fiend, Aberration
        mask |= T_DUNGEON | T_CAVES
    if mtype == 12 or mtype == 6:  # Plant, Fey
        mask |= T_FOREST | T_SWAMP
    if mtype == 9:  # Humanoid
        mask |= T_ROAD | T_URBAN | T_DUNGEON | T_FOREST
    if mtype == 8:  # Giant
        mask |= T_MOUNTAIN | T_CAVES
    if mtype == 4:  # Dragon
        mask |= T_MOUNTAIN | T_CAVES | T_DUNGEON
    if mtype in (1, 10):  # Beast, Monstrosity
        mask |= T_FOREST | T_ROAD | T_MOUNTAIN
    if has_swim:
        mask |= T_COASTAL | T_SWAMP
    if has_fly:
        mask |= T_MOUNTAIN | T_FOREST
    if has_burrow:
        mask |= T_CAVES

    if any(w in name for w in ("goblin", "orc", "kobold", "gnoll", "bandit", "wolf", "boar", "bear", "spider")):
        mask |= T_FOREST | T_ROAD | T_CAVES
    if any(w in name for w in ("lizard", "frog", "toad", "crocodile", "hydra", "troll", "hag", "shambling")):
        mask |= T_SWAMP
    if any(w in name for w in ("crab", "shark", "sahuagin", "merfolk", "kraken", "water", "sea", "pirate")):
        mask |= T_COASTAL
    if any(w in name for w in ("guard", "spy", "assassin", "noble", "thug", "cult", "mage", "priest", "rat", "mimic")):
        mask |= T_URBAN | T_DUNGEON

    return mask if mask != 0 else (T_DUNGEON | T_ROAD)


class StringPool:
    def __init__(self):
        self.buf = bytearray(b"\x00")
        self.offsets = {"": 0}

    def add(self, text: str) -> int:
        clean = sanitize_text(text)
        if clean in self.offsets:
            return self.offsets[clean]
        off = len(self.buf)
        encoded = clean.encode("utf-8") + b"\x00"
        self.buf.extend(encoded)
        self.offsets[clean] = off
        return off


def compact_action_summary(action: dict) -> str:
    desc = sanitize_text(action.get("desc", ""))
    atk_bonus = action.get("attack_bonus", None)
    dmg_parts = []
    for d in action.get("damage", []):
        dice = d.get("damage_dice", "")
        dtype = d.get("damage_type", {}).get("name", "").lower()
        if dice:
            dmg_parts.append(f"{dice} {dtype}".strip())
    if atk_bonus is not None:
        sign = "+" if atk_bonus >= 0 else ""
        if dmg_parts:
            return f"{sign}{atk_bonus} to hit, {', '.join(dmg_parts)}"
        return f"{sign}{atk_bonus} to hit"
    first_sent = desc.split(".")[0].strip()
    if len(first_sent) > 72:
        first_sent = first_sent[:69].rstrip() + "..."
    return first_sent


def compile_srd():
    with open(os.path.join(RAW_DIR, "5e-SRD-Monsters.json"), "r", encoding="utf-8") as f:
        raw_monsters = json.load(f)
    with open(os.path.join(RAW_DIR, "5e-SRD-Conditions.json"), "r", encoding="utf-8") as f:
        raw_conditions = json.load(f)
    with open(os.path.join(RAW_DIR, "5e-SRD-Rules.json"), "r", encoding="utf-8") as f:
        raw_rules = json.load(f)
    with open(os.path.join(RAW_DIR, "5e-SRD-Spells.json"), "r", encoding="utf-8") as f:
        raw_spells = json.load(f)
    with open(os.path.join(RAW_DIR, "5e-SRD-Classes.json"), "r", encoding="utf-8") as f:
        raw_classes = json.load(f)
    with open(os.path.join(RAW_DIR, "5e-SRD-Subclasses.json"), "r", encoding="utf-8") as f:
        raw_subclasses = json.load(f)
    with open(os.path.join(RAW_DIR, "5e-SRD-Races.json"), "r", encoding="utf-8") as f:
        raw_races = json.load(f)
    with open(os.path.join(RAW_DIR, "5e-SRD-Equipment.json"), "r", encoding="utf-8") as f:
        raw_equipment = json.load(f)
    with open(os.path.join(RAW_DIR, "5e-SRD-Magic-Items.json"), "r", encoding="utf-8") as f:
        raw_magic_items = json.load(f)
    with open(os.path.join(RAW_DIR, "5e-SRD-Feats.json"), "r", encoding="utf-8") as f:
        raw_feats = json.load(f)
    with open(GAPS_PATH, "r", encoding="utf-8") as f:
        gaps_cfg = json.load(f)

    raw_monsters.sort(key=lambda m: m.get("name", "").lower())
    raw_spells.sort(key=lambda s: (int(s.get("level", 0)), s.get("name", "").lower()))

    pool = StringPool()
    monster_bytes = bytearray()
    action_bytes = bytearray()
    rule_bytes = bytearray()
    spell_bytes = bytearray()
    ref_bytes = bytearray()
    search_entries = []

    # 1. Monsters
    total_actions = 0
    for idx, m in enumerate(raw_monsters):
        name = m.get("name", "Unknown")
        mtype_raw = m.get("type", "beast").lower()
        if "swarm" in mtype_raw or "beast" in mtype_raw:
            mtype = MONSTER_TYPE_MAP["beast"]
        else:
            mtype = MONSTER_TYPE_MAP.get(mtype_raw, 10)

        size_align = f"{m.get('size', 'Medium')} {mtype_raw}, {m.get('alignment', 'unaligned')}"
        ac_list = m.get("armor_class", [])
        if isinstance(ac_list, list) and len(ac_list) > 0:
            ac_val = int(ac_list[0].get("value", 10))
            ac_type = ac_list[0].get("type", "")
            ac_note = ac_type if ac_type and ac_type != "dex" else ""
        else:
            ac_val = 10
            ac_note = ""

        hp_val = int(m.get("hit_points", 1))
        hit_dice = m.get("hit_points_roll", m.get("hit_dice", ""))

        speed_dict = m.get("speed", {})
        speed_parts = [str(v) if k == "walk" else f"{k} {v}" for k, v in speed_dict.items()]
        speed_str = ", ".join(speed_parts)

        saves, skills = [], []
        for prof in m.get("proficiencies", []):
            pname = prof.get("proficiency", {}).get("name", "")
            pval = prof.get("value", 0)
            sign = "+" if pval >= 0 else ""
            if pname.startswith("Saving Throw: "):
                saves.append(f"{pname.replace('Saving Throw: ', '')} {sign}{pval}")
            elif pname.startswith("Skill: "):
                skills.append(f"{pname.replace('Skill: ', '')} {sign}{pval}")

        def_parts = []
        if m.get("damage_vulnerabilities"):
            def_parts.append("Vuln: " + ", ".join(m["damage_vulnerabilities"]))
        if m.get("damage_resistances"):
            def_parts.append("Res: " + ", ".join(m["damage_resistances"]))
        if m.get("damage_immunities"):
            def_parts.append("Imm: " + ", ".join(m["damage_immunities"]))
        if m.get("condition_immunities"):
            cnames = [c.get("name", "") for c in m["condition_immunities"]]
            def_parts.append("Cond Imm: " + ", ".join(cnames))

        senses_dict = m.get("senses", {})
        sense_parts = [f"{k.replace('_', ' ')} {v}" for k, v in senses_dict.items()]
        lang_str = m.get("languages", "")
        senses_lang = ", ".join(sense_parts)
        if lang_str:
            senses_lang = f"{senses_lang} | Lang: {lang_str}" if senses_lang else f"Lang: {lang_str}"

        str_s = int(m.get("strength", 10))
        dex_s = int(m.get("dexterity", 10))
        con_s = int(m.get("constitution", 10))
        int_s = int(m.get("intelligence", 10))
        wis_s = int(m.get("wisdom", 10))
        cha_s = int(m.get("charisma", 10))
        init_bonus = (dex_s - 10) // 2

        first_action_idx = total_actions
        act_count = 0
        sections = [
            (0, m.get("special_abilities", [])),
            (1, m.get("actions", [])),
            (2, m.get("reactions", [])),
            (3, m.get("legendary_actions", [])),
        ]
        for sec_type, items in sections:
            for item in items:
                if act_count >= 255:
                    break
                aname = item.get("name", "Action")
                adesc = item.get("desc", "")
                asum = compact_action_summary(item)
                ab = item.get("attack_bonus", None)
                atk_b = max(-127, min(127, int(ab))) if ab is not None else -128
                action_bytes.extend(
                    struct.pack("<IIIbBH", pool.add(aname), pool.add(asum), pool.add(adesc), atk_b, sec_type, 0)
                )
                total_actions += 1
                act_count += 1

        cr_idx = cr_to_index(m.get("challenge_rating", 0))
        xp_val = int(m.get("xp", 0))
        tmask = infer_terrain_mask(m, mtype)

        monster_bytes.extend(
            struct.pack(
                "<HBBIIIIIIIIIIHHBBBBBBbBHH",
                idx, mtype, cr_idx,
                pool.add(name), pool.add(size_align), pool.add(ac_note),
                pool.add(hit_dice), pool.add(speed_str), pool.add(", ".join(saves)),
                pool.add(", ".join(skills)), pool.add(" | ".join(def_parts)), pool.add(senses_lang),
                xp_val, ac_val, hp_val,
                str_s, dex_s, con_s, int_s, wis_s, cha_s,
                init_bonus, act_count, first_action_idx, tmask,
            )
        )

        norm = normalize_search_name(name)
        search_entries.append((norm, char_bitmask(norm), idx, CAT_MONSTER, cr_idx, mtype))

    # 2. Conditions & Combat Rules
    rule_id = 0
    raw_conditions.sort(key=lambda c: CONDITION_INDEX_MAP.get(c.get("index", ""), 99))
    for c in raw_conditions:
        cname = c.get("name", "Condition")
        cidx = CONDITION_INDEX_MAP.get(c.get("index", "").lower(), 0xFF)
        body = "\n".join(c.get("desc", []))
        rule_bytes.extend(
            struct.pack("<HBBIII", rule_id, 1, cidx, pool.add(cname), pool.add("Condition"), pool.add(body))
        )
        norm = normalize_search_name(cname)
        search_entries.append((norm, char_bitmask(norm), rule_id, CAT_CONDITION, 0, cidx))
        rule_id += 1

    rules_by_idx = {r.get("index", ""): r for r in raw_rules}
    for rkey in COMBAT_RULE_INDEXES:
        r = rules_by_idx.get(rkey)
        if not r or not r.get("desc"):
            continue
        rname = r.get("name", rkey)
        rbody = r.get("desc", "")
        rule_bytes.extend(
            struct.pack("<HBBIII", rule_id, 0, 0xFF, pool.add(rname), pool.add("Combat Rule"), pool.add(rbody))
        )
        norm = normalize_search_name(rname)
        search_entries.append((norm, char_bitmask(norm), rule_id, CAT_RULE, 0, 0))
        rule_id += 1

    # Add Grappler feat as a Rules entry with SRD note
    for feat in raw_feats:
        fname = feat.get("name", "Feat")
        fbody = "\n".join(feat.get("desc", []))
        fbody += "\n\nNote: Grappler is the only Feat included in the open SRD 5.1."
        rule_bytes.extend(
            struct.pack("<HBBIII", rule_id, 0, 0xFF, pool.add(fname), pool.add("Feat (SRD 5.1)"), pool.add(fbody))
        )
        norm = normalize_search_name(fname)
        search_entries.append((norm, char_bitmask(norm), rule_id, CAT_RULE, 0, 0))
        rule_id += 1

    # Add DM Screen Quick-Reference Cheat Sheet cards
    dm_screen_cards = [
        (
            "Difficulty Classes (DC)",
            "Very Easy: DC 5\nEasy: DC 10\nMedium: DC 15\nHard: DC 20\nVery Hard: DC 25\nNearly Impossible: DC 30\n\nPassive Check = 10 + all modifiers (+5 for Advantage, -5 for Disadvantage).",
        ),
        (
            "Cover & Line of Sight",
            "Half Cover: +2 bonus to AC and Dexterity saving throws (low wall, furniture, creatures).\nThree-Quarters Cover: +5 bonus to AC and Dexterity saving throws (portcullis, arrow slit, thick tree trunk).\nTotal Cover: Cannot be targeted directly by an attack or spell.",
        ),
        (
            "Exhaustion Levels",
            "1: Disadvantage on ability checks\n2: Speed halved\n3: Disadvantage on attack rolls and saving throws\n4: Hit point maximum halved\n5: Speed reduced to 0\n6: Death\n\nFinishing a long rest reduces exhaustion by 1 level (if food and drink are consumed).",
        ),
        (
            "Light & Vision",
            "Bright Light: Normal vision.\nDim Light (Lightly Obscured): Disadvantage on Wisdom (Perception) checks relying on sight.\nDarkness (Heavily Obscured): Effectively Blinded.\n\nLight Sources (Bright / Dim):\n- Candle: 5 ft / +5 ft (1 hr)\n- Torch / Light spell: 20 ft / +20 ft (1 hr)\n- Hooded Lantern: 30 ft / +30 ft (6 hr)\n- Bullseye Lantern: 60 ft cone / +60 ft (6 hr)",
        ),
        (
            "Travel Pace",
            "Fast: 400 ft/min | 4 mph | 30 mi/day (-5 Passive Perception)\nNormal: 300 ft/min | 3 mph | 24 mi/day\nSlow: 200 ft/min | 2 mph | 18 mi/day (Able to use Stealth)\n\nForced March: Beyond 8 hours, DC 10 + 1/hr CON save or gain 1 level of Exhaustion.",
        ),
        (
            "Object AC & HP",
            "Armor Class by Material:\n- Cloth/Paper/Rope: AC 11\n- Crystal/Glass/Ice: AC 13\n- Wood/Bone: AC 15\n- Stone: AC 17\n- Iron/Steel: AC 19\n- Mithral: AC 21 | Adamantine: AC 23\n\nHit Points (Fragile / Resilient):\n- Tiny (bottle, lock): 2 (1d4) / 5 (2d4)\n- Small (chest, lute): 3 (1d6) / 10 (3d6)\n- Medium (barrel, door): 4 (1d8) / 18 (4d8)\n- Large (cart, window): 5 (1d10) / 27 (5d10)",
        ),
        (
            "Improvised Damage",
            "1d10: Burned by coals, hit by falling bookcase, pricked by poison needle\n2d10: Struck by lightning, stumbling into a fire pit\n4d10: Hit by falling rubble in a collapsing tunnel, stumbling into acid\n10d10: Crushed by compacting walls, hit by whirling steel blades, wading through lava\n18d10: Submerged in lava, hit by a crashing flying fortress\n24d10: Tumbling into a vortex of fire or crushed in the jaws of a godlike creature",
        ),
    ]
    for dname, dbody in dm_screen_cards:
        rule_bytes.extend(
            struct.pack("<HBBIII", rule_id, 0, 0xFF, pool.add(dname), pool.add("DM Screen"), pool.add(dbody))
        )
        norm = normalize_search_name(dname)
        search_entries.append((norm, char_bitmask(norm), rule_id, CAT_RULE, 0, 0))
        rule_id += 1

    # 3. Spells (319)
    spell_id_by_name = {}
    for sidx, sp in enumerate(raw_spells):
        sname = sp.get("name", "Spell")
        level = int(sp.get("level", 0))
        flags = (1 if sp.get("ritual") else 0) | (2 if sp.get("concentration") else 0)
        school = sp.get("school", {}).get("name", "Magic")
        ctime = sp.get("casting_time", "1 action")
        srange = sp.get("range", "Self")
        comps = ", ".join(sp.get("components", []))
        if sp.get("material"):
            comps += f" ({sp['material']})"
        dur = sp.get("duration", "Instantaneous")
        classes = ", ".join(c.get("name", "") for c in sp.get("classes", []))
        body = "\n".join(sp.get("desc", []))
        if sp.get("higher_level"):
            body += "\n\nAt Higher Levels: " + "\n".join(sp["higher_level"])

        spell_bytes.extend(
            struct.pack(
                "<HBBIIIIIIII",
                sidx, level, flags,
                pool.add(sname), pool.add(school), pool.add(ctime),
                pool.add(srange), pool.add(comps), pool.add(dur),
                pool.add(classes), pool.add(body),
            )
        )
        spell_id_by_name[sname.lower()] = sidx
        norm = normalize_search_name(sname)
        search_entries.append((norm, char_bitmask(norm), sidx, CAT_SPELL, level, 0))

    # 4. Classes (+ Subclasses), Races, Equipment, Magic Items, and Gap Stubs -> ReferenceRecord
    ref_id = 0
    sub_by_class = {}
    for sub in raw_subclasses:
        cname = sub.get("class", {}).get("name", "")
        sub_by_class.setdefault(cname, []).append(sub)

    for cl in raw_classes:
        cname = cl.get("name", "Class")
        hd = cl.get("hit_die", 8)
        saves = ", ".join(s.get("name", "") for s in cl.get("saving_throws", []))
        profs = ", ".join(p.get("name", "") for p in cl.get("proficiencies", [])[:8])
        subs = sub_by_class.get(cname, [])
        sub_names = ", ".join(s.get("name", "") for s in subs) if subs else "None"

        subtitle = f"Hit Die d{hd} | Saves: {saves}"
        body_parts = [f"Proficiencies: {profs}", f"\nSRD Subclass Included: {sub_names}"]
        for s in subs:
            body_parts.append(f"\n[{s.get('name', '')}] " + "\n".join(s.get("desc", [])))
        body_parts.append("\nNote: Only 1 subclass per class is in the open SRD 5.1.")
        body = "\n".join(body_parts)

        ref_bytes.extend(
            struct.pack("<HBBIIIHH", ref_id, CAT_CLASS, 0, pool.add(cname), pool.add(subtitle), pool.add(body), 0xFFFF, 0)
        )
        norm = normalize_search_name(cname)
        search_entries.append((norm, char_bitmask(norm), ref_id, CAT_CLASS, 0, 0))
        ref_id += 1

    for rc in raw_races:
        rname = rc.get("name", "Race")
        speed = rc.get("speed", 30)
        size = rc.get("size", "Medium")
        bonuses = ", ".join(
            f"{b.get('ability_score', {}).get('name', '')} +{b.get('bonus', 0)}"
            for b in rc.get("ability_bonuses", [])
        )
        traits = ", ".join(t.get("name", "") for t in rc.get("traits", []))
        subtitle = f"{size} | Speed {speed} ft. | {bonuses}"
        body = f"{rc.get('size_description', '')}\n\n{rc.get('language_desc', '')}"
        if traits:
            body += f"\n\nTraits: {traits}"

        ref_bytes.extend(
            struct.pack("<HBBIIIHH", ref_id, CAT_RACE, 0, pool.add(rname), pool.add(subtitle), pool.add(body), 0xFFFF, 0)
        )
        norm = normalize_search_name(rname)
        search_entries.append((norm, char_bitmask(norm), ref_id, CAT_RACE, 0, 0))
        ref_id += 1

    for eq in raw_equipment:
        ename = eq.get("name", "Item")
        cat_name = eq.get("equipment_category", {}).get("name", "Gear")
        cost = eq.get("cost", {})
        cost_str = f"{cost.get('quantity', 0)} {cost.get('unit', 'gp')}" if cost else "-"
        weight = eq.get("weight", 0)

        if "Weapon" in cat_name:
            sub_t = EQ_WEAPON
            dmg = eq.get("damage", {})
            dmg_str = f"{dmg.get('damage_dice', '')} {dmg.get('damage_type', {}).get('name', '')}".strip()
            props = ", ".join(p.get("name", "") for p in eq.get("properties", []))
            subtitle = f"{eq.get('category_range', 'Weapon')} | {dmg_str or 'Special'} | {cost_str}"
            body = f"Cost: {cost_str}\nWeight: {weight} lb.\nDamage: {dmg_str or '-'}\nProperties: {props or 'None'}"
        elif "Armor" in cat_name:
            sub_t = EQ_ARMOR
            ac = eq.get("armor_class", {})
            ac_str = f"AC {ac.get('base', 10)}" + (" + DEX" if ac.get("dex_bonus") else "")
            subtitle = f"{eq.get('armor_category', 'Armor')} Armor | {ac_str} | {cost_str}"
            body = f"Cost: {cost_str}\nWeight: {weight} lb.\nArmor Class: {ac_str}"
        else:
            sub_t = EQ_GEAR
            subtitle = f"{cat_name} | {cost_str} | {weight} lb."
            desc_list = eq.get("desc", [])
            body = "\n".join(desc_list) if desc_list else f"Category: {cat_name}\nCost: {cost_str}\nWeight: {weight} lb."

        ref_bytes.extend(
            struct.pack("<HBBIIIHH", ref_id, CAT_EQUIPMENT, sub_t, pool.add(ename), pool.add(subtitle), pool.add(body), 0xFFFF, 0)
        )
        norm = normalize_search_name(ename)
        search_entries.append((norm, char_bitmask(norm), ref_id, CAT_EQUIPMENT, 0, sub_t))
        ref_id += 1

    for mi in raw_magic_items:
        mname = mi.get("name", "Magic Item")
        rarity = mi.get("rarity", {}).get("name", "Varies")
        cat_name = mi.get("equipment_category", {}).get("name", "Wondrous Item")
        subtitle = f"{rarity} {cat_name}"
        body = "\n".join(mi.get("desc", []))

        ref_bytes.extend(
            struct.pack("<HBBIIIHH", ref_id, CAT_EQUIPMENT, EQ_MAGIC_ITEM, pool.add(mname), pool.add(subtitle), pool.add(body), 0xFFFF, 0)
        )
        norm = normalize_search_name(mname)
        search_entries.append((norm, char_bitmask(norm), ref_id, CAT_EQUIPMENT, 0, EQ_MAGIC_ITEM))
        ref_id += 1

    # 5. Renamed SRD Aliases & Non-SRD Gap Stubs
    for al in gaps_cfg.get("renamed_aliases", []):
        alias_name = al["alias"]
        srd_target = al["srd_name"]
        target_id = spell_id_by_name.get(srd_target.lower(), 0xFFFF)
        subtitle = f"SRD Open Name: {srd_target}"
        body = f"In the open SRD 5.1, '{alias_name}' is published under the generic name '{srd_target}'."
        ref_bytes.extend(
            struct.pack("<HBBIIIHH", ref_id, CAT_SRD_GAP, GAP_ALIAS, pool.add(alias_name), pool.add(subtitle), pool.add(body), target_id, 0)
        )
        norm = normalize_search_name(alias_name)
        search_entries.append((norm, char_bitmask(norm), ref_id, CAT_SRD_GAP, 0, GAP_ALIAS))
        ref_id += 1

    for gp in gaps_cfg.get("non_srd_gaps", []):
        gname = gp["name"]
        subtitle = f"[Non-SRD] {gp['kind']}"
        body = (
            f"Not Included in Open SRD 5.1\n\n{gp['note']}\n\n"
            "Tip: You can add custom entries on your SD card under sdmc:/3ds/DM_Toolkit/homebrew/."
        )
        ref_bytes.extend(
            struct.pack("<HBBIIIHH", ref_id, CAT_SRD_GAP, GAP_NON_SRD, pool.add(gname), pool.add(subtitle), pool.add(body), 0xFFFF, 0)
        )
        norm = normalize_search_name(gname)
        search_entries.append((norm, char_bitmask(norm), ref_id, CAT_SRD_GAP, 0, GAP_NON_SRD))
        ref_id += 1

    search_bytes = bytearray()
    for norm, mask, rec_id, cat, cr_lvl, sub in search_entries:
        nb = norm.encode("ascii", errors="ignore")[:31]
        nb_padded = nb + b"\x00" * (32 - len(nb))
        search_bytes.extend(
            struct.pack("<32sIHBBB3s", nb_padded, mask, rec_id, cat, cr_lvl, sub, b"\x00\x00\x00")
        )

    header_size = 64
    monsters_offset = header_size
    actions_offset = monsters_offset + len(monster_bytes)
    rules_offset = actions_offset + len(action_bytes)
    spells_offset = rules_offset + len(rule_bytes)
    refs_offset = spells_offset + len(spell_bytes)
    search_offset = refs_offset + len(ref_bytes)
    string_offset = search_offset + len(search_bytes)

    header = struct.pack(
        "<4sIIIIIIIIIIIIIII",
        b"DMTK",
        SRD_BIN_VERSION,
        len(raw_monsters),
        total_actions,
        rule_id,
        len(raw_spells),
        ref_id,
        len(search_entries),
        monsters_offset,
        actions_offset,
        rules_offset,
        spells_offset,
        refs_offset,
        search_offset,
        string_offset,
        len(pool.buf),
    )

    os.makedirs(os.path.dirname(OUT_BIN), exist_ok=True)
    with open(OUT_BIN, "wb") as out:
        out.write(header)
        out.write(monster_bytes)
        out.write(action_bytes)
        out.write(rule_bytes)
        out.write(spell_bytes)
        out.write(ref_bytes)
        out.write(search_bytes)
        out.write(pool.buf)

    total_kb = os.path.getsize(OUT_BIN) / 1024.0
    print(
        f"Compiled {OUT_BIN}: {len(raw_monsters)} monsters, {len(raw_spells)} spells, "
        f"{ref_id} refs/gaps, {rule_id} rules, {len(search_entries)} search entries ({total_kb:.1f} KB)"
    )


if __name__ == "__main__":
    compile_srd()
