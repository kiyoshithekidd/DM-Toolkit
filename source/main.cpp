#include <3ds.h>
#include <citro2d.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "app_state.h"
#include "ui_theme.h"

using namespace dmtk;

namespace {

// ── Text Helpers ──────────────────────────────────────────────────────

void drawText(C2D_TextBuf buf, const char* str, float x, float y,
              float scale, uint32_t color, float z = 0.5f) {
    if (!str || str[0] == '\0') return;
    C2D_Text txt;
    C2D_TextParse(&txt, buf, str);
    C2D_TextOptimize(&txt);
    C2D_DrawText(&txt, C2D_WithColor, x, y, z, scale, scale, color);
}

void drawTextCentered(C2D_TextBuf buf, const char* str, const Rect& r,
                      float scale, uint32_t color, float z = 0.55f) {
    if (!str || str[0] == '\0') return;
    C2D_Text txt;
    C2D_TextParse(&txt, buf, str);
    C2D_TextOptimize(&txt);
    float w = 0.0f, h = 0.0f;
    C2D_TextGetDimensions(&txt, scale, scale, &w, &h);
    float tx = r.x + (r.w - w) * 0.5f;
    float ty = r.y + (r.h - h) * 0.5f;
    C2D_DrawText(&txt, C2D_WithColor, tx, ty, z, scale, scale, color);
}

void drawButton(C2D_TextBuf buf, const Rect& r, const char* label,
                uint32_t bg, uint32_t border, uint32_t fg, float scale = 0.42f) {
    C2D_DrawRectSolid(r.x, r.y, 0.3f, r.w, r.h, border);
    C2D_DrawRectSolid(r.x + 1.0f, r.y + 1.0f, 0.35f, r.w - 2.0f, r.h - 2.0f, bg);
    drawTextCentered(buf, label, r, scale, fg, 0.55f);
}

// Simple word-wrapper for fixed-width columns on the 3DS top screen
std::vector<std::string> wrapLines(const std::string& text, size_t maxChars) {
    std::vector<std::string> lines;
    if (text.empty() || maxChars < 4) return lines;

    size_t pos = 0;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        std::string para = (nl == std::string::npos) ? text.substr(pos) : text.substr(pos, nl - pos);
        pos = (nl == std::string::npos) ? text.size() : nl + 1;

        if (para.empty()) {
            lines.push_back("");
            continue;
        }
        size_t start = 0;
        while (start < para.size()) {
            if (para.size() - start <= maxChars) {
                lines.push_back(para.substr(start));
                break;
            }
            size_t cut = para.rfind(' ', start + maxChars);
            if (cut == std::string::npos || cut <= start) {
                cut = start + maxChars;
            }
            lines.push_back(para.substr(start, cut - start));
            start = cut;
            while (start < para.size() && para[start] == ' ') ++start;
        }
    }
    return lines;
}

bool openSystemKeyboard(std::string& inOut, int maxChars, const char* hint) {
    SwkbdState swkbd;
    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, maxChars);
    swkbdSetInitialText(&swkbd, inOut.c_str());
    swkbdSetHintText(&swkbd, hint);
    swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, "Cancel", false);
    swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, "OK", true);
    char buf[96] = {0};
    SwkbdButton btn = swkbdInputText(&swkbd, buf, sizeof(buf));
    if (btn == SWKBD_BUTTON_CONFIRM && buf[0] != '\0') {
        inOut = buf;
        return true;
    }
    return false;
}

// ── Always-Live Bottom Screen QWERTY Keypad ───────────────────────────

static const char* kQwertyRows[3] = {
    "QWERTYUIOP",
    "ASDFGHJKL",
    "ZXCVBNM"
};

bool handleQwertyTouch(float tx, float ty, std::string& query) {
    // Keypad occupies y = 166..238
    if (ty < 166.0f || ty > 238.0f) return false;
    int row = static_cast<int>((ty - 166.0f) / 24.0f);
    if (row < 0 || row > 2) return false;

    const char* keys = kQwertyRows[row];
    int n = static_cast<int>(std::strlen(keys));
    float keyW = 30.0f;
    float startX = (row == 0) ? 10.0f : (row == 1) ? 25.0f : 10.0f;

    for (int i = 0; i < n; ++i) {
        Rect r{startX + i * keyW, 166.0f + row * 24.0f, keyW - 2.0f, 22.0f};
        if (r.contains(tx, ty)) {
            if (query.size() < 24) {
                char ch = static_cast<char>(keys[i] - 'A' + 'a');
                query.push_back(ch);
            }
            return true;
        }
    }

    if (row == 2) {
        Rect rSpace{220.0f, 214.0f, 44.0f, 22.0f};
        Rect rDel{266.0f, 214.0f, 46.0f, 22.0f};
        if (rSpace.contains(tx, ty)) {
            if (!query.empty() && query.back() != ' ' && query.size() < 24) {
                query.push_back(' ');
            }
            return true;
        }
        if (rDel.contains(tx, ty)) {
            if (!query.empty()) query.pop_back();
            return true;
        }
    }
    return false;
}

void drawQwertyKeypad(C2D_TextBuf buf) {
    C2D_DrawRectSolid(0.0f, 163.0f, 0.2f, 320.0f, 77.0f, theme::WALNUT_DARK);
    for (int row = 0; row < 3; ++row) {
        const char* keys = kQwertyRows[row];
        int n = static_cast<int>(std::strlen(keys));
        float keyW = 30.0f;
        float startX = (row == 0) ? 10.0f : (row == 1) ? 25.0f : 10.0f;
        for (int i = 0; i < n; ++i) {
            Rect r{startX + i * keyW, 166.0f + row * 24.0f, keyW - 2.0f, 22.0f};
            char lbl[2] = {keys[i], '\0'};
            drawButton(buf, r, lbl, theme::PARCHMENT, theme::WALNUT_LIGHT, theme::INK, 0.44f);
        }
    }
    drawButton(buf, Rect{220.0f, 214.0f, 44.0f, 22.0f}, "SPC", theme::PARCHMENT_ALT, theme::WALNUT_LIGHT, theme::INK, 0.36f);
    drawButton(buf, Rect{266.0f, 214.0f, 46.0f, 22.0f}, "DEL", theme::CRIMSON, theme::GOLD_DARK, theme::WHITE, 0.38f);
}

// ── Top Screen Renderers (400x240) ────────────────────────────────────

