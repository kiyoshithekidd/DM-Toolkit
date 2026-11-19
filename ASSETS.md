# DM's Toolkit, Asset Plan

Date: November 19, 2026
Status: Draft. Grounded in PRD.md. The architecture plan is not on the repo yet, so packaging specs here follow the standard 3DS homebrew layout and will be confirmed against ARCHITECTURE.md when it lands.

## How assets work in this app

- Image assets live in gfx/ as PNGs, compiled into romfs/ at build time (same pipeline as Bard Beats: sprites.t3s and tex3ds, to be confirmed by the architecture).
- The app is text-heavy: the monster manual, the fight tracker, and the reference are text. Art is for the frame around the text, not the content.
- The dice roller draws the die live in code (gold die on dark green felt). The die itself is not an image asset.

## App packaging

| Asset | Spec | Purpose | Status |
| icon.png | 48x48 PNG | Home menu icon, used to generate the SMDH | Needed |
| banner.png | 256x128 PNG | CIA HOME Menu banner image | Needed |
| banner audio | 2-second jingle, optional | CIA banner audio (dice roll sound or short lute note) | TBD |

## Title screen

- Title screen art, top screen 400x240. Bard's toolkit and tavern feel, generic fantasy only. Needed.

## Dice tray

- Dark green felt texture for the roller background, 400x240 or tileable. The gold die is drawn live in code, not an image. Needed.

## Tab and section icons

Small PNG sprites for the five sections: monster manual, fight tracker, dice roller, rules reference, generators. Needed. Exact size TBD by the architecture (32x32 or 48x48).

## Monster art

- Out of scope for v1. The manual is text stat blocks. No monster images.

## Sounds

- Not in the PRD for v1. Optional: dice roll sound, UI tap sound. TBD, decide with the architecture.

## Data (content, not art)

- SRD 5.1 data files (monsters, spells, races, classes, weapons, armor, gear, conditions). Source and format TBD by the architecture. This is the app's content, not image or audio assets, but it is listed here so nothing is forgotten.

## Out of scope for v1

- Monster images.
- Player-side art (there is no player side in the app).
- Custom homebrew art.

## Who makes what

- Icon, banner, title screen, felt texture, tab icons: to be made.
- SRD data: source TBD by the architecture.
- Sounds: TBD.
