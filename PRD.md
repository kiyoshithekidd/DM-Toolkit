# DM's Toolkit, PRD

Date: November 18, 2026
Status: Draft. Name chosen: DM's Toolkit.

## What it is

A DM-only companion app for the Nintendo 3DS, built for homebrew tabletop campaigns run at the table. It puts the whole DM screen in one place: the monster manual, the fight tracker, the dice, and the rules reference. It is not a player app. There is no player side, no character sheets for players, no multiplayer.

## Who it is for

The DM. Used at the table during homebrew campaigns.

## Platform

Nintendo 3DS homebrew, on a modded 3DS XL. Ships as .3dsx first, with a .cia build later so it can launch from the home menu without the homebrew launcher (same packaging path as the Tomodachi Life Save Editor). Built from the same source either way.

## The core loop: the fight

Monsters are the heart of the app, and the fight is the heart of the app. The loop is:

1. Build an encounter or start one on the fly.
2. Roll initiative for everyone in it, monsters and the saved party.
3. Track the fight round by round: HP per monster, monster abilities, and the combat rules a tap away.
4. End the fight, clear the tracker, keep the campaign.

## Features, v1

### Monster manual

- Searchable manual of SRD monsters, searchable by name and filterable by type and challenge rating.
- Each entry shows the full stat block: HP, AC, speed, ability scores, saves, skills, resistances, and actions.
- Monster abilities are visible from the tracker, not just the manual, so the DM does not have to leave the fight to check what a monster can do.

### Encounter builder and random generator

- Preplanned encounters: pick monsters from the manual, set how many of each, save the encounter to the campaign before the session.
- On-the-fly adds: an add button mid-fight drops a monster straight into the current initiative order.
- Random encounter generator: rolls an encounter by party level and terrain.

### Initiative and HP tracker

- Rolls initiative for every combatant and shows the order.
- HP tracking per monster, with damage and healing applied by tap.
- The saved party is in the initiative order alongside the monsters.
- Combat rules one tap away from the tracker (actions, bonus actions, conditions, opportunity attacks).

### Dice roller

- Physics roll feel, modeled on the College of Keys roller spec: D&D Beyond style physics roll, die drawn live, gold die on dark green felt.
- All standard dice: d4, d6, d8, d10, d12, d20, d100, with modifiers and advantage/disadvantage.

### Generators

- NPC name generator for on-the-spot characters.
- Loot generator for rewards after a fight.

### Rules reference (pocket SRD)

- Lookups for spells, races, class abilities, weapons, armor, gear, and conditions, plus combat actions.
- All reference content comes from the SRD 5.1, which is free to use under CC-BY-4.0 with attribution.
- Gaps to flag, not hide: a few subclasses, a handful of spells, and most feats are not in the open set. The app flags those rather than pretending they are covered. Homebrew entries are a later addition, not v1.

## Campaign saves

- The party is saved once per campaign: names and initiative bonuses.
- Multiple campaign save files. Each campaign keeps its own party and its own preplanned encounters.
- Switching campaigns switches the party and encounters with it.
- Campaigns are named when created.

## UI and 3DS notes

- Built for the 3DS screens: top screen 400x240 for the fight and the manual entry, bottom touchscreen 320x240 for navigation, search, and quick actions.
- Search is the primary way in: type a few letters, get the monster, spell, or rule. Browsing by category is the backup.
- Text stat blocks are tiny, so the full SRD reference fits on the device without storage worry. The hard part is making search fast on the small touchscreen, not fitting the data.
- Keep it usable mid-game with one hand on the 3DS and the other on notes or dice. Big tap targets, no fiddly menus in the tracker.

## Content and IP rules

- Generic fantasy terms only. Never use "Dungeons & Dragons", "D&D", the ampersand logo, "Dungeon Master" spelled out, "d20 System", Forgotten Realms, beholder, mind flayer, or named settings and characters.
- "DM" as an abbreviation is fine, it is generic in the tabletop community, but the full "Dungeon Master" term stays out of the app and any listing.
- SRD 5.1 content is used under CC-BY-4.0 with attribution. All other content is original.

## Out of scope for v1

- Player side: no character sheets, no player dice screens.
- Multiplayer or versus modes.
- Full Player's Handbook content beyond the SRD.
- Homebrew entry editor (later).
- Conditions tracking on monsters (open decision, see below).

## Open decisions

- Conditions tracking: should the tracker mark conditions on monsters (stunned, poisoned, prone) or just HP for v1?
- Reference data: baked into the app for v1, or loaded from editable files so the DM can add homebrew entries?
- Dice physics: reuse the College of Keys physics roller approach, or a simpler roll animation for v1?

## Next steps

1. Name confirmed: DM's Toolkit. v1 scope as above.
2. Repo created: github.com/kiyoshithekidd/DM-Toolkit. Next: a starter prompt for the build.
3. First build slice: monster manual plus the fight tracker (initiative, HP, abilities), since monsters are the priority.
4. Second slice: dice roller, generators, and the rules reference.