void renderMonsterStatBlock(C2D_TextBuf buf, const SrdDatabase& db, const MonsterView& mv,
                            float x, float y, float w, float h, int scrollLine, bool compact) {
    C2D_DrawRectSolid(x, y, 0.15f, w, h, theme::PARCHMENT);
    C2D_DrawRectSolid(x, y, 0.2f, w, 24.0f, theme::WALNUT);

    char hdr[64];
    std::snprintf(hdr, sizeof(hdr), "%.26s  [CR %s]", mv.name, crIndexToString(mv.crIndex));
    drawText(buf, hdr, x + 6.0f, y + 4.0f, 0.46f, theme::GOLD_BRIGHT);

    float cy = y + 27.0f;
    char lineBuf[128];

    // Vitals bar
    std::snprintf(lineBuf, sizeof(lineBuf), "AC %u  |  HP %u (%s)  |  %s",
                  static_cast<unsigned>(mv.armorClass),
                  static_cast<unsigned>(mv.hitPoints),
                  mv.hitDice[0] ? mv.hitDice : "-",
                  mv.speed);
    drawText(buf, lineBuf, x + 6.0f, cy, 0.36f, theme::CRIMSON_DARK);
    cy += 14.0f;

    // 6 Ability scores compact strip
    std::snprintf(lineBuf, sizeof(lineBuf), "STR %+d  DEX %+d  CON %+d  INT %+d  WIS %+d  CHA %+d",
                  abilityModifier(mv.strScore), abilityModifier(mv.dexScore),
                  abilityModifier(mv.conScore), abilityModifier(mv.intScore),
                  abilityModifier(mv.wisScore), abilityModifier(mv.chaScore));
    drawText(buf, lineBuf, x + 6.0f, cy, compact ? 0.32f : 0.36f, theme::INK);
    cy += 14.0f;

    C2D_DrawRectSolid(x + 4.0f, cy, 0.25f, w - 8.0f, 1.0f, theme::PARCHMENT_EDGE);
    cy += 4.0f;

    // Build scrollable text body
    std::vector<std::string> lines;
    size_t maxChars = compact ? 34 : 62;

    if (mv.sizeAndAlign[0]) {
        auto v = wrapLines(mv.sizeAndAlign, maxChars);
        lines.insert(lines.end(), v.begin(), v.end());
    }
    if (mv.saves[0]) {
        auto v = wrapLines(std::string("Saves: ") + mv.saves, maxChars);
        lines.insert(lines.end(), v.begin(), v.end());
    }
    if (mv.defenses[0]) {
        auto v = wrapLines(mv.defenses, maxChars);
        lines.insert(lines.end(), v.begin(), v.end());
    }
    if (mv.sensesLang[0]) {
        auto v = wrapLines(mv.sensesLang, maxChars);
        lines.insert(lines.end(), v.begin(), v.end());
    }

    for (int i = 0; i < mv.actionCount; ++i) {
        ActionView av{};
        if (!db.getMonsterAction(mv.id, i, av)) continue;
        const char* tag = (av.sectionType == 0) ? "[Trait] "
                        : (av.sectionType == 2) ? "[React] "
                        : (av.sectionType == 3) ? "[Legend] "
                        : "[Act] ";
        std::string entry = std::string(tag) + av.name + ": " + (compact ? av.summary : av.fullDesc);
        auto v = wrapLines(entry, maxChars);
        lines.insert(lines.end(), v.begin(), v.end());
    }

    int startIdx = std::max(0, std::min(scrollLine, std::max(0, static_cast<int>(lines.size()) - 1)));
    float lineH = 12.5f;
    for (size_t i = static_cast<size_t>(startIdx); i < lines.size() && cy + lineH <= y + h - 4.0f; ++i) {
        uint32_t col = (lines[i].rfind("[Act]", 0) == 0 || lines[i].rfind("[Legend]", 0) == 0)
                           ? theme::CRIMSON_DARK
                           : theme::INK;
        drawText(buf, lines[i].c_str(), x + 6.0f, cy, compact ? 0.33f : 0.36f, col);
        cy += lineH;
    }
}

void renderTopFight(C2D_TextBuf buf, const AppState& app) {
    // Left Column (0..176): Initiative Ladder
    C2D_DrawRectSolid(0.0f, 0.0f, 0.1f, 176.0f, 240.0f, theme::WALNUT_DARK);
    C2D_DrawRectSolid(0.0f, 0.0f, 0.2f, 176.0f, 24.0f, theme::WALNUT);

    char hdr[48];
    if (app.tracker.isInCombat()) {
        std::snprintf(hdr, sizeof(hdr), "ROUND %d  (%zu in fight)",
                      app.tracker.roundNumber(), app.tracker.combatants().size());
    } else {
        std::snprintf(hdr, sizeof(hdr), "FIGHT TRACKER (IDLE)");
    }
    drawText(buf, hdr, 6.0f, 4.0f, 0.44f, theme::GOLD_BRIGHT);

    const auto& list = app.tracker.combatants();
    if (list.empty()) {
        drawText(buf, "No active fight.", 10.0f, 40.0f, 0.42f, theme::PARCHMENT);
        drawText(buf, "Pick a saved encounter", 10.0f, 62.0f, 0.36f, theme::PARCHMENT_ALT);
        drawText(buf, "or add monsters below.", 10.0f, 78.0f, 0.36f, theme::PARCHMENT_ALT);
    } else {
        int visible = 7;
        int sel = app.tracker.selectedIndex();
        int scroll = std::max(0, std::min(sel - 3, static_cast<int>(list.size()) - visible));
        for (int row = 0; row < visible && (scroll + row) < static_cast<int>(list.size()); ++row) {
            int idx = scroll + row;
            const Combatant& c = list[static_cast<size_t>(idx)];
            float ry = 26.0f + row * 30.0f;
            bool isTurn = (idx == app.tracker.activeTurnIndex());
            bool isSel  = (idx == sel);
            bool dead   = (!c.isPlayer && c.currentHp <= 0);

            uint32_t bg = isTurn ? theme::ACTIVE_ROW : (isSel ? theme::PARCHMENT : theme::PARCHMENT_ALT);
            if (dead) bg = rgba32(190, 180, 165);
            uint32_t border = isSel ? theme::GOLD : theme::WALNUT_LIGHT;

            C2D_DrawRectSolid(2.0f, ry, 0.2f, 172.0f, 28.0f, border);
            C2D_DrawRectSolid(3.0f, ry + 1.0f, 0.25f, 170.0f, 26.0f, bg);

            // Init badge
            Rect ib{5.0f, ry + 3.0f, 24.0f, 22.0f};
            char initStr[8];
            std::snprintf(initStr, sizeof(initStr), "%d", c.initRoll);
            drawButton(buf, ib, initStr, c.isPlayer ? theme::PLAYER_TAG : theme::WALNUT,
                       theme::GOLD_DARK, theme::WHITE, 0.40f);

            // Name
            char nameStr[24];
            std::snprintf(nameStr, sizeof(nameStr), "%s%.16s", isTurn ? ">" : "", c.displayName.c_str());
            drawText(buf, nameStr, 32.0f, ry + 2.0f, 0.38f, dead ? theme::INK_MUTED : theme::INK);

            // HP or Party tag + Conditions
            if (c.isPlayer) {
                char pinfo[24];
                std::snprintf(pinfo, sizeof(pinfo), "PARTY  AC %u", static_cast<unsigned>(c.armorClass));
                drawText(buf, pinfo, 32.0f, ry + 15.0f, 0.32f, theme::PLAYER_TAG);
            } else {
                float pct = (c.maxHp > 0) ? static_cast<float>(c.currentHp) / static_cast<float>(c.maxHp) : 0.0f;
                uint32_t hpCol = (pct > 0.5f) ? theme::HP_GOOD : (pct > 0.25f ? theme::HP_WARN : theme::HP_CRIT);
                C2D_DrawRectSolid(32.0f, ry + 17.0f, 0.3f, 64.0f, 7.0f, theme::WALNUT_LIGHT);
                if (pct > 0.0f) {
                    C2D_DrawRectSolid(32.0f, ry + 17.0f, 0.35f, 64.0f * pct, 7.0f, hpCol);
                }
                char hpStr[20];
                std::snprintf(hpStr, sizeof(hpStr), "%d/%d", c.currentHp, c.maxHp);
                drawText(buf, hpStr, 100.0f, ry + 14.0f, 0.33f, theme::INK);
            }

            // Up to 2 compact condition badges on the right edge
            if (c.conditionsMask != 0) {
                int drawn = 0;
                for (int b = 0; b < CONDITION_COUNT && drawn < 2; ++b) {
                    if (c.conditionsMask & (1u << b)) {
                        Rect cb{144.0f - drawn * 26.0f, ry + 2.0f, 25.0f, 11.0f};
                        drawButton(buf, cb, conditionIndexToShort(b), theme::COND_BADGE, theme::WALNUT, theme::WHITE, 0.28f);
                        ++drawn;
                    }
                }
            }
        }
    }

    // Right Column (178..400): Combat Rules Drawer OR Selected Monster Stat Block
    if (app.combatRulesDrawerOpen) {
        C2D_DrawRectSolid(178.0f, 0.0f, 0.15f, 222.0f, 240.0f, theme::PARCHMENT);
        C2D_DrawRectSolid(178.0f, 0.0f, 0.2f, 222.0f, 24.0f, theme::FELT);
        RuleView rv{};
        if (app.db.getRule(static_cast<uint16_t>(app.drawerRuleIndex), rv)) {
            char rhdr[48];
            std::snprintf(rhdr, sizeof(rhdr), "RULE: %.24s", rv.name);
            drawText(buf, rhdr, 184.0f, 4.0f, 0.42f, theme::GOLD_BRIGHT);
            drawText(buf, "[L/R] Cycle Rule   [B] Close", 184.0f, 26.0f, 0.32f, theme::INK_MUTED);
            auto lines = wrapLines(rv.body, 34);
            int st = std::max(0, std::min(app.trackerRightScroll, std::max(0, static_cast<int>(lines.size()) - 1)));
            float cy = 42.0f;
            for (size_t i = static_cast<size_t>(st); i < lines.size() && cy < 228.0f; ++i) {
                drawText(buf, lines[i].c_str(), 184.0f, cy, 0.34f, theme::INK);
                cy += 13.0f;
            }
        }
    } else if (!list.empty()) {
        const Combatant& selC = list[static_cast<size_t>(app.tracker.selectedIndex())];
        if (!selC.isPlayer) {
            MonsterView mv{};
            if (app.db.getMonster(selC.monsterId, mv)) {
                renderMonsterStatBlock(buf, app.db, mv, 178.0f, 0.0f, 222.0f, 240.0f, app.trackerRightScroll, true);
            }
        } else {
            C2D_DrawRectSolid(178.0f, 0.0f, 0.15f, 222.0f, 240.0f, theme::PARCHMENT);
            C2D_DrawRectSolid(178.0f, 0.0f, 0.2f, 222.0f, 24.0f, theme::PLAYER_TAG);
            drawText(buf, selC.displayName.c_str(), 186.0f, 4.0f, 0.48f, theme::WHITE);
            char pbuf[64];
            std::snprintf(pbuf, sizeof(pbuf), "Party Hero  |  AC %u  |  Init %+d",
                          static_cast<unsigned>(selC.armorClass), static_cast<int>(selC.initBonus));
            drawText(buf, pbuf, 186.0f, 36.0f, 0.38f, theme::INK);
            drawText(buf, "Use [-Init / +Init] below if", 186.0f, 64.0f, 0.36f, theme::INK_MUTED);
            drawText(buf, "the player rolled physical dice.", 186.0f, 80.0f, 0.36f, theme::INK_MUTED);
        }
    } else {
        C2D_DrawRectSolid(178.0f, 0.0f, 0.15f, 222.0f, 240.0f, theme::PARCHMENT);
        C2D_DrawRectSolid(178.0f, 0.0f, 0.2f, 222.0f, 24.0f, theme::WALNUT);
        drawText(buf, app.campaign.name.c_str(), 186.0f, 4.0f, 0.44f, theme::GOLD_BRIGHT);
        char lvlBuf[48];
        std::snprintf(lvlBuf, sizeof(lvlBuf), "Party (Level %u):", static_cast<unsigned>(app.campaign.partyLevel));
        drawText(buf, lvlBuf, 186.0f, 34.0f, 0.40f, theme::CRIMSON_DARK);
        float cy = 54.0f;
        for (const auto& p : app.campaign.party) {
            char pb[48];
            std::snprintf(pb, sizeof(pb), "- %.16s  (Init %+d, AC %u)",
                          p.name.c_str(), static_cast<int>(p.initBonus), static_cast<unsigned>(p.ac));
            drawText(buf, pb, 186.0f, cy, 0.36f, theme::INK);
            cy += 16.0f;
        }
    }
}

