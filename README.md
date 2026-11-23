# DM-Toolkit

A DM-only companion app for the Nintendo 3DS (modded 3DS XL). It puts the whole DM screen in one place: the monster manual, the fight tracker, the physics-feel dice tray, the tabletop generators, and the pocket SRD rules reference.

## Features

* **Fight Tracker (Home Screen):**
  * Dual-column top screen: live Initiative Ladder (round counter, HP bars for both **Party Heroes and Monsters**, `HEALTHY / BLOODIED / DOWN / STABILIZED` status badges, `CNC` concentration badge, and condition badges) on the left and the selected monster's full stat block or hero's health / death-save card on the right.
  * One-handed bottom touchscreen controls: high-contrast **DAMAGE** (`-10 / -5 / -1`) and **HEAL** (`+1 / +5`) controls, **Smart Action & Damage Roller** (`<` / `>` to cycle monster actions + 1-tap `Hit` and `Dmg` dice rolls), **`[Conc]`** toggle with automatic `DC = max(10, dmg / 2)` concentration prompts, **Hero Death Save** tracker (`+Succ Save`, `+Fail Save`, `Stabilize`), **`[Skip:ON/OFF]`** auto-skip for dead monsters, **1-tap `Undo`**, **Custom HP Numpad**, 15-condition toggle grid, `In- / In+` initiative overrides, `[Rm]` combatant removal, mid-fight `[+ Monster]` additions, 1-tap **Rules** drawer, paged **Saved Encounters**, automatic mid-combat SD auto-save, post-fight party HP sync, and `[Roll Loot]` on end-fight.
* **Monsters & Encounter Prep Builder (`Monsters` Tab):**
  * All **334 SRD 5.1 monsters** with full stat blocks, traits, actions, reactions, and legendary actions.
  * Instant always-live bottom-screen QWERTY search (~20-30 us) plus **Pinned (`Pin` / `*`)** favorites, **Monster Type**, and **CR** filter pills, with 1-tap `[+Fgt]` (drop straight into live combat) and `[+Prep]` (add to Encounter Builder draft).
  * **In-App Encounter Builder** (`Tools -> Prep Builder`): adjust monster quantities, view **Live SRD Encounter Difficulty** (`[Easy]`, `[Medium]`, `[Hard]`, `[Deadly]` with adjusted XP & party-size multipliers), save named encounters directly into your campaign, or launch the draft immediately.
* **Hybrid Multi-Dice Tray with Custom Skins, Shake & Flick:**
  * Roll `1` to `12` dice at once (`NdX + Mod`, e.g. `2d6+4`, `8d6`) with `- / +` count steppers, quick presets (`1d`, `2d`, `4d`, `6d`, `8d`), modifiers (`-10` to `+20`), dual-die **Advantage / Disadvantage**, and roll history with per-die breakdowns.
  * **3DS Accelerometer Shake-to-Roll & Stylus Flick:** Physically shake your 3DS or flick the stylus across the bottom screen in `Dice` mode to launch dice with directional impulse.
  * **Shaded Polyhedral Dice:** Multi-faceted `d4`, `d6`, `d8`, `d10`, `d12`, `d20`, and `d100` rendering with skin-matched facet shading, bevel outlines, engraved numerals/pips, and smooth bounce/spin physics.
  * **8 Dice Skins:** *Royal Gold* (default), *Obsidian Gold*, *Crimson Ruby*, *Arcane Sapphire*, *Ivory Bone*, *Emerald Jade*, *Frost Silver*, and *Shadow Amethyst*.
  * **8 Dice Tray Skins:** *Emerald Felt* (default), *Crimson Velvet*, *Royal Navy*, *Tavern Leather*, *Amethyst Slate*, *Obsidian Marble*, *Sunlit Oak*, and *Abyssal Teal*.
* **Full On-Device Party Roster Editor (`Tools -> Party`):**
  * Drop player character JSON files into `sdmc:/3ds/DM_Toolkit/party/*.json` and tap **`[Import]`**, or edit heroes directly on-device: `+ Hero`, `Rename`, `Class`, `AC-/+`, `In-/+`, `HP-/+`, `Lv-/+`, `Active/Bnc` (bench absent players), and `Rm`.
