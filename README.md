# DM-Toolkit

A DM-only companion app for the Nintendo 3DS (modded 3DS XL). It puts the whole DM screen in one place: the monster manual, the fight tracker, the physics-feel dice tray, the tabletop generators, and the pocket SRD rules reference.

## Features

* **Fight Tracker (Home Screen):**
  * Dual-column top screen: live Initiative Ladder (round counter, HP bars for both **Party Heroes and Monsters**, `HEALTHY / BLOODIED / DOWN` status badges, and condition badges) on the left and the selected monster's full stat block or hero's health card on the right.
  * One-handed bottom touchscreen controls: high-contrast, legible **DAMAGE** (`-10 / -5 / -1`) and **HEAL** (`+1 / +5`) labels and buttons, `< / >` **Target Cycling**, **1-tap `Undo`** for accidental HP taps, **Custom HP Numpad**, **1-Tap `[Roll Atk]`** straight into the Dice Tray, 15-condition toggle grid, `Init- / Init+` overrides, `[Rm]` combatant removal, mid-fight `[+ Monster]` additions, 1-tap **Combat Rules** drawer, paged **Saved Encounters**, automatic mid-combat SD auto-save, post-fight party HP sync, and `[Roll Loot]` on end-fight.
* **Monster Manual & Encounter Prep Builder:**
  * All **334 SRD 5.1 monsters** with full stat blocks, traits, actions, reactions, and legendary actions.
  * Instant always-live bottom-screen QWERTY search (~20-30 us) plus **Monster Type** and **CR** filter pills, with 1-tap `[+Fgt]` (drop straight into live combat) and `[+Prep]` (add to Encounter Builder draft).
  * **In-App Encounter Builder** (`Tools -> Prep Builder`): adjust monster quantities, inspect total base XP, save named encounters directly into your campaign, or launch the draft immediately.
* **Hybrid Multi-Dice Tray with Custom Skins:**
  * Roll `1` to `12` dice at once (`NdX + Mod`, e.g. `2d6+4`, `8d6`) with `- / +` count steppers, quick presets (`1d`, `2d`, `4d`, `6d`, `8d`), modifiers (`-10` to `+20`), dual-die **Advantage / Disadvantage**, and roll history with per-die breakdowns.
  * **Sprite-Based Polyhedral Dice:** Crisp, studio-shaded `d4`, `d6`, `d8`, `d10`, `d12`, `d20`, and `d100` sprites compiled into `romfs:/gfx/sprites.t3x` (`tex3ds`) with real-time skin tinting (`C2D_ImageTint`), smooth bounce/spin physics, and classic `d6` pips.
  * **8 Dice Skins:** *Royal Gold* (default), *Obsidian Gold*, *Crimson Ruby*, *Arcane Sapphire*, *Ivory Bone*, *Emerald Jade*, *Frost Silver*, and *Shadow Amethyst*.
  * **8 Dice Tray Skins:** *Emerald Felt* (default), *Crimson Velvet*, *Royal Navy*, *Tavern Leather*, *Amethyst Slate*, *Obsidian Marble*, *Sunlit Oak*, and *Abyssal Teal*.
* **Player Character Import & Party HP Tracking (`Tools -> Party`):**
  * Drop player character JSON files into `sdmc:/3ds/DM_Toolkit/party/*.json` and tap **`[Import SD]`** to load players' names, classes, levels, AC, and Max HP directly into your campaign and Fight Tracker.
* **Pocket SRD Rules Reference:**
  * Searchable across **319 Spells**, **12 Classes (+ 12 SRD Subclasses)**, **9 Races**, **237 Weapons/Armor/Gear**, **362 Magic Items**, **15 Conditions**, **33 Combat Rules**, and **Grappler**.
  * Transparently resolves renamed SRD spell aliases and flags `[Non-SRD]` gaps instead of hiding them.
* **Tabletop Generators:**
  * **Random Encounter Generator** tuned to active campaign party level, party size, 8 terrains, and SRD XP difficulty thresholds, with 1-tap `[Send to Fight Tracker]`.
  * **NPC Generator** (name, ancestry, occupation, roleplay quirk) and **Loot Generator** (individual pocket coins or full treasure hoards with gems and SRD magic items).