void renderTopManual(C2D_TextBuf buf, const AppState& app) {
    if (app.monsterResults.empty()) {
        C2D_DrawRectSolid(0.0f, 0.0f, 0.1f, 400.0f, 240.0f, theme::PARCHMENT);
        drawText(buf, "No matching monsters found.", 24.0f, 96.0f, 0.50f, theme::INK_MUTED);
        return;
    }
    int idx = std::clamp(app.monsterListCursor, 0, static_cast<int>(app.monsterResults.size()) - 1);
    MonsterView mv{};
    if (app.db.getMonster(app.monsterResults[static_cast<size_t>(idx)].recordId, mv)) {
        renderMonsterStatBlock(buf, app.db, mv, 0.0f, 0.0f, 400.0f, 240.0f, app.manualTopScroll, false);
    }
}

void renderTopRules(C2D_TextBuf buf, const AppState& app) {
    C2D_DrawRectSolid(0.0f, 0.0f, 0.1f, 400.0f, 240.0f, theme::PARCHMENT);
    if (app.ruleResults.empty()) {
        drawText(buf, "No matching rules found.", 24.0f, 96.0f, 0.50f, theme::INK_MUTED);
        return;
    }
    int idx = std::clamp(app.ruleListCursor, 0, static_cast<int>(app.ruleResults.size()) - 1);
    RuleView rv{};
    if (app.db.getRule(app.ruleResults[static_cast<size_t>(idx)].recordId, rv)) {
        C2D_DrawRectSolid(0.0f, 0.0f, 0.2f, 400.0f, 26.0f, rv.isCondition ? theme::COND_BADGE : theme::FELT);
        char hdr[64];
        std::snprintf(hdr, sizeof(hdr), "%s  [%s]", rv.name, rv.category);
        drawText(buf, hdr, 8.0f, 5.0f, 0.48f, theme::WHITE);

        auto lines = wrapLines(rv.body, 60);
        int st = std::max(0, std::min(app.ruleTopScroll, std::max(0, static_cast<int>(lines.size()) - 1)));
        float cy = 34.0f;
        for (size_t i = static_cast<size_t>(st); i < lines.size() && cy < 228.0f; ++i) {
            drawText(buf, lines[i].c_str(), 10.0f, cy, 0.38f, theme::INK);
            cy += 14.0f;
        }
    }
}