* **Pocket SRD Rules Reference:**
  * Searchable across **319 Spells**, **12 Classes (+ 12 SRD Subclasses)**, **9 Races**, **237 Weapons/Armor/Gear**, **362 Magic Items**, **15 Conditions**, **33 Combat Rules**, and **Grappler**, with **Pinned (`Pin` / `*`)** favorites.
  * Transparently resolves renamed SRD spell aliases and flags `[Non-SRD]` gaps instead of hiding them.
* **Tabletop Generators:**
  * **Random Encounter Generator** tuned to active campaign party level, party size, 8 terrains, and SRD XP difficulty thresholds, with 1-tap `[Send to Fight Tracker]`.
  * **NPC Generator** (name, ancestry, occupation, roleplay quirk) and **Loot Generator** (individual pocket coins or full treasure hoards with gems and SRD magic items).
* **Multi-Campaign Saves, Persistent Settings & On-Device Homebrew Creator:**
  * One atomic JSON save file per campaign in `sdmc:/3ds/DM_Toolkit/campaigns/` (party roster, preplanned encounters, and auto-saved active fight), with `< / >` paging in `Tools -> Saves` and automatic slug deduplication.
  * Automatic user preference persistence (`sdmc:/3ds/DM_Toolkit/settings.json`) remembering your chosen Dice Skin, Tray Skin, Sound setting, `Auto-Skip Dead`, `Pinned` favorites, and last opened campaign.
  * **On-Device Homebrew Creator** (`Tools -> Brew`): author, **`[Load]`** (edit existing), or **`[Del]`** custom monsters and spells directly on the 3DS via `swkbd`, saved atomically to `sdmc:/3ds/DM_Toolkit/homebrew/on_device_homebrew.json` and hot-reloaded straight into the search index.
  * Optional SD card overlay in `sdmc:/3ds/DM_Toolkit/homebrew/*.json` to import external custom monsters and spells (automatically superseding matching `[Non-SRD]` stubs).
* **Tabletop Audio Engine (`ndsp`):**
  * Procedural synthesized sound effects for dice clatter, Nat 20 chime, Nat 1 thud, and subtle UI taps using `libctru` `ndsp` audio.
  * Gracefully falls back to silent mode if `sdmc:/3ds/dspfirm.cdc` is not dumped, with a persistent `[Sound: ON / OFF]` toggle in `Tools -> Saves`.
* **Custom Tavern Art, 3D HOME Menu Banner & `tex3ds` Sprite Atlas:**
  * Custom candlelit tavern **Title Screen** (`400x240`), **5 golden fantasy tab icons** (`Fight`, `Monsters`, `Dice`, `Rules`, `Tools`), custom `48x48` HOME Menu icon, and a **rotating 3D golden d20 `.cgfx` CIA banner** (`gfx/banner.cgfx`) with stationary `DM-TOOLKIT` plaque.

## Controls (Nintendo 3DS)

* **Title Screen:** Tap anywhere or press `A` to jump straight into the Fight Tracker.
* **Touchscreen:** Primary one-handed navigation, live search keypad, HP adjustments, action/damage rolls, condition & concentration toggles, dice rolls & stylus flick, skin cycling, and on-device party/homebrew authoring.
* **D-Pad Up / Down:** Select combatant in Fight Tracker, scroll search results, or select party hero.
* **Circle Pad / C-Stick Up / Down (or L / R in Monsters & Rules):** Scroll long stat blocks, spells, and rule descriptions.
* **A / R (Fight Tracker):** Advance to the next turn (or cycle rules when the Combat Rules drawer is open).
* **A / X / Shake / Flick (Dice Tray):** Roll the selected die. **L / R:** Cycle Dice Skin and Tray Skin.
* **B:** Close Combat Rules drawer or condition overlay.
* **START:** Save active campaign/fight and exit cleanly.

