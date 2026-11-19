# DM's Toolkit, Asset Plan

Date: November 19, 2026
Status: Complete for v1.1. All packaging, title screen, and section icon assets are generated and wired into the `tex3ds` sprite atlas pipeline (`gfx/sprites.t3s` -> `romfs/gfx/sprites.t3x`).

## How assets work in this app

- Image assets live in `gfx/` as PNGs and are compiled into `romfs/gfx/sprites.t3x` at build time via `tex3ds` (`gfx/sprites.t3s`).
- The app is text-heavy: the monster manual, the fight tracker, and the reference are text. Art frames the experience without slowing down table lookups.
- The 3D dice roller draws live polyhedral dice and customizable tray skins in code (`source/dice_view.cpp`), supporting 5 Dice Skins and 5 Dice Tray Skins with zero extra ROM bloat.

## App packaging

| Asset | Spec | Purpose | Status |
| :--- | :--- | :--- | :--- |
| `icon.png` | `48x48` PNG | HOME Menu icon & SMDH (gold d20 on emerald felt & walnut crest) | Done |
| `gfx/banner.png` | `256x128` PNG | CIA HOME Menu banner (candlelit tavern DM desk, bestiary, gold d20 tray) | Done |
| `gfx/banner.wav` | 1.0s PCM WAV | CIA HOME Menu chime | Done |

## Title screen & UI sprites (`gfx/sprites.t3s`)

| Asset | Spec | Purpose | Status |
| :--- | :--- | :--- | :--- |
| `gfx/bg/title_screen.png` | `400x240` PNG | Top-screen tavern tabletop title splash | Done |
| `gfx/icons/icon_fight.png` | `20x20` RGBA PNG | Crossed swords tab icon (Fight Tracker) | Done |
| `gfx/icons/icon_manual.png` | `20x20` RGBA PNG | Leather bestiary tome tab icon (Monster Manual) | Done |
| `gfx/icons/icon_dice.png` | `20x20` RGBA PNG | Gold polyhedral d20 tab icon (Dice Tray) | Done |
| `gfx/icons/icon_rules.png` | `20x20` RGBA PNG | Parchment scroll & quill tab icon (Rules Reference) | Done |
| `gfx/icons/icon_tools.png` | `20x20` RGBA PNG | Brass tavern lantern tab icon (Tools & Campaigns) | Done |

## Out of scope for v1

- Individual monster portrait art (the manual is fast text stat blocks).
- Player-side art (there is no player side in the app).
- Custom homebrew art.