void renderTopCampaign(C2D_TextBuf buf, const AppState& app) {
    C2D_DrawRectSolid(0.0f, 0.0f, 0.1f, 400.0f, 240.0f, theme::PARCHMENT);
    C2D_DrawRectSolid(0.0f, 0.0f, 0.2f, 400.0f, 26.0f, theme::WALNUT);

    if (app.campaignSubTab == 2) {
        drawText(buf, "CREDITS & OPEN CONTENT LICENSE (CC-BY-4.0)", 8.0f, 5.0f, 0.44f, theme::GOLD_BRIGHT);
        const char* legal =
            "This work includes material taken from the System Reference Document 5.1 (\"SRD 5.1\") "
            "by Wizards of the Coast LLC and available at "
            "https://dnd.wizards.com/resources/systems-reference-document. "
            "The SRD 5.1 is licensed under the Creative Commons Attribution 4.0 International License "
            "available at https://creativecommons.org/licenses/by/4.0/legalcode.\n\n"
            "SD Card Homebrew Overlay:\n"
            "Drop custom JSON packs into sdmc:/3ds/DM_Toolkit/homebrew/ to add custom entries.";
        auto lines = wrapLines(legal, 58);
        float cy = 36.0f;
        for (const auto& ln : lines) {
            drawText(buf, ln.c_str(), 10.0f, cy, 0.37f, theme::INK);
            cy += 14.0f;
        }
        return;
    }

    char hdr[64];
    std::snprintf(hdr, sizeof(hdr), "CAMPAIGN: %.28s (Lv %u)",
                  app.campaign.name.c_str(), static_cast<unsigned>(app.campaign.partyLevel));
    drawText(buf, hdr, 8.0f, 5.0f, 0.46f, theme::GOLD_BRIGHT);

    drawText(buf, "Party Roster:", 10.0f, 34.0f, 0.42f, theme::CRIMSON_DARK);
    float cy = 54.0f;
    for (size_t i = 0; i < app.campaign.party.size(); ++i) {
        const auto& p = app.campaign.party[i];
        char pb[64];
        std::snprintf(pb, sizeof(pb), "%s%.18s  |  Init %+d  |  AC %u",
                      (static_cast<int>(i) == app.partySelectedIdx ? "> " : "  "),
                      p.name.c_str(), static_cast<int>(p.initBonus), static_cast<unsigned>(p.ac));
        drawText(buf, pb, 10.0f, cy, 0.38f, (static_cast<int>(i) == app.partySelectedIdx) ? theme::CRIMSON : theme::INK);
        cy += 16.0f;
    }

    char stats[96];
    std::snprintf(stats, sizeof(stats), "Loaded: %u SRD Monsters, %u SD Homebrew Monsters, %zu Campaigns",
                  static_cast<unsigned>(app.db.srdMonsterCount()),
                  static_cast<unsigned>(app.db.homebrewMonsterCount()),
                  app.campaigns.size());
    drawText(buf, stats, 10.0f, 218.0f, 0.33f, theme::INK_MUTED);
}

// ── Bottom Screen Renderers (320x240) ─────────────────────────────────

void drawModeTabs(C2D_TextBuf buf, AppMode active) {
    const char* labels[4] = {"Fight", "Manual", "Rules", "Campaign"};
    for (int i = 0; i < 4; ++i) {
        Rect r{i * 80.0f, 0.0f, 80.0f, 28.0f};
        bool sel = (static_cast<int>(active) == i);
        drawButton(buf, r, labels[i],
                   sel ? theme::GOLD : theme::WALNUT,
                   theme::WALNUT_DARK,
                   sel ? theme::INK : theme::PARCHMENT, 0.42f);
    }
}

void renderBottomFight(C2D_TextBuf buf, const AppState& app) {
    if (!app.tracker.isInCombat()) {
        drawText(buf, "Saved Encounters (Tap to Roll Initiative):", 8.0f, 34.0f, 0.38f, theme::PARCHMENT);
        float y = 54.0f;
        for (size_t i = 0; i < app.campaign.encounters.size() && i < 3; ++i) {
            Rect r{8.0f, y, 304.0f, 38.0f};
            drawButton(buf, r, app.campaign.encounters[i].name.c_str(),
                       theme::FELT_LIGHT, theme::GOLD, theme::WHITE, 0.44f);
            y += 44.0f;
        }
        drawButton(buf, Rect{8.0f, 190.0f, 304.0f, 40.0f}, "+ Pick Monsters from Manual",
                   theme::GOLD, theme::WALNUT_DARK, theme::INK, 0.46f);
        return;
    }

    if (app.fightPanel == FIGHT_PANEL_CONDITIONS) {
        drawText(buf, "Toggle Conditions (Tap Pill):", 8.0f, 32.0f, 0.38f, theme::GOLD_BRIGHT);
        const Combatant& c = app.tracker.combatants()[static_cast<size_t>(app.tracker.selectedIndex())];
        for (int i = 0; i < CONDITION_COUNT; ++i) {
            int col = i % 3;
            int row = i / 3;
            Rect r{6.0f + col * 104.0f, 48.0f + row * 30.0f, 100.0f, 26.0f};
            bool on = (c.conditionsMask & (1u << i)) != 0;
            drawButton(buf, r, conditionIndexToName(i),
                       on ? theme::COND_BADGE : theme::WALNUT_LIGHT,
                       on ? theme::GOLD_BRIGHT : theme::WALNUT_DARK,
                       theme::WHITE, 0.36f);
        }
        drawButton(buf, Rect{6.0f, 202.0f, 148.0f, 32.0f}, "Clear Conditions", theme::CRIMSON_DARK, theme::GOLD_DARK, theme::WHITE, 0.40f);
        drawButton(buf, Rect{166.0f, 202.0f, 148.0f, 32.0f}, "Done", theme::FELT_LIGHT, theme::GOLD, theme::WHITE, 0.44f);
        return;
    }

    if (app.fightPanel == FIGHT_PANEL_END_CONFIRM) {
        drawText(buf, "End this fight and clear the tracker?", 20.0f, 68.0f, 0.46f, theme::PARCHMENT);
        drawText(buf, "(Your campaign & party stay saved.)", 36.0f, 92.0f, 0.38f, theme::PARCHMENT_ALT);
        drawButton(buf, Rect{16.0f, 132.0f, 136.0f, 48.0f}, "Cancel", theme::WALNUT_LIGHT, theme::GOLD_DARK, theme::WHITE, 0.46f);
        drawButton(buf, Rect{168.0f, 132.0f, 136.0f, 48.0f}, "End Fight", theme::CRIMSON, theme::GOLD, theme::WHITE, 0.46f);
        return;
    }

    const Combatant& c = app.tracker.combatants()[static_cast<size_t>(app.tracker.selectedIndex())];
    char selHdr[64];
    if (c.isPlayer) {
        std::snprintf(selHdr, sizeof(selHdr), "%.18s  |  Init %d", c.displayName.c_str(), c.initRoll);
    } else {
        std::snprintf(selHdr, sizeof(selHdr), "%.16s | HP %d/%d | Init %d",
                      c.displayName.c_str(), c.currentHp, c.maxHp, c.initRoll);
    }
    drawText(buf, selHdr, 8.0f, 32.0f, 0.40f, theme::GOLD_BRIGHT);

    // Big one-handed HP damage/heal buttons (row 1: y = 52..96)
    drawButton(buf, Rect{6.0f,   52.0f, 58.0f, 44.0f}, "-10", theme::CRIMSON, theme::GOLD_DARK, theme::WHITE, 0.48f);
    drawButton(buf, Rect{68.0f,  52.0f, 58.0f, 44.0f}, "-5",  theme::CRIMSON, theme::GOLD_DARK, theme::WHITE, 0.48f);
    drawButton(buf, Rect{130.0f, 52.0f, 58.0f, 44.0f}, "-1",  theme::CRIMSON_DARK, theme::GOLD_DARK, theme::WHITE, 0.48f);
    drawButton(buf, Rect{194.0f, 52.0f, 58.0f, 44.0f}, "+1",  theme::HP_GOOD, theme::GOLD_DARK, theme::WHITE, 0.48f);
    drawButton(buf, Rect{256.0f, 52.0f, 58.0f, 44.0f}, "+5",  theme::HP_GOOD, theme::GOLD_DARK, theme::WHITE, 0.48f);

    // Row 2 (y = 102..142): Conditions, Init -, Init +, Combat Rules
    drawButton(buf, Rect{6.0f,   102.0f, 96.0f, 38.0f}, "Conditions", theme::COND_BADGE, theme::GOLD_DARK, theme::WHITE, 0.38f);
    drawButton(buf, Rect{106.0f, 102.0f, 50.0f, 38.0f}, "Init-", theme::WALNUT_LIGHT, theme::GOLD_DARK, theme::PARCHMENT, 0.36f);
    drawButton(buf, Rect{160.0f, 102.0f, 50.0f, 38.0f}, "Init+", theme::WALNUT_LIGHT, theme::GOLD_DARK, theme::PARCHMENT, 0.36f);
    drawButton(buf, Rect{214.0f, 102.0f, 100.0f, 38.0f},
               app.combatRulesDrawerOpen ? "Hide Rules" : "Combat Rules",
               theme::FELT_LIGHT, theme::GOLD, theme::WHITE, 0.36f);

    // Row 3 (y = 148..194): Primary NEXT TURN + Add Monster
    drawButton(buf, Rect{6.0f,   148.0f, 184.0f, 46.0f}, "NEXT TURN  [A/R]", theme::GOLD, theme::WALNUT_DARK, theme::INK, 0.46f);
    drawButton(buf, Rect{196.0f, 148.0f, 118.0f, 46.0f}, "+ Monster", theme::FELT, theme::GOLD, theme::WHITE, 0.42f);

    // Row 4 (y = 200..234): Prev / Select / End Fight
    drawButton(buf, Rect{6.0f,   200.0f, 96.0f, 34.0f}, "Prev Turn", theme::WALNUT_LIGHT, theme::WALNUT_DARK, theme::PARCHMENT, 0.38f);
    drawButton(buf, Rect{108.0f, 200.0f, 96.0f, 34.0f}, "Save Fight", theme::WALNUT_LIGHT, theme::WALNUT_DARK, theme::PARCHMENT, 0.38f);
    drawButton(buf, Rect{210.0f, 200.0f, 104.0f, 34.0f}, "End Fight", theme::CRIMSON_DARK, theme::GOLD_DARK, theme::WHITE, 0.38f);
}

