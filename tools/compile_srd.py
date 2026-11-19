#!/usr/bin/env python3
"""
Offline SRD 5.1 Binary Compiler for DM's Toolkit.
Ingests pinned 2014 SRD 5.1 JSON files from tools/raw_srd/ and compiles
a fast, zero-allocation binary database at romfs/data/srd.bin.
"""

import json
import os
import re
import struct
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(SCRIPT_DIR)
RAW_DIR = os.path.join(SCRIPT_DIR, "raw_srd")
OUT_BIN = os.path.join(REPO_ROOT, "romfs", "data", "srd.bin")

SRD_BIN_VERSION = 1

CAT_MONSTER = 0
CAT_RULE = 2
CAT_CONDITION = 3

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
    # Replace unicode dashes and smart quotes with clean ASCII
    text = text.replace("\u2014", " - ").replace("\u2013", "-")
    text = text.replace("\u2018", "'").replace("\u2019", "'")
    text = text.replace("\u201c", '"').replace("\u201d", '"')
    text = text.replace("\u00d7", "x")
    # Normalize any spelled-out game master title or protected example creature
    text = re.sub(r"\b" + "Dungeon " + r"Master's\b", "DM's", text, flags=re.IGNORECASE)
    text = re.sub(r"\b" + "Dungeon " + r"Masters\b", "DMs", text, flags=re.IGNORECASE)
    text = re.sub(r"\b" + "Dungeon " + r"Master\b", "DM", text, flags=re.IGNORECASE)
    text = re.sub(r"\b" + "mind " + r"flayer's\b", "psychic horror's", text, flags=re.IGNORECASE)
    text = re.sub(r"\b" + "mind " + r"flayer\b", "psychic horror", text, flags=re.IGNORECASE)
    # Collapse excess whitespace while keeping newlines
    lines = [re.sub(r"[ \t]+", " ", ln).strip() for ln in text.splitlines()]
    text = "\n".join(lines).strip()
    # Strip markdown heading hashes and bold/italic asterisks for clean 3DS text display
    text = re.sub(r"^#{1,6}\s*", "", text, flags=re.MULTILINE)
    text = text.replace("***", "").replace("**", "").replace("__", "")
    # Verify against strict IP blocklist
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
    # Otherwise take first sentence up to 72 chars
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

    # Sort monsters alphabetically by name for clean browsing
    raw_monsters.sort(key=lambda m: m.get("name", "").lower())

    pool = StringPool()
    monster_bytes = bytearray()
    action_bytes = bytearray()
    rule_bytes = bytearray()
    search_entries = []

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
        speed_parts = []
        for k, v in speed_dict.items():
            if k == "walk":
                speed_parts.append(str(v))
            else:
                speed_parts.append(f"{k} {v}")
        speed_str = ", ".join(speed_parts)

        saves = []
        skills = []
        for prof in m.get("proficiencies", []):
            pname = prof.get("proficiency", {}).get("name", "")
            pval = prof.get("value", 0)
            sign = "+" if pval >= 0 else ""
            if pname.startswith("Saving Throw: "):
                saves.append(f"{pname.replace('Saving Throw: ', '')} {sign}{pval}")
            elif pname.startswith("Skill: "):
                skills.append(f"{pname.replace('Skill: ', '')} {sign}{pval}")
        saves_str = ", ".join(saves)
        skills_str = ", ".join(skills)

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
        defenses_str = " | ".join(def_parts)

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
                    struct.pack(
                        "<IIIbBH",
                        pool.add(aname),
                        pool.add(asum),
                        pool.add(adesc),
                        atk_b,
                        sec_type,
                        0,
                    )
                )
                total_actions += 1
                act_count += 1

        cr_idx = cr_to_index(m.get("challenge_rating", 0))
        xp_val = int(m.get("xp", 0))

        monster_bytes.extend(
            struct.pack(
                "<HBBIIIIIIIIIIHHBBBBBBbBHH",
                idx,
                mtype,
                cr_idx,
                pool.add(name),
                pool.add(size_align),
                pool.add(ac_note),
                pool.add(hit_dice),
                pool.add(speed_str),
                pool.add(saves_str),
                pool.add(skills_str),
                pool.add(defenses_str),
                pool.add(senses_lang),
                xp_val,
                ac_val,
                hp_val,
                str_s,
                dex_s,
                con_s,
                int_s,
                wis_s,
                cha_s,
                init_bonus,
                act_count,
                first_action_idx,
                0,  # terrainMask (populated in Slice 2)
            )
        )

        norm = normalize_search_name(name)
        search_entries.append((norm, char_bitmask(norm), idx, CAT_MONSTER, cr_idx, mtype))

    # Compile Conditions (0..14) + Combat Rules
    rule_id = 0
    # Sort conditions to match ConditionBit order
    raw_conditions.sort(key=lambda c: CONDITION_INDEX_MAP.get(c.get("index", ""), 99))
    for c in raw_conditions:
        cname = c.get("name", "Condition")
        cidx = CONDITION_INDEX_MAP.get(c.get("index", "").lower(), 0xFF)
        body = "\n".join(c.get("desc", []))
        rule_bytes.extend(
            struct.pack(
                "<HBBIII",
                rule_id,
                1,
                cidx,
                pool.add(cname),
                pool.add("Condition"),
                pool.add(body),
            )
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
            struct.pack(
                "<HBBIII",
                rule_id,
                0,
                0xFF,
                pool.add(rname),
                pool.add("Combat Rule"),
                pool.add(rbody),
            )
        )
        norm = normalize_search_name(rname)
        search_entries.append((norm, char_bitmask(norm), rule_id, CAT_RULE, 0, 0))
        rule_id += 1

    search_bytes = bytearray()
    for norm, mask, rec_id, cat, cr_lvl, sub in search_entries:
        nb = norm.encode("ascii", errors="ignore")[:31]
        nb_padded = nb + b"\x00" * (32 - len(nb))
        search_bytes.extend(
            struct.pack("<32sIHBBB3s", nb_padded, mask, rec_id, cat, cr_lvl, sub, b"\x00\x00\x00")
        )

    header_size = 48
    monsters_offset = header_size
    actions_offset = monsters_offset + len(monster_bytes)
    rules_offset = actions_offset + len(action_bytes)
    search_offset = rules_offset + len(rule_bytes)
    string_offset = search_offset + len(search_bytes)

    header = struct.pack(
        "<4sIIIIIIIIIII",
        b"DMTK",
        SRD_BIN_VERSION,
        len(raw_monsters),
        total_actions,
        rule_id,
        len(search_entries),
        monsters_offset,
        actions_offset,
        rules_offset,
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
        out.write(search_bytes)
        out.write(pool.buf)

    total_kb = os.path.getsize(OUT_BIN) / 1024.0
    print(
        f"Compiled {OUT_BIN}: {len(raw_monsters)} monsters, {total_actions} actions, "
        f"{rule_id} rules/conditions, {len(search_entries)} search entries ({total_kb:.1f} KB)"
    )


if __name__ == "__main__":
    compile_srd()