* **Multi-Campaign Saves, Persistent Settings & On-Device Homebrew Creator:**
  * One atomic JSON save file per campaign in `sdmc:/3ds/DM_Toolkit/campaigns/` (party roster, preplanned encounters, and auto-saved active fight), with `< / >` paging in `Tools -> Saves` and automatic slug deduplication.
  * Automatic user preference persistence (`sdmc:/3ds/DM_Toolkit/settings.json`) remembering your chosen Dice Skin, Tray Skin, Sound setting, and last opened campaign.
  * **On-Device Homebrew Creator** (`Tools -> Brew`): author custom monsters (name, AC, HP, CR, action name, and action description with auto-parsed `+X to hit`) and custom spells (name, level, description) directly on the 3DS via the system keyboard (`swkbd`), saved atomically to `sdmc:/3ds/DM_Toolkit/homebrew/on_device_homebrew.json` (updating matching names in place) and hot-reloaded straight into the search index.
  * Optional SD card overlay in `sdmc:/3ds/DM_Toolkit/homebrew/*.json` to import external custom monsters and spells (automatically superseding matching `[Non-SRD]` stubs).
* **Tabletop Audio Engine (`ndsp`):**
  * Procedural synthesized sound effects for dice clatter, Nat 20 chime, Nat 1 thud, and subtle UI taps using `libctru` `ndsp` audio.
  * Gracefully falls back to silent mode if `sdmc:/3ds/dspfirm.cdc` is not dumped, with a persistent `[Sound: ON / OFF]` toggle in `Tools -> Saves`.
* **Custom Tavern Art, 3D HOME Menu Banner & `tex3ds` Sprite Atlas:**
  * Custom candlelit tavern **Title Screen** (`400x240`), **5 golden fantasy tab icons** (`Fight`, `Manual`, `Dice`, `Rules`, `Tools`), and **studio-shaded polyhedral dice sprites** compiled into `romfs:/gfx/sprites.t3x`, plus a custom `48x48` HOME Menu icon and a **rotating 3D golden d20 `.cgfx` CIA banner** (`gfx/banner.cgfx`) with stationary `DM-TOOLKIT` plaque.

## Controls (Nintendo 3DS)

* **Title Screen:** Tap anywhere or press `A` to jump straight into the Fight Tracker.
* **Touchscreen:** Primary one-handed navigation, live search keypad, HP adjustments, condition toggles, dice rolls, skin cycling, and on-device homebrew authoring.
* **D-Pad Up / Down:** Select combatant in Fight Tracker, scroll search results, or select party hero.
* **Circle Pad / C-Stick Up / Down (or L / R in Manual & Rules):** Scroll long stat blocks, spells, and rule descriptions.
* **A / R (Fight Tracker):** Advance to the next turn (or cycle rules when the Combat Rules drawer is open).
* **A / X (Dice Tray):** Roll the selected die. **L / R:** Cycle Dice Skin and Tray Skin.
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

* **v1.4.4:**
  * Raised all sub-`0.38f` UI text scales across top and bottom screens (`0.38f–0.40f` minimum floor) to eliminate pixelated/illegible text on 240p 3DS screens.
  * Centered and brightened the bottom-screen `DAMAGE` and `HEAL` labels for high-contrast readability against dark walnut.
  * Adjusted compact stat-block and condition-badge sizing so larger text fits cleanly without clipping.
* **v1.4.3:**
  * Fixed code-review bugs (#21–#27) and polished documentation.

## Content and IP Attribution

This work includes material taken from the System Reference Document 5.1 ("SRD 5.1") by Wizards of the Coast LLC and available at https://dnd.wizards.com/resources/systems-reference-document. The SRD 5.1 is licensed under the Creative Commons Attribution 4.0 International License available at https://creativecommons.org/licenses/by/4.0/legalcode.

All other content and code in this repository is original. Generic fantasy terms only.