void renderBottomManual(C2D_TextBuf buf, const AppState& app) {
    // Search bar + filter pills (y = 32..58)
    C2D_DrawRectSolid(6.0f, 32.0f, 0.25f, 148.0f, 24.0f, theme::PARCHMENT);
    char qBuf[40];
    std::snprintf(qBuf, sizeof(qBuf), "Search: %s_", app.monsterQuery.c_str());
    drawText(buf, qBuf, 10.0f, 36.0f, 0.38f, theme::INK);

    const char* crLbl[CR_FILTER_COUNT] = {"CR:All", "CR:0-1", "CR:2-4", "CR:5-10", "CR:11+"};
    const char* tpLbl = (app.monsterTypeFilter < 0) ? "Type:All" : monsterTypeToString(static_cast<uint8_t>(app.monsterTypeFilter));
    drawButton(buf, Rect{158.0f, 32.0f, 66.0f, 24.0f}, tpLbl, theme::WALNUT_LIGHT, theme::GOLD_DARK, theme::PARCHMENT, 0.32f);
    drawButton(buf, Rect{226.0f, 32.0f, 54.0f, 24.0f}, crLbl[app.monsterCrFilter], theme::WALNUT_LIGHT, theme::GOLD_DARK, theme::PARCHMENT, 0.32f);
    drawButton(buf, Rect{282.0f, 32.0f, 34.0f, 24.0f}, "Clr", theme::CRIMSON_DARK, theme::GOLD_DARK, theme::WHITE, 0.34f);

    // 3 visible monster rows (y = 60..159)
    for (int i = 0; i < 3; ++i) {
        int idx = app.monsterListScroll + i;
        if (idx >= static_cast<int>(app.monsterResults.size())) break;
        const SearchHit& h = app.monsterResults[static_cast<size_t>(idx)];
        MonsterView mv{};
        if (!app.db.getMonster(h.recordId, mv)) continue;

        float ry = 60.0f + i * 33.0f;
        bool sel = (idx == app.monsterListCursor);
        C2D_DrawRectSolid(6.0f, ry, 0.2f, 246.0f, 30.0f, sel ? theme::GOLD : theme::WALNUT_LIGHT);
        C2D_DrawRectSolid(7.0f, ry + 1.0f, 0.25f, 244.0f, 28.0f, sel ? theme::ACTIVE_ROW : theme::PARCHMENT);

        char rowLbl[48];
        std::snprintf(rowLbl, sizeof(rowLbl), "%.20s%s", mv.name, mv.isHomebrew ? " [HB]" : "");
        drawText(buf, rowLbl, 12.0f, ry + 2.0f, 0.39f, theme::INK);

        char subLbl[48];
        std::snprintf(subLbl, sizeof(subLbl), "%s  |  CR %s  |  HP %u",
                      monsterTypeToString(mv.type), crIndexToString(mv.crIndex), static_cast<unsigned>(mv.hitPoints));
        drawText(buf, subLbl, 12.0f, ry + 16.0f, 0.31f, theme::INK_MUTED);

        drawButton(buf, Rect{256.0f, ry, 58.0f, 30.0f}, "+ Fight", theme::FELT_LIGHT, theme::GOLD, theme::WHITE, 0.36f);
    }

    drawQwertyKeypad(buf);
}

void renderBottomRules(C2D_TextBuf buf, const AppState& app) {
    C2D_DrawRectSolid(6.0f, 32.0f, 0.25f, 186.0f, 24.0f, theme::PARCHMENT);
    char qBuf[40];
    std::snprintf(qBuf, sizeof(qBuf), "Rule: %s_", app.ruleQuery.c_str());
    drawText(buf, qBuf, 10.0f, 36.0f, 0.38f, theme::INK);

    drawButton(buf, Rect{196.0f, 32.0f, 80.0f, 24.0f},
               app.rulesConditionsOnly ? "Conds Only" : "All Rules",
               theme::COND_BADGE, theme::GOLD_DARK, theme::WHITE, 0.33f);
    drawButton(buf, Rect{280.0f, 32.0f, 34.0f, 24.0f}, "Clr", theme::CRIMSON_DARK, theme::GOLD_DARK, theme::WHITE, 0.34f);

    for (int i = 0; i < 3; ++i) {
        int idx = app.ruleListScroll + i;
        if (idx >= static_cast<int>(app.ruleResults.size())) break;
        RuleView rv{};
        if (!app.db.getRule(app.ruleResults[static_cast<size_t>(idx)].recordId, rv)) continue;

        float ry = 60.0f + i * 33.0f;
        bool sel = (idx == app.ruleListCursor);
        C2D_DrawRectSolid(6.0f, ry, 0.2f, 308.0f, 30.0f, sel ? theme::GOLD : theme::WALNUT_LIGHT);
        C2D_DrawRectSolid(7.0f, ry + 1.0f, 0.25f, 306.0f, 28.0f, sel ? theme::ACTIVE_ROW : theme::PARCHMENT);
        drawText(buf, rv.name, 12.0f, ry + 3.0f, 0.40f, theme::INK);
        drawText(buf, rv.category, 12.0f, ry + 17.0f, 0.31f, rv.isCondition ? theme::COND_BADGE : theme::FELT);
    }

    drawQwertyKeypad(buf);
}