## Building from Source

* **Host Unit Tests & SRD Compiler (Linux/macOS/WSL):**
  ```bash
  make test
  ```
* **Nintendo 3DS `.3dsx` and `.cia` (requires `devkitPro` / `devkitARM`, `tex3ds`, `bannertool`, `makerom`):**
  ```bash
  make
  make cia
  ```
  Prebuilt `DM_Toolkit.3dsx` and `DM_Toolkit.cia` artifacts are also built automatically on every push and pull request via GitHub Actions.

## Documentation & Project Structure

* [PRD.md](PRD.md): Full product specification and v1 scope.
* [PERSONAS.md](PERSONAS.md): User personas (Kitchen Table DM first).
* [ARCHITECTURE.md](ARCHITECTURE.md): Hardware toolchain, SRD binary layout, UI architecture, and playtest checklists.
* [ASSETS.md](ASSETS.md): Visual asset manifest and `tex3ds` sprite pipeline.
* `gfx/sprites.t3s`: Sprite atlas manifest compiled into `romfs/gfx/sprites.t3x`.
* `tools/compile_srd.py`: Offline SRD 5.1 binary compiler (`romfs/data/srd.bin`) with automated IP sanitization.
* `source/` & `include/`: C++17 core engine (`srd_db`, `combat_tracker`, `campaign_io`, `generators`, `dice_roller`, `sound_fx`) and `citro2d` 3DS UI (`main`, `dice_view`).

## Changelog

* **v1.5.0:**
  * **Smart Monster Action & Damage Roller (#28):** Cycle monster actions (`<` / `>`) and roll `Hit` (`1d20 + atk`) or parsed `Dmg` (`NdX + mod`) straight into the Dice Tray.
  * **Combat Tracker QoL (#29):** Added `Conc` toggle with automatic `DC = max(10, dmg / 2)` concentration prompts, Hero Death Save tracker (`+Succ`, `+Fail`, `Stabilize`), `Skip Dead` auto-skip toggle, and removed overlapping target arrows.
  * **3DS Accelerometer Shake & Stylus Flick (#30):** Shake the console or flick the stylus in the Dice Tray to roll dice with directional physics impulse.
  * **Live SRD Encounter Difficulty Calculator (#31):** `Tools -> Prep Builder` now displays color-coded `[Easy] / [Medium] / [Hard] / [Deadly]` ratings with adjusted XP.
  * **Pinned Session Favorites (#32):** Pin/unpin Monsters, Spells, and Rules (`*` / `+`) and filter by `Pin` (persisted in `settings.json`).
  * **Full Party & Homebrew Editor (#33):** Edit hero `Class`, `AC`, `Lv`, `Active/Bench`, or `Rm` in `Tools -> Party`, and `Load` / `Del` existing on-device entries in `Tools -> Brew`.
  * **Polyhedral Dice Facet Shading (#36):** Procedural multi-faceted `citro2d` dice (`d4`–`d100`) with skin shading, bevel outlines, and engraved numerals/pips.
  * Renamed `Manual` tab to `Monsters`.
* **v1.4.4:**
  * Raised all sub-`0.38f` UI text scales across top and bottom screens (`0.38f–0.40f` minimum floor) to eliminate pixelated/illegible text on 240p 3DS screens.
  * Centered and brightened the bottom-screen `DAMAGE` and `HEAL` labels for high-contrast readability against dark walnut.
  * Adjusted compact stat-block and condition-badge sizing so larger text fits cleanly without clipping.
* **v1.4.3:**
  * Fixed code-review bugs (#21–#27) and polished documentation.

## Content and IP Attribution

This work includes material taken from the System Reference Document 5.1 ("SRD 5.1") by Wizards of the Coast LLC and available at https://dnd.wizards.com/resources/systems-reference-document. The SRD 5.1 is licensed under the Creative Commons Attribution 4.0 International License available at https://creativecommons.org/licenses/by/4.0/legalcode.

All other content and code in this repository is original. Generic fantasy terms only.