void renderBottomCampaign(C2D_TextBuf buf, const AppState& app) {
    drawButton(buf, Rect{6.0f,   34.0f, 98.0f, 28.0f}, "Party",
               app.campaignSubTab == 0 ? theme::GOLD : theme::WALNUT_LIGHT, theme::WALNUT_DARK,
               app.campaignSubTab == 0 ? theme::INK : theme::PARCHMENT, 0.38f);
    drawButton(buf, Rect{110.0f, 34.0f, 98.0f, 28.0f}, "Campaigns",
               app.campaignSubTab == 1 ? theme::GOLD : theme::WALNUT_LIGHT, theme::WALNUT_DARK,
               app.campaignSubTab == 1 ? theme::INK : theme::PARCHMENT, 0.38f);
    drawButton(buf, Rect{214.0f, 34.0f, 100.0f, 28.0f}, "CC-BY Legal",
               app.campaignSubTab == 2 ? theme::GOLD : theme::WALNUT_LIGHT, theme::WALNUT_DARK,
               app.campaignSubTab == 2 ? theme::INK : theme::PARCHMENT, 0.36f);

    if (app.campaignSubTab == 0) {
        drawButton(buf, Rect{6.0f,   72.0f, 148.0f, 38.0f}, "+ Add Hero", theme::FELT_LIGHT, theme::GOLD, theme::WHITE, 0.42f);
        drawButton(buf, Rect{166.0f, 72.0f, 148.0f, 38.0f}, "Rename Hero", theme::WALNUT_LIGHT, theme::GOLD_DARK, theme::PARCHMENT, 0.40f);
        drawButton(buf, Rect{6.0f,  118.0f, 72.0f,  38.0f}, "Init -1", theme::CRIMSON_DARK, theme::GOLD_DARK, theme::WHITE, 0.40f);
        drawButton(buf, Rect{82.0f, 118.0f, 72.0f,  38.0f}, "Init +1", theme::HP_GOOD, theme::GOLD_DARK, theme::WHITE, 0.40f);
        drawButton(buf, Rect{166.0f,118.0f, 72.0f,  38.0f}, "Lv -1", theme::WALNUT_LIGHT, theme::GOLD_DARK, theme::PARCHMENT, 0.40f);
        drawButton(buf, Rect{242.0f,118.0f, 72.0f,  38.0f}, "Lv +1", theme::WALNUT_LIGHT, theme::GOLD_DARK, theme::PARCHMENT, 0.40f);
        drawButton(buf, Rect{6.0f,  166.0f, 308.0f, 38.0f}, "Save Campaign to SD", theme::GOLD, theme::WALNUT_DARK, theme::INK, 0.44f);
    } else if (app.campaignSubTab == 1) {
        float y = 70.0f;
        for (size_t i = 0; i < app.campaigns.size() && i < 3; ++i) {
            bool active = (app.campaigns[i].slug == app.campaign.slug);
            drawButton(buf, Rect{6.0f, y, 308.0f, 34.0f}, app.campaigns[i].name.c_str(),
                       active ? theme::FELT_LIGHT : theme::WALNUT_LIGHT,
                       active ? theme::GOLD : theme::WALNUT_DARK, theme::WHITE, 0.42f);
            y += 38.0f;
        }
        drawButton(buf, Rect{6.0f, 192.0f, 308.0f, 38.0f}, "+ Create New Campaign", theme::GOLD, theme::WALNUT_DARK, theme::INK, 0.44f);
    } else {
        drawText(buf, "DM's Toolkit (Slice 1)", 10.0f, 80.0f, 0.46f, theme::GOLD_BRIGHT);
        drawText(buf, "SRD 5.1 under CC-BY-4.0", 10.0f, 106.0f, 0.40f, theme::PARCHMENT);
        drawText(buf, "See top screen for full legal notice.", 10.0f, 130.0f, 0.36f, theme::PARCHMENT_ALT);
    }
}

} // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    romfsInit();
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    C3D_RenderTarget* topTarget = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* botTarget = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    C2D_TextBuf topBuf = C2D_TextBufNew(8192);
    C2D_TextBuf botBuf = C2D_TextBufNew(4096);

    AppState app;
    app.rngState = static_cast<uint32_t>(svcGetSystemTick() ^ 0xA5A55A5Au);

    if (app.db.loadFromFile("romfs:/data/srd.bin")) {
        app.campaignMgr.ensureDefaultCampaign(app.db);
        int hbLoaded = app.db.loadHomebrewDir(app.campaignMgr.homebrewDir().c_str());
        app.campaigns = app.campaignMgr.listCampaigns();
        if (!app.campaigns.empty()) {
            app.campaignMgr.loadCampaign(app.campaigns[0].slug, app.campaign, &app.tracker);
        }
        app.refreshMonsterSearch();
        app.refreshRuleSearch();
        if (hbLoaded > 0) {
            app.setStatus("Loaded SRD + SD homebrew entries.");
        }
    } else {
        app.setStatus("Error: romfs:/data/srd.bin missing!", 300);
    }

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        if (kDown & KEY_START) break;

        touchPosition touch{};
        bool tapped = (kDown & KEY_TOUCH) != 0;
        if (tapped) hidTouchRead(&touch);
        float tx = static_cast<float>(touch.px);
        float ty = static_cast<float>(touch.py);

        // Mode tabs (y = 0..28)
        if (tapped && ty <= 28.0f) {
            int tab = std::clamp(static_cast<int>(tx / 80.0f), 0, 3);
            app.mode = static_cast<AppMode>(tab);
            app.fightPanel = FIGHT_PANEL_MAIN;
        } else if (app.mode == MODE_FIGHT) {
            // Hardware buttons in Fight Tracker
            if (kDown & KEY_DUP) {
                if (app.tracker.isInCombat()) {
                    app.tracker.setSelectedIndex(app.tracker.selectedIndex() - 1);
                    app.trackerRightScroll = 0;
                }
            } else if (kDown & KEY_DDOWN) {
                if (app.tracker.isInCombat()) {
                    app.tracker.setSelectedIndex(app.tracker.selectedIndex() + 1);
                    app.trackerRightScroll = 0;
                }
            } else if ((kDown & KEY_A) || ((kDown & KEY_R) && !app.combatRulesDrawerOpen)) {
                if (app.tracker.isInCombat()) {
                    app.tracker.nextTurn();
                    app.trackerRightScroll = 0;
                }
            } else if (kDown & KEY_B) {
                if (app.combatRulesDrawerOpen) app.combatRulesDrawerOpen = false;
                else if (app.fightPanel != FIGHT_PANEL_MAIN) app.fightPanel = FIGHT_PANEL_MAIN;
            } else if (app.combatRulesDrawerOpen && (kDown & KEY_R)) {
                app.drawerRuleIndex = (app.drawerRuleIndex + 1) % std::max(1u, app.db.ruleCount());
                app.trackerRightScroll = 0;
            } else if (app.combatRulesDrawerOpen && (kDown & KEY_L)) {
                int rc = static_cast<int>(std::max(1u, app.db.ruleCount()));
                app.drawerRuleIndex = (app.drawerRuleIndex + rc - 1) % rc;
                app.trackerRightScroll = 0;
            } else if (kDown & KEY_CSTICK_DOWN || kDown & KEY_CPAD_DOWN) {
                app.trackerRightScroll += 2;
            } else if (kDown & KEY_CSTICK_UP || kDown & KEY_CPAD_UP) {
                app.trackerRightScroll = std::max(0, app.trackerRightScroll - 2);
            }

            // Touch controls in Fight Tracker
            if (tapped) {
                if (!app.tracker.isInCombat()) {
                    float y = 54.0f;
                    for (size_t i = 0; i < app.campaign.encounters.size() && i < 3; ++i) {
                        if (Rect{8.0f, y, 304.0f, 38.0f}.contains(tx, ty)) {
                            app.tracker.startEncounter(app.campaign.encounters[i], app.campaign.party, app.db, app.rngState);
                            app.campaignMgr.saveCampaign(app.campaign, &app.tracker);
                            app.setStatus("Initiative rolled!");
                            break;
                        }
                        y += 44.0f;
                    }
                    if (Rect{8.0f, 190.0f, 304.0f, 40.0f}.contains(tx, ty)) {
                        app.mode = MODE_MANUAL;
                    }
                } else if (app.fightPanel == FIGHT_PANEL_CONDITIONS) {
                    for (int i = 0; i < CONDITION_COUNT; ++i) {
                        int col = i % 3;
                        int row = i / 3;
                        if (Rect{6.0f + col * 104.0f, 48.0f + row * 30.0f, 100.0f, 26.0f}.contains(tx, ty)) {
                            app.tracker.toggleCondition(app.tracker.selectedIndex(), i);
                            app.drawerRuleIndex = i; // Conditions are 0..14
                            break;
                        }
                    }
                    if (Rect{6.0f, 202.0f, 148.0f, 32.0f}.contains(tx, ty)) {
                        app.tracker.clearConditions(app.tracker.selectedIndex());
                    } else if (Rect{166.0f, 202.0f, 148.0f, 32.0f}.contains(tx, ty)) {
                        app.fightPanel = FIGHT_PANEL_MAIN;
                    }
                } else if (app.fightPanel == FIGHT_PANEL_END_CONFIRM) {
                    if (Rect{16.0f, 132.0f, 136.0f, 48.0f}.contains(tx, ty)) {
                        app.fightPanel = FIGHT_PANEL_MAIN;
                    } else if (Rect{168.0f, 132.0f, 136.0f, 48.0f}.contains(tx, ty)) {
                        app.tracker.clear();
                        app.campaignMgr.saveCampaign(app.campaign, &app.tracker);
                        app.fightPanel = FIGHT_PANEL_MAIN;
                        app.setStatus("Fight cleared. Campaign kept.");
                    }
                } else {
                    int sel = app.tracker.selectedIndex();
                    if (Rect{6.0f,   52.0f, 58.0f, 44.0f}.contains(tx, ty)) app.tracker.applyHpDelta(sel, -10);
                    else if (Rect{68.0f,  52.0f, 58.0f, 44.0f}.contains(tx, ty)) app.tracker.applyHpDelta(sel, -5);
                    else if (Rect{130.0f, 52.0f, 58.0f, 44.0f}.contains(tx, ty)) app.tracker.applyHpDelta(sel, -1);
                    else if (Rect{194.0f, 52.0f, 58.0f, 44.0f}.contains(tx, ty)) app.tracker.applyHpDelta(sel, +1);
                    else if (Rect{256.0f, 52.0f, 58.0f, 44.0f}.contains(tx, ty)) app.tracker.applyHpDelta(sel, +5);
                    else if (Rect{6.0f,   102.0f, 96.0f, 38.0f}.contains(tx, ty)) app.fightPanel = FIGHT_PANEL_CONDITIONS;
                    else if (Rect{106.0f, 102.0f, 50.0f, 38.0f}.contains(tx, ty)) {
                        app.tracker.setInitiative(sel, app.tracker.combatants()[static_cast<size_t>(sel)].initRoll - 1);
                    } else if (Rect{160.0f, 102.0f, 50.0f, 38.0f}.contains(tx, ty)) {
                        app.tracker.setInitiative(sel, app.tracker.combatants()[static_cast<size_t>(sel)].initRoll + 1);
                    } else if (Rect{214.0f, 102.0f, 100.0f, 38.0f}.contains(tx, ty)) {
                        app.combatRulesDrawerOpen = !app.combatRulesDrawerOpen;
                        app.trackerRightScroll = 0;
                    } else if (Rect{6.0f, 148.0f, 184.0f, 46.0f}.contains(tx, ty)) {
                        app.tracker.nextTurn();
                        app.trackerRightScroll = 0;
                    } else if (Rect{196.0f, 148.0f, 118.0f, 46.0f}.contains(tx, ty)) {
                        app.mode = MODE_MANUAL;
                    } else if (Rect{6.0f, 200.0f, 96.0f, 34.0f}.contains(tx, ty)) {
                        app.tracker.prevTurn();
                    } else if (Rect{108.0f, 200.0f, 96.0f, 34.0f}.contains(tx, ty)) {
                        app.campaignMgr.saveCampaign(app.campaign, &app.tracker);
                        app.setStatus("Active fight saved.");
                    } else if (Rect{210.0f, 200.0f, 104.0f, 34.0f}.contains(tx, ty)) {
                        app.fightPanel = FIGHT_PANEL_END_CONFIRM;
                    }
                }
            }
        } else if (app.mode == MODE_MANUAL) {
            if (kDown & KEY_DUP) {
                if (app.monsterListCursor > 0) {
                    --app.monsterListCursor;
                    if (app.monsterListCursor < app.monsterListScroll) app.monsterListScroll = app.monsterListCursor;
                    app.manualTopScroll = 0;
                }
            } else if (kDown & KEY_DDOWN) {
                if (app.monsterListCursor + 1 < static_cast<int>(app.monsterResults.size())) {
                    ++app.monsterListCursor;
                    if (app.monsterListCursor >= app.monsterListScroll + 3) app.monsterListScroll = app.monsterListCursor - 2;
                    app.manualTopScroll = 0;
                }
            } else if (kDown & KEY_R || kDown & KEY_CPAD_DOWN) {
                app.manualTopScroll += 3;
            } else if (kDown & KEY_L || kDown & KEY_CPAD_UP) {
                app.manualTopScroll = std::max(0, app.manualTopScroll - 3);
            }

            if (tapped) {
                if (Rect{158.0f, 32.0f, 66.0f, 24.0f}.contains(tx, ty)) {
                    app.monsterTypeFilter = (app.monsterTypeFilter + 2 > MT_COUNT) ? -1 : app.monsterTypeFilter + 1;
                    app.refreshMonsterSearch();
                } else if (Rect{226.0f, 32.0f, 54.0f, 24.0f}.contains(tx, ty)) {
                    app.monsterCrFilter = static_cast<CrFilter>((app.monsterCrFilter + 1) % CR_FILTER_COUNT);
                    app.refreshMonsterSearch();
                } else if (Rect{282.0f, 32.0f, 34.0f, 24.0f}.contains(tx, ty)) {
                    app.monsterQuery.clear();
                    app.monsterTypeFilter = -1;
                    app.monsterCrFilter = CR_FILTER_ALL;
                    app.refreshMonsterSearch();
                } else if (handleQwertyTouch(tx, ty, app.monsterQuery)) {
                    app.refreshMonsterSearch();
                } else {
                    for (int i = 0; i < 3; ++i) {
                        int idx = app.monsterListScroll + i;
                        if (idx >= static_cast<int>(app.monsterResults.size())) break;
                        float ry = 60.0f + i * 33.0f;
                        if (Rect{256.0f, ry, 58.0f, 30.0f}.contains(tx, ty)) {
                            app.tracker.addPartyIfMissing(app.campaign.party, app.rngState);
                            app.tracker.addMonsterMidFight(app.monsterResults[static_cast<size_t>(idx)].recordId, 1, app.db, app.rngState);
                            app.campaignMgr.saveCampaign(app.campaign, &app.tracker);
                            app.setStatus("Added to Fight Tracker!");
                            break;
                        } else if (Rect{6.0f, ry, 246.0f, 30.0f}.contains(tx, ty)) {
                            app.monsterListCursor = idx;
                            app.manualTopScroll = 0;
                            break;
                        }
                    }
                }
            }
        } else if (app.mode == MODE_RULES) {
            if (kDown & KEY_DUP && app.ruleListCursor > 0) {
                --app.ruleListCursor;
                if (app.ruleListCursor < app.ruleListScroll) app.ruleListScroll = app.ruleListCursor;
                app.ruleTopScroll = 0;
            } else if (kDown & KEY_DDOWN && app.ruleListCursor + 1 < static_cast<int>(app.ruleResults.size())) {
                ++app.ruleListCursor;
                if (app.ruleListCursor >= app.ruleListScroll + 3) app.ruleListScroll = app.ruleListCursor - 2;
                app.ruleTopScroll = 0;
            } else if (kDown & KEY_R || kDown & KEY_CPAD_DOWN) {
                app.ruleTopScroll += 3;
            } else if (kDown & KEY_L || kDown & KEY_CPAD_UP) {
                app.ruleTopScroll = std::max(0, app.ruleTopScroll - 3);
            }

            if (tapped) {
                if (Rect{196.0f, 32.0f, 80.0f, 24.0f}.contains(tx, ty)) {
                    app.rulesConditionsOnly = !app.rulesConditionsOnly;
                    app.refreshRuleSearch();
                } else if (Rect{280.0f, 32.0f, 34.0f, 24.0f}.contains(tx, ty)) {
                    app.ruleQuery.clear();
                    app.rulesConditionsOnly = false;
                    app.refreshRuleSearch();
                } else if (handleQwertyTouch(tx, ty, app.ruleQuery)) {
                    app.refreshRuleSearch();
                } else {
                    for (int i = 0; i < 3; ++i) {
                        int idx = app.ruleListScroll + i;
                        if (idx >= static_cast<int>(app.ruleResults.size())) break;
                        if (Rect{6.0f, 60.0f + i * 33.0f, 308.0f, 30.0f}.contains(tx, ty)) {
                            app.ruleListCursor = idx;
                            app.ruleTopScroll = 0;
                            break;
                        }
                    }
                }
            }
        } else if (app.mode == MODE_CAMPAIGN) {
            if (kDown & KEY_DUP && app.partySelectedIdx > 0) --app.partySelectedIdx;
            if (kDown & KEY_DDOWN && app.partySelectedIdx + 1 < static_cast<int>(app.campaign.party.size())) ++app.partySelectedIdx;

            if (tapped) {
                if (Rect{6.0f, 34.0f, 98.0f, 28.0f}.contains(tx, ty)) app.campaignSubTab = 0;
                else if (Rect{110.0f, 34.0f, 98.0f, 28.0f}.contains(tx, ty)) app.campaignSubTab = 1;
                else if (Rect{214.0f, 34.0f, 100.0f, 28.0f}.contains(tx, ty)) app.campaignSubTab = 2;
                else if (app.campaignSubTab == 0) {
                    if (Rect{6.0f, 72.0f, 148.0f, 38.0f}.contains(tx, ty) && app.campaign.party.size() < 8) {
                        std::string hname = "New Hero";
                        if (openSystemKeyboard(hname, 24, "Hero Name")) {
                            app.campaign.party.push_back({hname, 1, app.campaign.partyLevel, 14, true});
                            app.partySelectedIdx = static_cast<int>(app.campaign.party.size()) - 1;
                            app.campaignMgr.saveCampaign(app.campaign, &app.tracker);
                        }
                    } else if (Rect{166.0f, 72.0f, 148.0f, 38.0f}.contains(tx, ty) && !app.campaign.party.empty()) {
                        auto& p = app.campaign.party[static_cast<size_t>(app.partySelectedIdx)];
                        if (openSystemKeyboard(p.name, 24, "Rename Hero")) {
                            app.campaignMgr.saveCampaign(app.campaign, &app.tracker);
                        }
                    } else if (Rect{6.0f, 118.0f, 72.0f, 38.0f}.contains(tx, ty) && !app.campaign.party.empty()) {
                        auto& p = app.campaign.party[static_cast<size_t>(app.partySelectedIdx)];
                        p.initBonus = static_cast<int8_t>(std::max(-5, static_cast<int>(p.initBonus) - 1));
                    } else if (Rect{82.0f, 118.0f, 72.0f, 38.0f}.contains(tx, ty) && !app.campaign.party.empty()) {
                        auto& p = app.campaign.party[static_cast<size_t>(app.partySelectedIdx)];
                        p.initBonus = static_cast<int8_t>(std::min(15, static_cast<int>(p.initBonus) + 1));
                    } else if (Rect{166.0f, 118.0f, 72.0f, 38.0f}.contains(tx, ty)) {
                        app.campaign.partyLevel = static_cast<uint8_t>(std::max(1, static_cast<int>(app.campaign.partyLevel) - 1));
                    } else if (Rect{242.0f, 118.0f, 72.0f, 38.0f}.contains(tx, ty)) {
                        app.campaign.partyLevel = static_cast<uint8_t>(std::min(20, static_cast<int>(app.campaign.partyLevel) + 1));
                    } else if (Rect{6.0f, 166.0f, 308.0f, 38.0f}.contains(tx, ty)) {
                        app.campaignMgr.saveCampaign(app.campaign, &app.tracker);
                        app.setStatus("Campaign saved to SD!");
                    }
                } else if (app.campaignSubTab == 1) {
                    float y = 70.0f;
                    for (size_t i = 0; i < app.campaigns.size() && i < 3; ++i) {
                        if (Rect{6.0f, y, 308.0f, 34.0f}.contains(tx, ty)) {
                            app.campaignMgr.saveCampaign(app.campaign, &app.tracker);
                            app.campaignMgr.loadCampaign(app.campaigns[i].slug, app.campaign, &app.tracker);
                            app.partySelectedIdx = 0;
                            app.setStatus("Switched campaign.");
                            break;
                        }
                        y += 38.0f;
                    }
                    if (Rect{6.0f, 192.0f, 308.0f, 38.0f}.contains(tx, ty)) {
                        std::string cname = "New Campaign";
                        if (openSystemKeyboard(cname, 28, "Campaign Name")) {
                            app.campaignMgr.saveCampaign(app.campaign, &app.tracker);
                            app.campaignMgr.createCampaign(cname, app.campaign);
                            app.tracker.clear();
                            app.campaigns = app.campaignMgr.listCampaigns();
                            app.setStatus("Created campaign.");
                        }
                    }
                }
            }
        }

        if (app.statusFrames > 0) --app.statusFrames;

        // ── Render Frame ──────────────────────────────────────────────
        C2D_TextBufClear(topBuf);
        C2D_TextBufClear(botBuf);
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        C2D_TargetClear(topTarget, theme::WALNUT_DARK);
        C2D_SceneBegin(topTarget);
        if (app.mode == MODE_FIGHT)         renderTopFight(topBuf, app);
        else if (app.mode == MODE_MANUAL)   renderTopManual(topBuf, app);
        else if (app.mode == MODE_RULES)    renderTopRules(topBuf, app);
        else                                renderTopCampaign(topBuf, app);

        C2D_TargetClear(botTarget, theme::WALNUT);
        C2D_SceneBegin(botTarget);
        drawModeTabs(botBuf, app.mode);
        if (app.mode == MODE_FIGHT)         renderBottomFight(botBuf, app);
        else if (app.mode == MODE_MANUAL)   renderBottomManual(botBuf, app);
        else if (app.mode == MODE_RULES)    renderBottomRules(botBuf, app);
        else                                renderBottomCampaign(botBuf, app);

        if (app.statusFrames > 0 && !app.statusBanner.empty()) {
            Rect toast{20.0f, 212.0f, 280.0f, 22.0f};
            drawButton(botBuf, toast, app.statusBanner.c_str(), theme::FELT, theme::GOLD, theme::WHITE, 0.36f);
        }

        C3D_FrameEnd(0);
    }

    // Auto-save active campaign and fight on clean exit
    app.campaignMgr.saveCampaign(app.campaign, &app.tracker);

    C2D_TextBufDelete(topBuf);
    C2D_TextBufDelete(botBuf);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    romfsExit();
    return 0;
}
