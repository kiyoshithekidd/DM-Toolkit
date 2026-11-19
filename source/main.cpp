#include <3ds.h>
#include <citro2d.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "app_state.h"
#include "dice_view.h"
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
                uint32_t bg, uint32_t border, uint32_t fg, float scale = 0.40f) {
    C2D_DrawRectSolid(r.x, r.y, 0.3f, r.w, r.h, border);
    C2D_DrawRectSolid(r.x + 1.0f, r.y + 1.0f, 0.35f, r.w - 2.0f, r.h - 2.0f, bg);
    drawTextCentered(buf, label, r, scale, fg, 0.55f);
}

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

static const char* kQwertyRows[3] = {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};

bool handleQwertyTouch(float tx, float ty, std::string& query) {
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
                query.push_back(static_cast<char>(keys[i] - 'A' + 'a'));
            }
            return true;
        }
    }

    if (row == 2) {
        if (Rect{220.0f, 214.0f, 44.0f, 22.0f}.contains(tx, ty)) {
            if (!query.empty() && query.back() != ' ' && query.size() < 24) query.push_back(' ');
            return true;
        }
        if (Rect{266.0f, 214.0f, 46.0f, 22.0f}.contains(tx, ty)) {
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
    std::snprintf(lineBuf, sizeof(lineBuf), "AC %u  |  HP %u (%s)  |  %s",
                  static_cast<unsigned>(mv.armorClass),
                  static_cast<unsigned>(mv.hitPoints),
                  mv.hitDice[0] ? mv.hitDice : "-",
                  mv.speed);
    drawText(buf, lineBuf, x + 6.0f, cy, 0.36f, theme::CRIMSON_DARK);
    cy += 14.0f;

    std::snprintf(lineBuf, sizeof(lineBuf), "STR %+d  DEX %+d  CON %+d  INT %+d  WIS %+d  CHA %+d",
                  abilityModifier(mv.strScore), abilityModifier(mv.dexScore),
                  abilityModifier(mv.conScore), abilityModifier(mv.intScore),
                  abilityModifier(mv.wisScore), abilityModifier(mv.chaScore));
    drawText(buf, lineBuf, x + 6.0f, cy, compact ? 0.32f : 0.36f, theme::INK);
    cy += 14.0f;

    C2D_DrawRectSolid(x + 4.0f, cy, 0.25f, w - 8.0f, 1.0f, theme::PARCHMENT_EDGE);
    cy += 4.0f;

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

            Rect ib{5.0f, ry + 3.0f, 24.0f, 22.0f};
            char initStr[8];
            std::snprintf(initStr, sizeof(initStr), "%d", c.initRoll);
            drawButton(buf, ib, initStr, c.isPlayer ? theme::PLAYER_TAG : theme::WALNUT,
                       theme::GOLD_DARK, theme::WHITE, 0.40f);

            char nameStr[24];
            std::snprintf(nameStr, sizeof(nameStr), "%s%.16s", isTurn ? ">" : "", c.displayName.c_str());
            drawText(buf, nameStr, 32.0f, ry + 2.0f, 0.38f, dead ? theme::INK_MUTED : theme::INK);

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
            drawText(buf, "Use [Init- / Init+] below if", 186.0f, 64.0f, 0.36f, theme::INK_MUTED);
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

void renderSpellCard(C2D_TextBuf buf, const SpellView& sv, int scrollLine, const char* aliasBanner = nullptr) {
    C2D_DrawRectSolid(0.0f, 0.0f, 0.1f, 400.0f, 240.0f, theme::PARCHMENT);
    C2D_DrawRectSolid(0.0f, 0.0f, 0.2f, 400.0f, 26.0f, theme::SPELL_BLUE);

    char hdr[72];
    if (sv.level == 0) {
        std::snprintf(hdr, sizeof(hdr), "%.28s  (Cantrip %s)", sv.name, sv.school);
    } else {
        std::snprintf(hdr, sizeof(hdr), "%.28s  (Lv %u %s)", sv.name, static_cast<unsigned>(sv.level), sv.school);
    }
    drawText(buf, hdr, 8.0f, 5.0f, 0.46f, theme::WHITE);

    float cy = 30.0f;
    if (aliasBanner && aliasBanner[0]) {
        C2D_DrawRectSolid(6.0f, cy, 0.2f, 388.0f, 16.0f, theme::AMBER_BG);
        drawText(buf, aliasBanner, 10.0f, cy + 2.0f, 0.34f, theme::AMBER_HDR);
        cy += 18.0f;
    }

    char meta1[96], meta2[96];
    std::snprintf(meta1, sizeof(meta1), "Time: %s  |  Range: %s", sv.castingTime, sv.range);
    std::snprintf(meta2, sizeof(meta2), "Duration: %s%s  |  Classes: %s",
                  sv.duration, sv.concentration ? " (Conc)" : "", sv.classes);
    drawText(buf, meta1, 8.0f, cy, 0.35f, theme::CRIMSON_DARK); cy += 13.0f;
    drawText(buf, meta2, 8.0f, cy, 0.35f, theme::INK_MUTED); cy += 14.0f;
    C2D_DrawRectSolid(6.0f, cy, 0.25f, 388.0f, 1.0f, theme::PARCHMENT_EDGE);
    cy += 4.0f;

    std::string full = std::string("Components: ") + sv.components + "\n\n" + sv.body;
    auto lines = wrapLines(full, 62);
    int st = std::max(0, std::min(scrollLine, std::max(0, static_cast<int>(lines.size()) - 1)));
    for (size_t i = static_cast<size_t>(st); i < lines.size() && cy < 228.0f; ++i) {
        drawText(buf, lines[i].c_str(), 8.0f, cy, 0.36f, theme::INK);
        cy += 13.5f;
    }
}

void renderTopRules(C2D_TextBuf buf, const AppState& app) {
    C2D_DrawRectSolid(0.0f, 0.0f, 0.1f, 400.0f, 240.0f, theme::PARCHMENT);
    if (app.ruleResults.empty()) {
        drawText(buf, "No matching SRD entries found.", 24.0f, 96.0f, 0.50f, theme::INK_MUTED);
        return;
    }

    int idx = std::clamp(app.ruleListCursor, 0, static_cast<int>(app.ruleResults.size()) - 1);
    const SearchHit& h = app.ruleResults[static_cast<size_t>(idx)];

    if (h.category == CAT_SPELL) {
        SpellView sv{};
        if (app.db.getSpell(h.recordId, sv)) {
            renderSpellCard(buf, sv, app.ruleTopScroll);
        }
    } else if (h.category == CAT_RULE || h.category == CAT_CONDITION) {
        RuleView rv{};
        if (app.db.getRule(h.recordId, rv)) {
            C2D_DrawRectSolid(0.0f, 0.0f, 0.2f, 400.0f, 26.0f, rv.isCondition ? theme::COND_BADGE : theme::FELT);
            char hdr[64];
            std::snprintf(hdr, sizeof(hdr), "%s  [%s]", rv.name, rv.category);
            drawText(buf, hdr, 8.0f, 5.0f, 0.46f, theme::WHITE);

            auto lines = wrapLines(rv.body, 60);
            int st = std::max(0, std::min(app.ruleTopScroll, std::max(0, static_cast<int>(lines.size()) - 1)));
            float cy = 34.0f;
            for (size_t i = static_cast<size_t>(st); i < lines.size() && cy < 228.0f; ++i) {
                drawText(buf, lines[i].c_str(), 10.0f, cy, 0.37f, theme::INK);
                cy += 14.0f;
            }
        }
    } else {
        ReferenceView rv{};
        if (app.db.getReference(h.recordId, rv)) {
            if (rv.category == CAT_SRD_GAP && rv.subType == GAP_ALIAS && rv.linkedId != 0xFFFF) {
                SpellView sv{};
                if (app.db.getSpell(rv.linkedId, sv)) {
                    renderSpellCard(buf, sv, app.ruleTopScroll, rv.subtitle);
                    return;
                }
            }
            bool isGap = (rv.category == CAT_SRD_GAP);
            C2D_DrawRectSolid(0.0f, 0.0f, 0.12f, 400.0f, 240.0f, isGap ? theme::AMBER_BG : theme::PARCHMENT);
            C2D_DrawRectSolid(0.0f, 0.0f, 0.2f, 400.0f, 26.0f, isGap ? theme::AMBER_HDR : theme::WALNUT);
            drawText(buf, rv.name, 8.0f, 5.0f, 0.48f, theme::WHITE);
            drawText(buf, rv.subtitle, 10.0f, 32.0f, 0.38f, isGap ? theme::AMBER_HDR : theme::CRIMSON_DARK);
            C2D_DrawRectSolid(8.0f, 48.0f, 0.25f, 384.0f, 1.0f, theme::PARCHMENT_EDGE);

            auto lines = wrapLines(rv.body, 60);
            int st = std::max(0, std::min(app.ruleTopScroll, std::max(0, static_cast<int>(lines.size()) - 1)));
            float cy = 54.0f;
            for (size_t i = static_cast<size_t>(st); i < lines.size() && cy < 228.0f; ++i) {
                drawText(buf, lines[i].c_str(), 10.0f, cy, 0.37f, theme::INK);
                cy += 14.0f;
            }
        }
    }
}

void renderTopTools(C2D_TextBuf buf, const AppState& app) {
    C2D_DrawRectSolid(0.0f, 0.0f, 0.1f, 400.0f, 240.0f, theme::PARCHMENT);
    C2D_DrawRectSolid(0.0f, 0.0f, 0.2f, 400.0f, 26.0f, theme::WALNUT);

    if (app.toolsSubTab == 0) {
        if (app.encBuilderView) {
            drawText(buf, "ENCOUNTER PREP BUILDER", 8.0f, 5.0f, 0.46f, theme::GOLD_BRIGHT);
            drawText(buf, "Add monsters via [+Prep] in Manual. D-Pad selects row.", 10.0f, 32.0f, 0.34f, theme::INK_MUTED);
            if (app.draftEncounter.slots.empty()) {
                drawText(buf, "Draft encounter is empty.", 10.0f, 70.0f, 0.44f, theme::INK_MUTED);
                drawText(buf, "Tap [Manual] -> [+Prep] to add monsters.", 10.0f, 94.0f, 0.38f, theme::FELT);
            } else {
                uint32_t totalXp = 0;
                float cy = 54.0f;
                for (size_t i = 0; i < app.draftEncounter.slots.size() && i < 8; ++i) {
                    const auto& s = app.draftEncounter.slots[i];
                    MonsterView mv{};
                    if (app.db.getMonster(s.monsterId, mv)) {
                        totalXp += mv.xp * static_cast<uint32_t>(s.count);
                        bool sel = (static_cast<int>(i) == app.draftSelectedIdx);
                        char mb[80];
                        std::snprintf(mb, sizeof(mb), "%s%ux %s  (CR %s, %u XP ea)",
                                      sel ? "> " : "  ",
                                      static_cast<unsigned>(s.count), mv.name,
                                      crIndexToString(mv.crIndex), static_cast<unsigned>(mv.xp));
                        drawText(buf, mb, 10.0f, cy, 0.39f, sel ? theme::CRIMSON : theme::INK);
                        cy += 18.0f;
                    }
                }
                char xpBuf[64];
                std::snprintf(xpBuf, sizeof(xpBuf), "Total Base XP: %u  |  Saved Encounters: %zu",
                              static_cast<unsigned>(totalXp), app.campaign.encounters.size());
                drawText(buf, xpBuf, 10.0f, 214.0f, 0.36f, theme::FELT);
            }
        } else {
            drawText(buf, "RANDOM ENCOUNTER GENERATOR", 8.0f, 5.0f, 0.46f, theme::GOLD_BRIGHT);
            char cfg[80];
            std::snprintf(cfg, sizeof(cfg), "Party: Lv %u (%zu Heroes)  |  %s  |  %s",
                          static_cast<unsigned>(app.campaign.partyLevel),
                          app.campaign.party.size(),
                          terrainToString(app.genTerrain),
                          Generators::difficultyToString(app.genDifficulty));
            drawText(buf, cfg, 10.0f, 34.0f, 0.38f, theme::CRIMSON_DARK);

            if (!app.hasGenEncounter) {
                drawText(buf, "Tap [Roll Encounter] below to build a fight!", 10.0f, 70.0f, 0.42f, theme::INK_MUTED);
            } else {
                drawText(buf, app.lastGenEncounter.name.c_str(), 10.0f, 62.0f, 0.46f, theme::INK);
                float cy = 86.0f;
                for (const auto& s : app.lastGenEncounter.slots) {
                    MonsterView mv{};
                    if (app.db.getMonster(s.monsterId, mv)) {
                        char mb[72];
                        std::snprintf(mb, sizeof(mb), "- %ux %s  (CR %s, HP %u, AC %u)",
                                      static_cast<unsigned>(s.count), mv.name,
                                      crIndexToString(mv.crIndex),
                                      static_cast<unsigned>(mv.hitPoints),
                                      static_cast<unsigned>(mv.armorClass));
                        drawText(buf, mb, 14.0f, cy, 0.40f, theme::INK);
                        cy += 18.0f;
                    }
                }
                drawText(buf, "Tap [Send to Fight Tracker] to roll initiative!", 10.0f, 198.0f, 0.36f, theme::FELT);
            }
        }
    } else if (app.toolsSubTab == 1) {
        drawText(buf, "NPC & POST-FIGHT LOOT GENERATORS", 8.0f, 5.0f, 0.46f, theme::GOLD_BRIGHT);
        float cy = 34.0f;
        drawText(buf, "NPC on the Spot:", 10.0f, cy, 0.40f, theme::CRIMSON_DARK); cy += 18.0f;
        if (app.hasGenNpc) {
            char nb[80];
            std::snprintf(nb, sizeof(nb), "%s  (%s %s)",
                          app.lastGenNpc.name.c_str(), app.lastGenNpc.ancestry.c_str(),
                          app.lastGenNpc.occupation.c_str());
            drawText(buf, nb, 14.0f, cy, 0.40f, theme::INK); cy += 16.0f;
            auto qlines = wrapLines(app.lastGenNpc.quirk, 58);
            for (const auto& ln : qlines) {
                drawText(buf, ln.c_str(), 14.0f, cy, 0.36f, theme::INK_MUTED);
                cy += 14.0f;
            }
        } else {
            drawText(buf, "Tap [Roll NPC] below.", 14.0f, cy, 0.38f, theme::INK_MUTED); cy += 18.0f;
        }

        cy = 114.0f;
        C2D_DrawRectSolid(8.0f, cy - 6.0f, 0.25f, 384.0f, 1.0f, theme::PARCHMENT_EDGE);
        drawText(buf, "Treasure & Loot:", 10.0f, cy, 0.40f, theme::CRIMSON_DARK); cy += 18.0f;
        if (app.hasGenLoot) {
            drawText(buf, app.lastGenLoot.summary.c_str(), 14.0f, cy, 0.40f, theme::INK); cy += 17.0f;
            if (!app.lastGenLoot.gemOrArt.empty()) {
                std::string g = "Gem/Art: " + app.lastGenLoot.gemOrArt;
                drawText(buf, g.c_str(), 14.0f, cy, 0.36f, theme::INK); cy += 15.0f;
            }
            for (const auto& it : app.lastGenLoot.items) {
                auto ilines = wrapLines(std::string("- ") + it, 58);
                for (const auto& ln : ilines) {
                    if (cy < 228.0f) {
                        drawText(buf, ln.c_str(), 14.0f, cy, 0.35f, theme::SPELL_BLUE);
                        cy += 14.0f;
                    }
                }
            }
        } else {
            drawText(buf, "Tap [Pocket Loot] or [Hoard Loot] below.", 14.0f, cy, 0.38f, theme::INK_MUTED);
        }
    } else if (app.toolsSubTab == 2) {
        drawText(buf, "ON-DEVICE HOMEBREW CREATOR", 8.0f, 5.0f, 0.46f, theme::GOLD_BRIGHT);
        char typeHdr[64];
        std::snprintf(typeHdr, sizeof(typeHdr), "Draft %s  (Saved to sdmc:/3ds/DM_Toolkit/homebrew/)",
                      app.hbIsSpell ? "Spell" : "Monster");
        drawText(buf, typeHdr, 10.0f, 32.0f, 0.34f, theme::INK_MUTED);

        char nameBuf[64];
        std::snprintf(nameBuf, sizeof(nameBuf), "Name: %.36s", app.hbName.c_str());
        drawText(buf, nameBuf, 10.0f, 54.0f, 0.48f, theme::CRIMSON_DARK);

        if (!app.hbIsSpell) {
            char statBuf[80];
            std::snprintf(statBuf, sizeof(statBuf), "AC: %d   |   HP: %d   |   CR: %d",
                          app.hbAc, app.hbHp, app.hbCr);
            drawText(buf, statBuf, 10.0f, 80.0f, 0.42f, theme::INK);
            char actHdr[64];
            std::snprintf(actHdr, sizeof(actHdr), "Primary Action: %.32s", app.hbActionName.c_str());
            drawText(buf, actHdr, 10.0f, 104.0f, 0.38f, theme::FELT);
        } else {
            char lvlBuf[64];
            std::snprintf(lvlBuf, sizeof(lvlBuf), "Spell Level: %d   |   School: Custom", app.hbSpellLevel);
            drawText(buf, lvlBuf, 10.0f, 80.0f, 0.42f, theme::SPELL_BLUE);
        }

        auto dlines = wrapLines(app.hbDesc, 58);
        float cy = 128.0f;
        for (const auto& ln : dlines) {
            if (cy < 208.0f) {
                drawText(buf, ln.c_str(), 10.0f, cy, 0.36f, theme::INK);
                cy += 14.0f;
            }
        }
        char cntBuf[80];
        std::snprintf(cntBuf, sizeof(cntBuf), "Loaded Custom Entries: %u Monsters (SD overlay active)",
                      static_cast<unsigned>(app.db.homebrewMonsterCount()));
        drawText(buf, cntBuf, 10.0f, 218.0f, 0.33f, theme::INK_MUTED);
    } else if (app.toolsSubTab == 5) {
        drawText(buf, "CREDITS & OPEN CONTENT LICENSE (CC-BY-4.0)", 8.0f, 5.0f, 0.44f, theme::GOLD_BRIGHT);
        const char* legal =
            "This work includes material taken from the System Reference Document 5.1 (\"SRD 5.1\") "
            "by Wizards of the Coast LLC and available at "
            "https://dnd.wizards.com/resources/systems-reference-document. "
            "The SRD 5.1 is licensed under the Creative Commons Attribution 4.0 International License "
            "available at https://creativecommons.org/licenses/by/4.0/legalcode.\n\n"
            "SD Card Homebrew Overlay:\n"
            "Drop custom JSON packs into sdmc:/3ds/DM_Toolkit/homebrew/ or create them in [Brew].";
        auto lines = wrapLines(legal, 58);
        float cy = 36.0f;
        for (const auto& ln : lines) {
            drawText(buf, ln.c_str(), 10.0f, cy, 0.37f, theme::INK);
            cy += 14.0f;
        }
    } else {
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
        std::snprintf(stats, sizeof(stats), "SRD: %u Monsters, %u Spells, %u Refs | %zu Campaigns",
                      static_cast<unsigned>(app.db.monsterCount()),
                      static_cast<unsigned>(app.db.spellCount()),
                      static_cast<unsigned>(app.db.refCount()),
                      app.campaigns.size());
        drawText(buf, stats, 10.0f, 218.0f, 0.33f, theme::INK_MUTED);
    }
}

// ── Sprite Atlas Indices (matches gfx/sprites.t3s order) ──────────────

enum SpriteIdx : size_t {
    SPR_TITLE_BG    = 0,
    SPR_ICON_FIGHT  = 1,
    SPR_ICON_MANUAL = 2,
    SPR_ICON_DICE   = 3,
    SPR_ICON_RULES  = 4,
    SPR_ICON_TOOLS  = 5
};

void drawSprite(C2D_SpriteSheet sheet, size_t idx, float x, float y,
                float z = 0.5f, float scaleX = 1.0f, float scaleY = 1.0f) {
    if (sheet && idx < C2D_SpriteSheetCount(sheet)) {
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(sheet, idx), x, y, z, nullptr, scaleX, scaleY);
    }
}

void renderTopTitleSplash(C2D_TextBuf buf, C2D_SpriteSheet sheet) {
    drawSprite(sheet, SPR_TITLE_BG, 0.0f, 0.0f, 0.1f, 1.0f, 1.0f);
    // Subtle dark translucent banner near top center for crisp title legibility
    C2D_DrawRectSolid(0.0f, 8.0f, 0.35f, 400.0f, 52.0f, rgba32(18, 12, 8, 185));
    C2D_DrawRectSolid(40.0f, 8.0f, 0.40f, 320.0f, 2.0f, theme::GOLD);
    C2D_DrawRectSolid(40.0f, 58.0f, 0.40f, 320.0f, 2.0f, theme::GOLD);
    drawTextCentered(buf, "DM's TOOLKIT", Rect{0.0f, 12.0f, 400.0f, 28.0f}, 0.82f, theme::GOLD_BRIGHT, 0.55f);
    drawTextCentered(buf, "Pocket DM Screen, Fight Tracker & 3D Dice Tray",
                     Rect{0.0f, 38.0f, 400.0f, 18.0f}, 0.38f, theme::PARCHMENT, 0.55f);
}

void renderBottomTitleSplash(C2D_TextBuf buf, C2D_SpriteSheet sheet, const AppState& app) {
    C2D_DrawRectSolid(0.0f, 0.0f, 0.05f, 320.0f, 240.0f, theme::WALNUT_DARK);
    C2D_DrawRectSolid(10.0f, 10.0f, 0.10f, 300.0f, 220.0f, theme::WALNUT);
    C2D_DrawRectSolid(12.0f, 12.0f, 0.12f, 296.0f, 216.0f, theme::GOLD_DARK);
    C2D_DrawRectSolid(14.0f, 14.0f, 0.15f, 292.0f, 212.0f, theme::FELT);

    // 5 golden emblem icons row
    for (int i = 0; i < 5; ++i) {
        drawSprite(sheet, SPR_ICON_FIGHT + static_cast<size_t>(i), 48.0f + i * 50.0f, 28.0f, 0.45f, 1.2f, 1.2f);
    }

    char campBuf[64];
    std::snprintf(campBuf, sizeof(campBuf), "Active Campaign: %.24s", app.campaign.name.c_str());
    drawTextCentered(buf, campBuf, Rect{14.0f, 68.0f, 292.0f, 22.0f}, 0.44f, theme::GOLD_BRIGHT, 0.55f);

    char statBuf[80];
    std::snprintf(statBuf, sizeof(statBuf), "%u Monsters  |  %u Spells  |  %u Entries",
                  static_cast<unsigned>(app.db.monsterCount()),
                  static_cast<unsigned>(app.db.spellCount()),
                  static_cast<unsigned>(app.db.searchEntryCount()));
    drawTextCentered(buf, statBuf, Rect{14.0f, 94.0f, 292.0f, 20.0f}, 0.36f, theme::PARCHMENT, 0.55f);

    drawButton(buf, Rect{36.0f, 130.0f, 248.0f, 52.0f}, "ENTER DM SCREEN  [A]",
               theme::GOLD, theme::WALNUT_DARK, theme::INK, 0.50f);

    drawTextCentered(buf, "Tap anywhere or press any button to begin",
                     Rect{14.0f, 194.0f, 292.0f, 20.0f}, 0.34f, theme::PARCHMENT_ALT, 0.55f);
}

// ── Bottom Screen Renderers (320x240) ─────────────────────────────────

void drawModeTabs(C2D_TextBuf buf, C2D_SpriteSheet sheet, AppMode active) {
    const char* labels[5] = {"Fight", "Manual", "Dice", "Rules", "Tools"};
    for (int i = 0; i < 5; ++i) {
        Rect r{i * 64.0f, 0.0f, 64.0f, 28.0f};
        bool sel = (static_cast<int>(active) == i);
        C2D_DrawRectSolid(r.x, r.y, 0.30f, r.w, r.h, theme::WALNUT_DARK);
        C2D_DrawRectSolid(r.x + 1.0f, r.y + 1.0f, 0.35f, r.w - 2.0f, r.h - 2.0f,
                          sel ? theme::GOLD : theme::WALNUT);
        drawSprite(sheet, SPR_ICON_FIGHT + static_cast<size_t>(i),
                   r.x + 4.0f, r.y + 6.0f, 0.50f, 0.80f, 0.80f);
        drawText(buf, labels[i], r.x + 22.0f, r.y + 8.0f, 0.34f,
                 sel ? theme::INK : theme::PARCHMENT, 0.55f);
    }
}

void renderBottomFight(C2D_TextBuf buf, const AppState& app) {
    if (!app.tracker.isInCombat()) {
        drawText(buf, "Saved Encounters (Tap to Roll Initiative):", 8.0f, 34.0f, 0.38f, theme::PARCHMENT);
        float y = 54.0f;
        for (size_t i = 0; i < app.campaign.encounters.size() && i < 3; ++i) {
            drawButton(buf, Rect{8.0f, y, 304.0f, 38.0f}, app.campaign.encounters[i].name.c_str(),
                       theme::FELT_LIGHT, theme::GOLD, theme::WHITE, 0.44f);
            y += 44.0f;
        }
        drawButton(buf, Rect{8.0f, 190.0f, 148.0f, 40.0f}, "+ Pick Monster", theme::GOLD, theme::WALNUT_DARK, theme::INK, 0.42f);
        drawButton(buf, Rect{164.0f, 190.0f, 148.0f, 40.0f}, "Random Fight", theme::SPELL_BLUE, theme::GOLD, theme::WHITE, 0.42f);
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
        drawText(buf, "End this fight and clear the tracker?", 20.0f, 58.0f, 0.46f, theme::PARCHMENT);
        drawText(buf, "(Your campaign & party stay saved.)", 36.0f, 82.0f, 0.38f, theme::PARCHMENT_ALT);
        drawButton(buf, Rect{12.0f, 118.0f, 94.0f, 46.0f}, "Cancel", theme::WALNUT_LIGHT, theme::GOLD_DARK, theme::WHITE, 0.42f);
        drawButton(buf, Rect{113.0f, 118.0f, 94.0f, 46.0f}, "Roll Loot", theme::GOLD, theme::WALNUT_DARK, theme::INK, 0.42f);
        drawButton(buf, Rect{214.0f, 118.0f, 94.0f, 46.0f}, "End Fight", theme::CRIMSON, theme::GOLD, theme::WHITE, 0.42f);
        return;
    }

    if (app.fightPanel == FIGHT_PANEL_CUSTOM_HP) {
        const Combatant& c = app.tracker.combatants()[static_cast<size_t>(app.tracker.selectedIndex())];
        char hdr[64];
        std::snprintf(hdr, sizeof(hdr), "Custom HP for %.18s (%d/%d)", c.displayName.c_str(), c.currentHp, c.maxHp);
        drawText(buf, hdr, 8.0f, 32.0f, 0.38f, theme::GOLD_BRIGHT);

        // 3x4 keypad on left (x = 6..184, y = 50..232)
        const char* keys[12] = {"1","2","3","4","5","6","7","8","9","Clr","0","Back"};
        for (int i = 0; i < 12; ++i) {
            int col = i % 3, row = i / 3;
            Rect r{6.0f + col * 60.0f, 50.0f + row * 46.0f, 56.0f, 42.0f};
            uint32_t bg = (i == 9) ? theme::CRIMSON_DARK : (i == 11 ? theme::WALNUT_LIGHT : theme::PARCHMENT);
            uint32_t fg = (i == 9 || i == 11) ? theme::WHITE : theme::INK;
            drawButton(buf, r, keys[i], bg, theme::GOLD_DARK, fg, 0.46f);
        }

        // Right column: typed value display + Apply Damage / Apply Heal
        char valBuf[16];
        std::snprintf(valBuf, sizeof(valBuf), "%d", app.customHpValue);
        drawButton(buf, Rect{194.0f, 50.0f, 120.0f, 46.0f}, valBuf, theme::WALNUT_DARK, theme::GOLD, theme::GOLD_BRIGHT, 0.72f);
        drawButton(buf, Rect{194.0f, 106.0f, 120.0f, 54.0f}, "- Damage", theme::CRIMSON, theme::GOLD, theme::WHITE, 0.46f);
        drawButton(buf, Rect{194.0f, 170.0f, 120.0f, 54.0f}, "+ Heal", theme::HP_GOOD, theme::GOLD, theme::WHITE, 0.46f);
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

    drawButton(buf, Rect{6.0f,   52.0f, 58.0f, 44.0f}, "-10", theme::CRIMSON, theme::GOLD_DARK, theme::WHITE, 0.48f);
    drawButton(buf, Rect{68.0f,  52.0f, 58.0f, 44.0f}, "-5",  theme::CRIMSON, theme::GOLD_DARK, theme::WHITE, 0.48f);
    drawButton(buf, Rect{130.0f, 52.0f, 58.0f, 44.0f}, "-1",  theme::CRIMSON_DARK, theme::GOLD_DARK, theme::WHITE, 0.48f);
    drawButton(buf, Rect{194.0f, 52.0f, 58.0f, 44.0f}, "+1",  theme::HP_GOOD, theme::GOLD_DARK, theme::WHITE, 0.48f);
    drawButton(buf, Rect{256.0f, 52.0f, 58.0f, 44.0f}, "+5",  theme::HP_GOOD, theme::GOLD_DARK, theme::WHITE, 0.48f);

    drawButton(buf, Rect{6.0f,   102.0f, 96.0f, 38.0f}, "Conditions", theme::COND_BADGE, theme::GOLD_DARK, theme::WHITE, 0.38f);
    drawButton(buf, Rect{106.0f, 102.0f, 50.0f, 38.0f}, "Init-", theme::WALNUT_LIGHT, theme::GOLD_DARK, theme::PARCHMENT, 0.36f);
    drawButton(buf, Rect{160.0f, 102.0f, 50.0f, 38.0f}, "Init+", theme::WALNUT_LIGHT, theme::GOLD_DARK, theme::PARCHMENT, 0.36f);
    drawButton(buf, Rect{214.0f, 102.0f, 100.0f, 38.0f},
               app.combatRulesDrawerOpen ? "Hide Rules" : "Combat Rules",
               theme::FELT_LIGHT, theme::GOLD, theme::WHITE, 0.36f);

    drawButton(buf, Rect{6.0f,   148.0f, 184.0f, 46.0f}, "NEXT TURN  [A/R]", theme::GOLD, theme::WALNUT_DARK, theme::INK, 0.46f);
    drawButton(buf, Rect{196.0f, 148.0f, 118.0f, 46.0f}, "+ Monster", theme::FELT, theme::GOLD, theme::WHITE, 0.42f);

    drawButton(buf, Rect{6.0f,   200.0f, 68.0f, 34.0f}, "Prev", theme::WALNUT_LIGHT, theme::WALNUT_DARK, theme::PARCHMENT, 0.36f);
    drawButton(buf, Rect{78.0f,  200.0f, 84.0f, 34.0f}, "Custom HP", theme::WALNUT_LIGHT, theme::GOLD_DARK, theme::GOLD_BRIGHT, 0.35f);
    drawButton(buf, Rect{166.0f, 200.0f, 76.0f, 34.0f}, "Roll Atk", theme::SPELL_BLUE, theme::GOLD, theme::WHITE, 0.35f);
    drawButton(buf, Rect{246.0f, 200.0f, 68.0f, 34.0f}, "End", theme::CRIMSON_DARK, theme::GOLD_DARK, theme::WHITE, 0.36f);
}

void renderBottomManual(C2D_TextBuf buf, const AppState& app) {
    C2D_DrawRectSolid(6.0f, 32.0f, 0.25f, 148.0f, 24.0f, theme::PARCHMENT);
    char qBuf[40];
    std::snprintf(qBuf, sizeof(qBuf), "Search: %s_", app.monsterQuery.c_str());
    drawText(buf, qBuf, 10.0f, 36.0f, 0.38f, theme::INK);

    const char* crLbl[CR_FILTER_COUNT] = {"CR:All", "CR:0-1", "CR:2-4", "CR:5-10", "CR:11+"};
    const char* tpLbl = (app.monsterTypeFilter < 0) ? "Type:All" : monsterTypeToString(static_cast<uint8_t>(app.monsterTypeFilter));
    drawButton(buf, Rect{158.0f, 32.0f, 66.0f, 24.0f}, tpLbl, theme::WALNUT_LIGHT, theme::GOLD_DARK, theme::PARCHMENT, 0.32f);
    drawButton(buf, Rect{226.0f, 32.0f, 54.0f, 24.0f}, crLbl[app.monsterCrFilter], theme::WALNUT_LIGHT, theme::GOLD_DARK, theme::PARCHMENT, 0.32f);
    drawButton(buf, Rect{282.0f, 32.0f, 34.0f, 24.0f}, "Clr", theme::CRIMSON_DARK, theme::GOLD_DARK, theme::WHITE, 0.34f);

    for (int i = 0; i < 3; ++i) {
        int idx = app.monsterListScroll + i;
        if (idx >= static_cast<int>(app.monsterResults.size())) break;
        const SearchHit& h = app.monsterResults[static_cast<size_t>(idx)];
        MonsterView mv{};
        if (!app.db.getMonster(h.recordId, mv)) continue;

        float ry = 60.0f + i * 33.0f;
        bool sel = (idx == app.monsterListCursor);
        C2D_DrawRectSolid(6.0f, ry, 0.2f, 204.0f, 30.0f, sel ? theme::GOLD : theme::WALNUT_LIGHT);
        C2D_DrawRectSolid(7.0f, ry + 1.0f, 0.25f, 202.0f, 28.0f, sel ? theme::ACTIVE_ROW : theme::PARCHMENT);

        char rowLbl[40];
        std::snprintf(rowLbl, sizeof(rowLbl), "%.17s%s", mv.name, mv.isHomebrew ? " [HB]" : "");
        drawText(buf, rowLbl, 11.0f, ry + 2.0f, 0.38f, theme::INK);

        char subLbl[40];
        std::snprintf(subLbl, sizeof(subLbl), "%s | CR %s | HP %u",
                      monsterTypeToString(mv.type), crIndexToString(mv.crIndex), static_cast<unsigned>(mv.hitPoints));
        drawText(buf, subLbl, 11.0f, ry + 16.0f, 0.30f, theme::INK_MUTED);

        drawButton(buf, Rect{214.0f, ry, 46.0f, 30.0f}, "+Prep", theme::SPELL_BLUE, theme::GOLD_DARK, theme::WHITE, 0.33f);
        drawButton(buf, Rect{262.0f, ry, 52.0f, 30.0f}, "+Fgt", theme::FELT_LIGHT, theme::GOLD, theme::WHITE, 0.35f);
    }

    drawQwertyKeypad(buf);
}

void renderBottomDice(C2D_TextBuf buf, const AppState& app) {
    // Row 1: 7 die buttons (y = 34..74)
    for (int i = 0; i < DIE_SHAPE_COUNT; ++i) {
        Rect r{4.0f + i * 44.5f, 34.0f, 42.0f, 38.0f};
        bool sel = (static_cast<int>(app.diceShape) == i);
        drawButton(buf, r, DiceRoller::shapeLabel(static_cast<DieShape>(i)),
                   sel ? theme::GOLD : theme::WALNUT_LIGHT,
                   sel ? theme::GOLD_BRIGHT : theme::WALNUT_DARK,
                   sel ? theme::INK : theme::PARCHMENT, 0.40f);
    }

    // Row 2: Modifier & Advantage/Disadvantage (y = 80..118)
    char modLbl[24];
    std::snprintf(modLbl, sizeof(modLbl), "Mod %+d", app.diceMod);
    drawButton(buf, Rect{6.0f,   80.0f, 44.0f, 36.0f}, "-1", theme::CRIMSON_DARK, theme::GOLD_DARK, theme::WHITE, 0.44f);
    drawButton(buf, Rect{54.0f,  80.0f, 76.0f, 36.0f}, modLbl, theme::WALNUT_DARK, theme::GOLD_DARK, theme::GOLD_BRIGHT, 0.40f);
    drawButton(buf, Rect{134.0f, 80.0f, 44.0f, 36.0f}, "+1", theme::HP_GOOD, theme::GOLD_DARK, theme::WHITE, 0.44f);

    const char* advStr = (app.diceAdvMode == ROLL_ADVANTAGE) ? "Advantage"
                       : (app.diceAdvMode == ROLL_DISADVANTAGE) ? "Disadv."
                       : "Normal";
    uint32_t advCol = (app.diceAdvMode == ROLL_ADVANTAGE) ? theme::HP_GOOD
                    : (app.diceAdvMode == ROLL_DISADVANTAGE) ? theme::CRIMSON
                    : theme::WALNUT_LIGHT;
    drawButton(buf, Rect{186.0f, 80.0f, 128.0f, 36.0f}, advStr, advCol, theme::GOLD, theme::WHITE, 0.40f);

    // Row 3: Huge ROLL button (y = 124..176)
    char rollLbl[40];
    std::snprintf(rollLbl, sizeof(rollLbl), "ROLL %s  [A]", DiceRoller::shapeLabel(app.diceShape));
    drawButton(buf, Rect{6.0f, 124.0f, 308.0f, 52.0f}, rollLbl, theme::GOLD, theme::WALNUT_DARK, theme::INK, 0.54f);

    // Row 4: Dice Skin & Tray Skin customizer pills (y = 184..232)
    char dSkinLbl[40], tSkinLbl[40];
    std::snprintf(dSkinLbl, sizeof(dSkinLbl), "Die: %s", getDiceSkin(app.diceSkinIdx).name);
    std::snprintf(tSkinLbl, sizeof(tSkinLbl), "Tray: %s", getTraySkin(app.traySkinIdx).name);
    drawButton(buf, Rect{6.0f,   184.0f, 150.0f, 44.0f}, dSkinLbl, theme::WALNUT_LIGHT, theme::GOLD, theme::GOLD_BRIGHT, 0.35f);
    drawButton(buf, Rect{164.0f, 184.0f, 150.0f, 44.0f}, tSkinLbl, theme::FELT, theme::GOLD, theme::WHITE, 0.35f);
}

void renderBottomRules(C2D_TextBuf buf, const AppState& app) {
    // Sub-tabs (y = 30..50)
    const char* tabs[REF_TAB_COUNT] = {"All", "Spells", "Combat", "Cls/Race", "Gear"};
    for (int i = 0; i < REF_TAB_COUNT; ++i) {
        Rect r{4.0f + i * 62.5f, 30.0f, 60.0f, 20.0f};
        bool sel = (static_cast<int>(app.refTab) == i);
        drawButton(buf, r, tabs[i], sel ? theme::GOLD : theme::WALNUT_LIGHT, theme::WALNUT_DARK,
                   sel ? theme::INK : theme::PARCHMENT, 0.33f);
    }

    // Search bar + Clear (y = 53..74)
    C2D_DrawRectSolid(6.0f, 53.0f, 0.25f, 266.0f, 21.0f, theme::PARCHMENT);
    char qBuf[44];
    std::snprintf(qBuf, sizeof(qBuf), "Find: %s_", app.ruleQuery.c_str());
    drawText(buf, qBuf, 10.0f, 56.0f, 0.36f, theme::INK);
    drawButton(buf, Rect{276.0f, 53.0f, 38.0f, 21.0f}, "Clr", theme::CRIMSON_DARK, theme::GOLD_DARK, theme::WHITE, 0.33f);

    // 3 visible reference rows (y = 77..160)
    for (int i = 0; i < 3; ++i) {
        int idx = app.ruleListScroll + i;
        if (idx >= static_cast<int>(app.ruleResults.size())) break;
        const SearchHit& h = app.ruleResults[static_cast<size_t>(idx)];
        float ry = 77.0f + i * 28.0f;
        bool sel = (idx == app.ruleListCursor);

        const char* title = "";
        const char* badge = "";
        uint32_t badgeCol = theme::INK_MUTED;
        if (h.category == CAT_SPELL) {
            SpellView sv{};
            if (app.db.getSpell(h.recordId, sv)) { title = sv.name; badge = sv.isHomebrew ? "Spell [HB]" : "Spell"; badgeCol = theme::SPELL_BLUE; }
        } else if (h.category == CAT_RULE || h.category == CAT_CONDITION) {
            RuleView rv{};
            if (app.db.getRule(h.recordId, rv)) { title = rv.name; badge = rv.category; badgeCol = rv.isCondition ? theme::COND_BADGE : theme::FELT; }
        } else {
            ReferenceView rv{};
            if (app.db.getReference(h.recordId, rv)) {
                title = rv.name;
                badge = (rv.category == CAT_SRD_GAP) ? (rv.subType == GAP_ALIAS ? "SRD Alias" : "Non-SRD") : rv.subtitle;
                badgeCol = (rv.category == CAT_SRD_GAP) ? theme::AMBER_HDR : theme::WALNUT_LIGHT;
            }
        }

        C2D_DrawRectSolid(6.0f, ry, 0.2f, 308.0f, 26.0f, sel ? theme::GOLD : theme::WALNUT_LIGHT);
        C2D_DrawRectSolid(7.0f, ry + 1.0f, 0.25f, 306.0f, 24.0f, sel ? theme::ACTIVE_ROW : theme::PARCHMENT);
        char tBuf[28];
        std::snprintf(tBuf, sizeof(tBuf), "%.24s", title);
        drawText(buf, tBuf, 12.0f, ry + 4.0f, 0.37f, theme::INK);
        char bBuf[20];
        std::snprintf(bBuf, sizeof(bBuf), "%.16s", badge);
        drawText(buf, bBuf, 210.0f, ry + 5.0f, 0.31f, badgeCol);
    }

    drawQwertyKeypad(buf);
}

void renderBottomTools(C2D_TextBuf buf, const AppState& app) {
    const char* sub[6] = {"Enc", "Gen", "Brew", "Party", "Saves", "Legal"};
    for (int i = 0; i < 6; ++i) {
        Rect r{4.0f + i * 52.0f, 32.0f, 50.0f, 26.0f};
        bool sel = (app.toolsSubTab == i);
        drawButton(buf, r, sub[i], sel ? theme::GOLD : theme::WALNUT_LIGHT, theme::WALNUT_DARK,
                   sel ? theme::INK : theme::PARCHMENT, 0.33f);
    }

    if (app.toolsSubTab == 0) {
        drawButton(buf, Rect{6.0f,   62.0f, 150.0f, 26.0f}, "Random Gen",
                   !app.encBuilderView ? theme::GOLD : theme::WALNUT_DARK, theme::GOLD_DARK,
                   !app.encBuilderView ? theme::INK : theme::PARCHMENT, 0.36f);
        drawButton(buf, Rect{164.0f, 62.0f, 150.0f, 26.0f}, "Prep Builder",
                   app.encBuilderView ? theme::GOLD : theme::WALNUT_DARK, theme::GOLD_DARK,
                   app.encBuilderView ? theme::INK : theme::PARCHMENT, 0.36f);

        if (app.encBuilderView) {
            drawButton(buf, Rect{6.0f,   94.0f, 98.0f, 38.0f}, "Qty -1", theme::CRIMSON_DARK, theme::GOLD_DARK, theme::WHITE, 0.38f);
            drawButton(buf, Rect{111.0f, 94.0f, 98.0f, 38.0f}, "Qty +1", theme::HP_GOOD, theme::GOLD_DARK, theme::WHITE, 0.38f);
            drawButton(buf, Rect{216.0f, 94.0f, 98.0f, 38.0f}, "Clear", theme::WALNUT_LIGHT, theme::GOLD_DARK, theme::PARCHMENT, 0.38f);
            drawButton(buf, Rect{6.0f,  138.0f, 308.0f, 42.0f}, "Save Encounter to Campaign",
                       app.draftEncounter.slots.empty() ? theme::WALNUT_DARK : theme::GOLD,
                       theme::WALNUT_DARK,
                       app.draftEncounter.slots.empty() ? theme::INK_MUTED : theme::INK, 0.42f);
            drawButton(buf, Rect{6.0f,  186.0f, 308.0f, 42.0f}, "Start Draft in Fight Tracker",
                       app.draftEncounter.slots.empty() ? theme::WALNUT_DARK : theme::FELT_LIGHT,
                       theme::GOLD, theme::WHITE, 0.42f);
        } else {
            char tLbl[32], dLbl[32];
            std::snprintf(tLbl, sizeof(tLbl), "Terrain: %s", terrainToString(app.genTerrain));
            std::snprintf(dLbl, sizeof(dLbl), "Diff: %s", Generators::difficultyToString(app.genDifficulty));
            drawButton(buf, Rect{6.0f,   94.0f, 150.0f, 38.0f}, tLbl, theme::WALNUT_LIGHT, theme::GOLD_DARK, theme::PARCHMENT, 0.37f);
            drawButton(buf, Rect{164.0f, 94.0f, 150.0f, 38.0f}, dLbl, theme::WALNUT_LIGHT, theme::GOLD_DARK, theme::PARCHMENT, 0.37f);
            drawButton(buf, Rect{6.0f,  138.0f, 308.0f, 42.0f}, "Roll Random Encounter", theme::GOLD, theme::WALNUT_DARK, theme::INK, 0.44f);
            drawButton(buf, Rect{6.0f,  186.0f, 308.0f, 42.0f}, "Send to Fight Tracker",
                       app.hasGenEncounter ? theme::FELT_LIGHT : theme::WALNUT_DARK, theme::GOLD, theme::WHITE, 0.42f);
        }
    } else if (app.toolsSubTab == 1) {
        drawButton(buf, Rect{6.0f,   70.0f, 308.0f, 48.0f}, "Roll Random NPC", theme::GOLD, theme::WALNUT_DARK, theme::INK, 0.46f);
        drawButton(buf, Rect{6.0f,  130.0f, 150.0f, 48.0f}, "Pocket Loot", theme::FELT_LIGHT, theme::GOLD, theme::WHITE, 0.42f);
        drawButton(buf, Rect{164.0f,130.0f, 150.0f, 48.0f}, "Hoard Loot", theme::SPELL_BLUE, theme::GOLD, theme::WHITE, 0.42f);
    } else if (app.toolsSubTab == 2) {
        drawButton(buf, Rect{6.0f,   62.0f, 150.0f, 26.0f}, "Custom Monster",
                   !app.hbIsSpell ? theme::GOLD : theme::WALNUT_DARK, theme::GOLD_DARK,
                   !app.hbIsSpell ? theme::INK : theme::PARCHMENT, 0.36f);
        drawButton(buf, Rect{164.0f, 62.0f, 150.0f, 26.0f}, "Custom Spell",
                   app.hbIsSpell ? theme::GOLD : theme::WALNUT_DARK, theme::GOLD_DARK,
                   app.hbIsSpell ? theme::INK : theme::PARCHMENT, 0.36f);

        drawButton(buf, Rect{6.0f,   92.0f, 150.0f, 34.0f}, "Edit Name", theme::WALNUT_LIGHT, theme::GOLD_DARK, theme::PARCHMENT, 0.38f);
        drawButton(buf, Rect{164.0f, 92.0f, 150.0f, 34.0f}, "Edit Desc", theme::WALNUT_LIGHT, theme::GOLD_DARK, theme::PARCHMENT, 0.38f);

        if (!app.hbIsSpell) {
            drawButton(buf, Rect{6.0f,   132.0f, 48.0f, 36.0f}, "AC-", theme::CRIMSON_DARK, theme::GOLD_DARK, theme::WHITE, 0.34f);
            drawButton(buf, Rect{56.0f,  132.0f, 48.0f, 36.0f}, "AC+", theme::HP_GOOD, theme::GOLD_DARK, theme::WHITE, 0.34f);
            drawButton(buf, Rect{110.0f, 132.0f, 48.0f, 36.0f}, "HP-5", theme::CRIMSON_DARK, theme::GOLD_DARK, theme::WHITE, 0.33f);
            drawButton(buf, Rect{160.0f, 132.0f, 48.0f, 36.0f}, "HP+5", theme::HP_GOOD, theme::GOLD_DARK, theme::WHITE, 0.33f);
            drawButton(buf, Rect{214.0f, 132.0f, 48.0f, 36.0f}, "CR-", theme::WALNUT_LIGHT, theme::GOLD_DARK, theme::PARCHMENT, 0.34f);
            drawButton(buf, Rect{264.0f, 132.0f, 50.0f, 36.0f}, "CR+", theme::WALNUT_LIGHT, theme::GOLD_DARK, theme::PARCHMENT, 0.34f);
        } else {
            drawButton(buf, Rect{6.0f,   132.0f, 150.0f, 36.0f}, "Spell Lv -1", theme::CRIMSON_DARK, theme::GOLD_DARK, theme::WHITE, 0.38f);
            drawButton(buf, Rect{164.0f, 132.0f, 150.0f, 36.0f}, "Spell Lv +1", theme::HP_GOOD, theme::GOLD_DARK, theme::WHITE, 0.38f);
        }

        drawButton(buf, Rect{6.0f, 176.0f, 308.0f, 46.0f}, "Save Homebrew to SD & Reload",
                   theme::GOLD, theme::WALNUT_DARK, theme::INK, 0.44f);
    } else if (app.toolsSubTab == 3) {
        drawButton(buf, Rect{6.0f,   72.0f, 148.0f, 38.0f}, "+ Add Hero", theme::FELT_LIGHT, theme::GOLD, theme::WHITE, 0.42f);
        drawButton(buf, Rect{166.0f, 72.0f, 148.0f, 38.0f}, "Rename Hero", theme::WALNUT_LIGHT, theme::GOLD_DARK, theme::PARCHMENT, 0.40f);
        drawButton(buf, Rect{6.0f,  118.0f, 72.0f,  38.0f}, "Init -1", theme::CRIMSON_DARK, theme::GOLD_DARK, theme::WHITE, 0.40f);
        drawButton(buf, Rect{82.0f, 118.0f, 72.0f,  38.0f}, "Init +1", theme::HP_GOOD, theme::GOLD_DARK, theme::WHITE, 0.40f);
        drawButton(buf, Rect{166.0f,118.0f, 72.0f,  38.0f}, "Lv -1", theme::WALNUT_LIGHT, theme::GOLD_DARK, theme::PARCHMENT, 0.40f);
        drawButton(buf, Rect{242.0f,118.0f, 72.0f,  38.0f}, "Lv +1", theme::WALNUT_LIGHT, theme::GOLD_DARK, theme::PARCHMENT, 0.40f);
        drawButton(buf, Rect{6.0f,  166.0f, 308.0f, 38.0f}, "Save Campaign to SD", theme::GOLD, theme::WALNUT_DARK, theme::INK, 0.44f);
    } else if (app.toolsSubTab == 4) {
        float y = 64.0f;
        for (size_t i = 0; i < app.campaigns.size() && i < 3; ++i) {
            bool active = (app.campaigns[i].slug == app.campaign.slug);
            drawButton(buf, Rect{6.0f, y, 308.0f, 32.0f}, app.campaigns[i].name.c_str(),
                       active ? theme::FELT_LIGHT : theme::WALNUT_LIGHT,
                       active ? theme::GOLD : theme::WALNUT_DARK, theme::WHITE, 0.40f);
            y += 35.0f;
        }
        drawButton(buf, Rect{6.0f,   174.0f, 180.0f, 40.0f}, "+ New Campaign", theme::GOLD, theme::WALNUT_DARK, theme::INK, 0.40f);
        drawButton(buf, Rect{192.0f, 174.0f, 122.0f, 40.0f},
                   app.soundEnabled ? "Sound: ON" : "Sound: OFF",
                   app.soundEnabled ? theme::HP_GOOD : theme::CRIMSON_DARK,
                   theme::GOLD, theme::WHITE, 0.38f);
    } else {
        drawText(buf, "DM's Toolkit v1.2", 10.0f, 80.0f, 0.48f, theme::GOLD_BRIGHT);
        drawText(buf, "SRD 5.1 under CC-BY-4.0", 10.0f, 108.0f, 0.40f, theme::PARCHMENT);
        drawText(buf, "See top screen for full legal notice.", 10.0f, 132.0f, 0.36f, theme::PARCHMENT_ALT);
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
    C2D_SpriteSheet spriteSheet = C2D_SpriteSheetLoad("romfs:/gfx/sprites.t3x");

    AppState app;
    app.sfx.init();
    app.rngState = static_cast<uint32_t>(svcGetSystemTick() ^ 0xA5A55A5Au);
    app.dice.roll(DIE_D20, 0, ROLL_NORMAL, app.rngState);
    app.dice.finishImmediately();

    if (app.db.loadFromFile("romfs:/data/srd.bin")) {
        app.campaignMgr.ensureDefaultCampaign(app.db);
        int hbLoaded = app.db.loadHomebrewDir(app.campaignMgr.homebrewDir().c_str());
        app.campaigns = app.campaignMgr.listCampaigns();

        UserSettings savedSettings;
        if (app.campaignMgr.loadSettings(savedSettings)) {
            app.diceSkinIdx  = savedSettings.diceSkinIdx % DICE_SKIN_COUNT;
            app.traySkinIdx  = savedSettings.traySkinIdx % TRAY_SKIN_COUNT;
            app.soundEnabled = savedSettings.soundEnabled;
            app.sfx.setEnabled(app.soundEnabled);
        }

        std::string targetSlug;
        if (!savedSettings.lastCampaignSlug.empty()) {
            for (const auto& c : app.campaigns) {
                if (c.slug == savedSettings.lastCampaignSlug) {
                    targetSlug = c.slug;
                    break;
                }
            }
        }
        if (targetSlug.empty() && !app.campaigns.empty()) {
            targetSlug = app.campaigns[0].slug;
        }
        if (!targetSlug.empty()) {
            app.campaignMgr.loadCampaign(targetSlug, app.campaign, &app.tracker);
        }

        app.refreshMonsterSearch();
        app.refreshRuleSearch();
        if (hbLoaded > 0) app.setStatus("Loaded SRD + SD homebrew entries.");
    } else {
        app.setStatus("Error: romfs:/data/srd.bin missing!", 300);
    }

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        if (kDown & KEY_START) break;

        bool rollingNow = app.dice.isRolling();
        app.dice.step(1.0f / 60.0f);
        if (rollingNow && !app.dice.isRolling() && app.dice.currentShape() == DIE_D20) {
            int kept = app.dice.dieA().result;
            if (app.dice.dieB().active) {
                kept = (app.dice.currentAdvMode() == ROLL_ADVANTAGE)
                           ? std::max(app.dice.dieA().result, app.dice.dieB().result)
                           : std::min(app.dice.dieA().result, app.dice.dieB().result);
            }
            if (kept == 20) app.sfx.play(SFX_NAT20);
            else if (kept == 1) app.sfx.play(SFX_NAT1);
        }

        touchPosition touch{};
        bool tapped = (kDown & KEY_TOUCH) != 0;
        if (tapped) hidTouchRead(&touch);
        float tx = static_cast<float>(touch.px);
        float ty = static_cast<float>(touch.py);

        if (app.showTitleSplash) {
            if (tapped || (kDown & (KEY_A | KEY_B | KEY_X | KEY_Y))) {
                app.showTitleSplash = false;
                app.sfx.play(SFX_TAP);
            }
        } else if (tapped && ty <= 28.0f) {
            int tab = std::clamp(static_cast<int>(tx / 64.0f), 0, 4);
            app.mode = static_cast<AppMode>(tab);
            app.fightPanel = FIGHT_PANEL_MAIN;
            app.sfx.play(SFX_TAP);
        } else if (app.mode == MODE_FIGHT) {
            if (kDown & KEY_DUP && app.tracker.isInCombat()) {
                app.tracker.setSelectedIndex(app.tracker.selectedIndex() - 1);
                app.trackerRightScroll = 0;
            } else if (kDown & KEY_DDOWN && app.tracker.isInCombat()) {
                app.tracker.setSelectedIndex(app.tracker.selectedIndex() + 1);
                app.trackerRightScroll = 0;
            } else if (((kDown & KEY_A) || ((kDown & KEY_R) && !app.combatRulesDrawerOpen)) && app.tracker.isInCombat()) {
                app.tracker.nextTurn();
                app.trackerRightScroll = 0;
                app.sfx.play(SFX_TAP);
            } else if (kDown & KEY_B) {
                if (app.combatRulesDrawerOpen) app.combatRulesDrawerOpen = false;
                else if (app.fightPanel != FIGHT_PANEL_MAIN) app.fightPanel = FIGHT_PANEL_MAIN;
            } else if (app.combatRulesDrawerOpen && (kDown & KEY_R)) {
                app.drawerRuleIndex = (app.drawerRuleIndex + 1) % std::max<uint32_t>(1, app.db.ruleCount());
                app.trackerRightScroll = 0;
            } else if (app.combatRulesDrawerOpen && (kDown & KEY_L)) {
                int rc = static_cast<int>(std::max<uint32_t>(1, app.db.ruleCount()));
                app.drawerRuleIndex = (app.drawerRuleIndex + rc - 1) % rc;
                app.trackerRightScroll = 0;
            } else if (kDown & KEY_CSTICK_DOWN || kDown & KEY_CPAD_DOWN) {
                app.trackerRightScroll += 2;
            } else if (kDown & KEY_CSTICK_UP || kDown & KEY_CPAD_UP) {
                app.trackerRightScroll = std::max(0, app.trackerRightScroll - 2);
            }

            if (tapped) {
                if (!app.tracker.isInCombat()) {
                    float y = 54.0f;
                    for (size_t i = 0; i < app.campaign.encounters.size() && i < 3; ++i) {
                        if (Rect{8.0f, y, 304.0f, 38.0f}.contains(tx, ty)) {
                            app.tracker.startEncounter(app.campaign.encounters[i], app.campaign.party, app.db, app.rngState);
                            app.campaignMgr.saveCampaign(app.campaign, &app.tracker);
                            app.sfx.play(SFX_DICE_ROLL);
                            app.setStatus("Initiative rolled!");
                            break;
                        }
                        y += 44.0f;
                    }
                    if (Rect{8.0f, 190.0f, 148.0f, 40.0f}.contains(tx, ty)) {
                        app.mode = MODE_MANUAL;
                    } else if (Rect{164.0f, 190.0f, 148.0f, 40.0f}.contains(tx, ty)) {
                        app.mode = MODE_TOOLS;
                        app.toolsSubTab = 0;
                    }
                } else if (app.fightPanel == FIGHT_PANEL_CONDITIONS) {
                    for (int i = 0; i < CONDITION_COUNT; ++i) {
                        int col = i % 3, row = i / 3;
                        if (Rect{6.0f + col * 104.0f, 48.0f + row * 30.0f, 100.0f, 26.0f}.contains(tx, ty)) {
                            app.tracker.toggleCondition(app.tracker.selectedIndex(), i);
                            app.drawerRuleIndex = i;
                            app.sfx.play(SFX_TAP);
                            break;
                        }
                    }
                    if (Rect{6.0f, 202.0f, 148.0f, 32.0f}.contains(tx, ty)) {
                        app.tracker.clearConditions(app.tracker.selectedIndex());
                    } else if (Rect{166.0f, 202.0f, 148.0f, 32.0f}.contains(tx, ty)) {
                        app.fightPanel = FIGHT_PANEL_MAIN;
                    }
                } else if (app.fightPanel == FIGHT_PANEL_END_CONFIRM) {
                    if (Rect{12.0f, 118.0f, 94.0f, 46.0f}.contains(tx, ty)) {
                        app.fightPanel = FIGHT_PANEL_MAIN;
                    } else if (Rect{113.0f, 118.0f, 94.0f, 46.0f}.contains(tx, ty)) {
                        app.tracker.clear();
                        app.campaignMgr.saveCampaign(app.campaign, &app.tracker);
                        app.lastGenLoot = Generators::generateLoot(app.db, app.campaign.partyLevel, true, app.rngState);
                        app.hasGenLoot  = true;
                        app.fightPanel  = FIGHT_PANEL_MAIN;
                        app.mode        = MODE_TOOLS;
                        app.toolsSubTab = 1;
                        app.sfx.play(SFX_NAT20);
                        app.setStatus("Post-fight loot rolled!");
                    } else if (Rect{214.0f, 118.0f, 94.0f, 46.0f}.contains(tx, ty)) {
                        app.tracker.clear();
                        app.campaignMgr.saveCampaign(app.campaign, &app.tracker);
                        app.fightPanel = FIGHT_PANEL_MAIN;
                        app.setStatus("Fight cleared. Campaign kept.");
                    }
                } else if (app.fightPanel == FIGHT_PANEL_CUSTOM_HP) {
                    int sel = app.tracker.selectedIndex();
                    for (int i = 0; i < 12; ++i) {
                        int col = i % 3, row = i / 3;
                        if (Rect{6.0f + col * 60.0f, 50.0f + row * 46.0f, 56.0f, 42.0f}.contains(tx, ty)) {
                            if (i < 9) {
                                int digit = i + 1;
                                if (app.customHpValue < 100) app.customHpValue = app.customHpValue * 10 + digit;
                            } else if (i == 9) {
                                app.customHpValue = 0;
                            } else if (i == 10) {
                                if (app.customHpValue < 100) app.customHpValue = app.customHpValue * 10;
                            } else {
                                app.fightPanel = FIGHT_PANEL_MAIN;
                            }
                            app.sfx.play(SFX_TAP);
                            break;
                        }
                    }
                    if (Rect{194.0f, 106.0f, 120.0f, 54.0f}.contains(tx, ty)) {
                        if (app.customHpValue > 0) app.tracker.applyHpDelta(sel, -app.customHpValue);
                        app.customHpValue = 0;
                        app.fightPanel = FIGHT_PANEL_MAIN;
                        app.sfx.play(SFX_TAP);
                    } else if (Rect{194.0f, 170.0f, 120.0f, 54.0f}.contains(tx, ty)) {
                        if (app.customHpValue > 0) app.tracker.applyHpDelta(sel, +app.customHpValue);
                        app.customHpValue = 0;
                        app.fightPanel = FIGHT_PANEL_MAIN;
                        app.sfx.play(SFX_TAP);
                    }
                } else {
                    int sel = app.tracker.selectedIndex();
                    if (Rect{6.0f,   52.0f, 58.0f, 44.0f}.contains(tx, ty)) { app.tracker.applyHpDelta(sel, -10); app.sfx.play(SFX_TAP); }
                    else if (Rect{68.0f,  52.0f, 58.0f, 44.0f}.contains(tx, ty)) { app.tracker.applyHpDelta(sel, -5); app.sfx.play(SFX_TAP); }
                    else if (Rect{130.0f, 52.0f, 58.0f, 44.0f}.contains(tx, ty)) { app.tracker.applyHpDelta(sel, -1); app.sfx.play(SFX_TAP); }
                    else if (Rect{194.0f, 52.0f, 58.0f, 44.0f}.contains(tx, ty)) { app.tracker.applyHpDelta(sel, +1); app.sfx.play(SFX_TAP); }
                    else if (Rect{256.0f, 52.0f, 58.0f, 44.0f}.contains(tx, ty)) { app.tracker.applyHpDelta(sel, +5); app.sfx.play(SFX_TAP); }
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
                        app.sfx.play(SFX_TAP);
                    } else if (Rect{196.0f, 148.0f, 118.0f, 46.0f}.contains(tx, ty)) {
                        app.mode = MODE_MANUAL;
                    } else if (Rect{6.0f, 200.0f, 68.0f, 34.0f}.contains(tx, ty)) {
                        app.tracker.prevTurn();
                    } else if (Rect{78.0f, 200.0f, 84.0f, 34.0f}.contains(tx, ty)) {
                        app.customHpValue = 0;
                        app.fightPanel = FIGHT_PANEL_CUSTOM_HP;
                    } else if (Rect{166.0f, 200.0f, 76.0f, 34.0f}.contains(tx, ty)) {
                        const Combatant& cur = app.tracker.combatants()[static_cast<size_t>(sel)];
                        int atkMod = 0;
                        std::string atkName = cur.displayName + " Attack";
                        if (!cur.isPlayer) {
                            MonsterView mv{};
                            if (app.db.getMonster(cur.monsterId, mv)) {
                                for (int a = 0; a < mv.actionCount; ++a) {
                                    ActionView av{};
                                    if (app.db.getMonsterAction(mv.id, a, av) && av.attackBonus != -128) {
                                        atkMod = av.attackBonus;
                                        atkName = std::string(mv.name) + ": " + av.name;
                                        break;
                                    }
                                }
                            }
                        }
                        app.diceShape   = DIE_D20;
                        app.diceMod     = atkMod;
                        app.diceAdvMode = ROLL_NORMAL;
                        app.dice.roll(app.diceShape, app.diceMod, app.diceAdvMode, app.rngState);
                        app.sfx.play(SFX_DICE_ROLL);
                        app.mode = MODE_DICE;
                        char msg[64];
                        std::snprintf(msg, sizeof(msg), "%.28s (%+d)", atkName.c_str(), atkMod);
                        app.setStatus(msg, 150);
                    } else if (Rect{246.0f, 200.0f, 68.0f, 34.0f}.contains(tx, ty)) {
                        app.fightPanel = FIGHT_PANEL_END_CONFIRM;
                    }
                }
            }
        } else if (app.mode == MODE_MANUAL) {
            if (kDown & KEY_DUP && app.monsterListCursor > 0) {
                --app.monsterListCursor;
                if (app.monsterListCursor < app.monsterListScroll) app.monsterListScroll = app.monsterListCursor;
                app.manualTopScroll = 0;
            } else if (kDown & KEY_DDOWN && app.monsterListCursor + 1 < static_cast<int>(app.monsterResults.size())) {
                ++app.monsterListCursor;
                if (app.monsterListCursor >= app.monsterListScroll + 3) app.monsterListScroll = app.monsterListCursor - 2;
                app.manualTopScroll = 0;
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
                        if (Rect{214.0f, ry, 46.0f, 30.0f}.contains(tx, ty)) {
                            app.addMonsterToDraft(app.monsterResults[static_cast<size_t>(idx)].recordId);
                            app.sfx.play(SFX_TAP);
                            app.setStatus("Added to Prep Builder!");
                            break;
                        } else if (Rect{262.0f, ry, 52.0f, 30.0f}.contains(tx, ty)) {
                            app.tracker.addPartyIfMissing(app.campaign.party, app.rngState);
                            app.tracker.addMonsterMidFight(app.monsterResults[static_cast<size_t>(idx)].recordId, 1, app.db, app.rngState);
                            app.campaignMgr.saveCampaign(app.campaign, &app.tracker);
                            app.sfx.play(SFX_TAP);
                            app.setStatus("Added to Fight Tracker!");
                            break;
                        } else if (Rect{6.0f, ry, 204.0f, 30.0f}.contains(tx, ty)) {
                            app.monsterListCursor = idx;
                            app.manualTopScroll = 0;
                            break;
                        }
                    }
                }
            }
        } else if (app.mode == MODE_DICE) {
            if (kDown & KEY_A || kDown & KEY_X) {
                app.dice.rollPool(app.diceShape, app.diceCount, app.diceMod, app.diceAdvMode, app.rngState);
                app.sfx.play(SFX_DICE_ROLL);
            } else if (kDown & KEY_L) {
                app.diceSkinIdx = (app.diceSkinIdx + 1) % DICE_SKIN_COUNT;
                app.saveUserSettings();
                app.sfx.play(SFX_TAP);
            } else if (kDown & KEY_R) {
                app.traySkinIdx = (app.traySkinIdx + 1) % TRAY_SKIN_COUNT;
                app.saveUserSettings();
                app.sfx.play(SFX_TAP);
            }

            if (tapped) {
                for (int i = 0; i < DIE_SHAPE_COUNT; ++i) {
                    if (Rect{4.0f + i * 44.5f, 32.0f, 42.0f, 32.0f}.contains(tx, ty)) {
                        app.diceShape = static_cast<DieShape>(i);
                        app.dice.rollPool(app.diceShape, app.diceCount, app.diceMod, app.diceAdvMode, app.rngState);
                        app.sfx.play(SFX_DICE_ROLL);
                        break;
                    }
                }
                const int presets[5] = {1, 2, 4, 6, 8};
                if (Rect{6.0f, 68.0f, 34.0f, 32.0f}.contains(tx, ty)) {
                    app.diceCount = std::max(1, app.diceCount - 1);
                    app.sfx.play(SFX_TAP);
                } else if (Rect{116.0f, 68.0f, 34.0f, 32.0f}.contains(tx, ty)) {
                    app.diceCount = std::min(DiceRoller::MAX_POOL_DICE, app.diceCount + 1);
                    app.sfx.play(SFX_TAP);
                } else {
                    for (int i = 0; i < 5; ++i) {
                        if (Rect{156.0f + i * 32.0f, 68.0f, 30.0f, 32.0f}.contains(tx, ty)) {
                            app.diceCount = presets[i];
                            app.sfx.play(SFX_TAP);
                            break;
                        }
                    }
                }

                if (Rect{6.0f, 104.0f, 44.0f, 32.0f}.contains(tx, ty)) {
                    app.diceMod = std::max(-10, app.diceMod - 1);
                    app.sfx.play(SFX_TAP);
                } else if (Rect{134.0f, 104.0f, 44.0f, 32.0f}.contains(tx, ty)) {
                    app.diceMod = std::min(20, app.diceMod + 1);
                    app.sfx.play(SFX_TAP);
                } else if (Rect{186.0f, 104.0f, 128.0f, 32.0f}.contains(tx, ty)) {
                    app.diceShape = DIE_D20;
                    app.diceCount = 1;
                    app.diceAdvMode = static_cast<RollAdvantageMode>((app.diceAdvMode + 1) % 3);
                    app.sfx.play(SFX_TAP);
                } else if (Rect{6.0f, 142.0f, 308.0f, 46.0f}.contains(tx, ty)) {
                    app.dice.rollPool(app.diceShape, app.diceCount, app.diceMod, app.diceAdvMode, app.rngState);
                    app.sfx.play(SFX_DICE_ROLL);
                } else if (Rect{6.0f, 194.0f, 150.0f, 40.0f}.contains(tx, ty)) {
                    app.diceSkinIdx = (app.diceSkinIdx + 1) % DICE_SKIN_COUNT;
                    app.saveUserSettings();
                    app.sfx.play(SFX_TAP);
                } else if (Rect{164.0f, 194.0f, 150.0f, 40.0f}.contains(tx, ty)) {
                    app.traySkinIdx = (app.traySkinIdx + 1) % TRAY_SKIN_COUNT;
                    app.saveUserSettings();
                    app.sfx.play(SFX_TAP);
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
                for (int i = 0; i < REF_TAB_COUNT; ++i) {
                    if (Rect{4.0f + i * 62.5f, 30.0f, 60.0f, 20.0f}.contains(tx, ty)) {
                        app.refTab = static_cast<RefTabFilter>(i);
                        app.refreshRuleSearch();
                        break;
                    }
                }
                if (Rect{276.0f, 53.0f, 38.0f, 21.0f}.contains(tx, ty)) {
                    app.ruleQuery.clear();
                    app.refreshRuleSearch();
                } else if (handleQwertyTouch(tx, ty, app.ruleQuery)) {
                    app.refreshRuleSearch();
                } else {
                    for (int i = 0; i < 3; ++i) {
                        int idx = app.ruleListScroll + i;
                        if (idx >= static_cast<int>(app.ruleResults.size())) break;
                        if (Rect{6.0f, 77.0f + i * 28.0f, 308.0f, 26.0f}.contains(tx, ty)) {
                            app.ruleListCursor = idx;
                            app.ruleTopScroll = 0;
                            break;
                        }
                    }
                }
            }
        } else if (app.mode == MODE_TOOLS) {
            if (app.toolsSubTab == 0 && app.encBuilderView) {
                if (kDown & KEY_DUP && app.draftSelectedIdx > 0) --app.draftSelectedIdx;
                if (kDown & KEY_DDOWN && app.draftSelectedIdx + 1 < static_cast<int>(app.draftEncounter.slots.size())) ++app.draftSelectedIdx;
            } else {
                if (kDown & KEY_DUP && app.partySelectedIdx > 0) --app.partySelectedIdx;
                if (kDown & KEY_DDOWN && app.partySelectedIdx + 1 < static_cast<int>(app.campaign.party.size())) ++app.partySelectedIdx;
            }

            if (tapped) {
                for (int i = 0; i < 6; ++i) {
                    if (Rect{4.0f + i * 52.0f, 32.0f, 50.0f, 26.0f}.contains(tx, ty)) {
                        app.toolsSubTab = i;
                        app.sfx.play(SFX_TAP);
                        break;
                    }
                }
                if (app.toolsSubTab == 0) {
                    if (Rect{6.0f, 62.0f, 150.0f, 26.0f}.contains(tx, ty)) {
                        app.encBuilderView = false;
                    } else if (Rect{164.0f, 62.0f, 150.0f, 26.0f}.contains(tx, ty)) {
                        app.encBuilderView = true;
                    } else if (app.encBuilderView) {
                        if (Rect{6.0f, 94.0f, 98.0f, 38.0f}.contains(tx, ty) && !app.draftEncounter.slots.empty()) {
                            int di = std::clamp(app.draftSelectedIdx, 0, static_cast<int>(app.draftEncounter.slots.size()) - 1);
                            if (app.draftEncounter.slots[static_cast<size_t>(di)].count > 1) {
                                --app.draftEncounter.slots[static_cast<size_t>(di)].count;
                            } else {
                                app.draftEncounter.slots.erase(app.draftEncounter.slots.begin() + di);
                                app.draftSelectedIdx = std::max(0, std::min(app.draftSelectedIdx, static_cast<int>(app.draftEncounter.slots.size()) - 1));
                            }
                        } else if (Rect{111.0f, 94.0f, 98.0f, 38.0f}.contains(tx, ty) && !app.draftEncounter.slots.empty()) {
                            int di = std::clamp(app.draftSelectedIdx, 0, static_cast<int>(app.draftEncounter.slots.size()) - 1);
                            if (app.draftEncounter.slots[static_cast<size_t>(di)].count < 20) {
                                ++app.draftEncounter.slots[static_cast<size_t>(di)].count;
                            }
                        } else if (Rect{216.0f, 94.0f, 98.0f, 38.0f}.contains(tx, ty)) {
                            app.draftEncounter.slots.clear();
                            app.draftSelectedIdx = 0;
                        } else if (Rect{6.0f, 138.0f, 308.0f, 42.0f}.contains(tx, ty) && !app.draftEncounter.slots.empty()) {
                            std::string ename = "Custom Encounter";
                            if (openSystemKeyboard(ename, 32, "Encounter Name")) {
                                SavedEncounter toSave = app.draftEncounter;
                                toSave.name = ename;
                                app.campaign.encounters.insert(app.campaign.encounters.begin(), toSave);
                                if (app.campaign.encounters.size() > 32) app.campaign.encounters.resize(32);
                                app.campaignMgr.saveCampaign(app.campaign, &app.tracker);
                                app.setStatus("Encounter saved to campaign!");
                            }
                        } else if (Rect{6.0f, 186.0f, 308.0f, 42.0f}.contains(tx, ty) && !app.draftEncounter.slots.empty()) {
                            app.tracker.startEncounter(app.draftEncounter, app.campaign.party, app.db, app.rngState);
                            app.campaignMgr.saveCampaign(app.campaign, &app.tracker);
                            app.mode = MODE_FIGHT;
                            app.sfx.play(SFX_DICE_ROLL);
                            app.setStatus("Draft encounter started!");
                        }
                    } else {
                        if (Rect{6.0f, 94.0f, 150.0f, 38.0f}.contains(tx, ty)) {
                            app.genTerrain = static_cast<TerrainType>((app.genTerrain + 1) % TERRAIN_COUNT);
                        } else if (Rect{164.0f, 94.0f, 150.0f, 38.0f}.contains(tx, ty)) {
                            app.genDifficulty = static_cast<EncounterDifficulty>((app.genDifficulty + 1) % DIFF_COUNT);
                        } else if (Rect{6.0f, 138.0f, 308.0f, 42.0f}.contains(tx, ty)) {
                            int pSize = std::max(1, static_cast<int>(app.campaign.party.size()));
                            app.lastGenEncounter = Generators::generateEncounter(
                                app.db, app.campaign.partyLevel, pSize, app.genTerrain, app.genDifficulty, app.rngState);
                            app.hasGenEncounter = !app.lastGenEncounter.slots.empty();
                            app.sfx.play(SFX_DICE_ROLL);
                        } else if (Rect{6.0f, 186.0f, 308.0f, 42.0f}.contains(tx, ty) && app.hasGenEncounter) {
                            app.tracker.startEncounter(app.lastGenEncounter, app.campaign.party, app.db, app.rngState);
                            app.campaignMgr.saveCampaign(app.campaign, &app.tracker);
                            app.mode = MODE_FIGHT;
                            app.sfx.play(SFX_DICE_ROLL);
                            app.setStatus("Random encounter started!");
                        }
                    }
                } else if (app.toolsSubTab == 1) {
                    if (Rect{6.0f, 70.0f, 308.0f, 48.0f}.contains(tx, ty)) {
                        app.lastGenNpc = Generators::generateNpc(app.rngState);
                        app.hasGenNpc = true;
                        app.sfx.play(SFX_TAP);
                    } else if (Rect{6.0f, 130.0f, 150.0f, 48.0f}.contains(tx, ty)) {
                        app.lastGenLoot = Generators::generateLoot(app.db, app.campaign.partyLevel, false, app.rngState);
                        app.hasGenLoot = true;
                        app.sfx.play(SFX_TAP);
                    } else if (Rect{164.0f, 130.0f, 150.0f, 48.0f}.contains(tx, ty)) {
                        app.lastGenLoot = Generators::generateLoot(app.db, app.campaign.partyLevel, true, app.rngState);
                        app.hasGenLoot = true;
                        app.sfx.play(SFX_NAT20);
                    }
                } else if (app.toolsSubTab == 2) {
                    if (Rect{6.0f, 62.0f, 150.0f, 26.0f}.contains(tx, ty)) {
                        app.hbIsSpell = false;
                    } else if (Rect{164.0f, 62.0f, 150.0f, 26.0f}.contains(tx, ty)) {
                        app.hbIsSpell = true;
                    } else if (Rect{6.0f, 92.0f, 150.0f, 34.0f}.contains(tx, ty)) {
                        openSystemKeyboard(app.hbName, 28, app.hbIsSpell ? "Spell Name" : "Monster Name");
                    } else if (Rect{164.0f, 92.0f, 150.0f, 34.0f}.contains(tx, ty)) {
                        openSystemKeyboard(app.hbDesc, 80, app.hbIsSpell ? "Spell Description" : "Attack / Ability Text");
                    } else if (!app.hbIsSpell) {
                        if (Rect{6.0f,   132.0f, 48.0f, 36.0f}.contains(tx, ty)) app.hbAc = std::max(5, app.hbAc - 1);
                        else if (Rect{56.0f,  132.0f, 48.0f, 36.0f}.contains(tx, ty)) app.hbAc = std::min(30, app.hbAc + 1);
                        else if (Rect{110.0f, 132.0f, 48.0f, 36.0f}.contains(tx, ty)) app.hbHp = std::max(1, app.hbHp - 5);
                        else if (Rect{160.0f, 132.0f, 48.0f, 36.0f}.contains(tx, ty)) app.hbHp = std::min(999, app.hbHp + 5);
                        else if (Rect{214.0f, 132.0f, 48.0f, 36.0f}.contains(tx, ty)) app.hbCr = std::max(0, app.hbCr - 1);
                        else if (Rect{264.0f, 132.0f, 50.0f, 36.0f}.contains(tx, ty)) app.hbCr = std::min(30, app.hbCr + 1);
                    } else {
                        if (Rect{6.0f, 132.0f, 150.0f, 36.0f}.contains(tx, ty)) app.hbSpellLevel = std::max(0, app.hbSpellLevel - 1);
                        else if (Rect{164.0f, 132.0f, 150.0f, 36.0f}.contains(tx, ty)) app.hbSpellLevel = std::min(9, app.hbSpellLevel + 1);
                    }

                    if (Rect{6.0f, 176.0f, 308.0f, 46.0f}.contains(tx, ty)) {
                        app.campaignMgr.ensureDirectories();
                        bool ok = false;
                        if (!app.hbIsSpell) {
                            HomebrewMonster m;
                            m.name         = app.hbName;
                            m.sizeAndAlign = "Medium custom creature";
                            m.armorClass   = static_cast<uint16_t>(app.hbAc);
                            m.hitPoints    = static_cast<uint16_t>(app.hbHp);
                            m.crIndex      = static_cast<uint8_t>(app.hbCr <= 0 ? 0 : std::min(33, app.hbCr + 3));
                            HomebrewAction act;
                            act.name     = app.hbActionName;
                            act.summary  = app.hbDesc.substr(0, 72);
                            act.fullDesc = app.hbDesc;
                            m.actions.push_back(act);
                            ok = app.db.saveHomebrewMonster(m, app.campaignMgr.homebrewDir().c_str());
                            app.refreshMonsterSearch();
                        } else {
                            HomebrewSpell s;
                            s.name  = app.hbName;
                            s.level = static_cast<uint8_t>(app.hbSpellLevel);
                            s.body  = app.hbDesc;
                            ok = app.db.saveHomebrewSpell(s, app.campaignMgr.homebrewDir().c_str());
                            app.refreshRuleSearch();
                        }
                        if (ok) {
                            app.sfx.play(SFX_NAT20);
                            app.setStatus("Saved Homebrew entry to SD!");
                        } else {
                            app.setStatus("Failed to save Homebrew entry.");
                        }
                    }
                } else if (app.toolsSubTab == 3) {
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
                } else if (app.toolsSubTab == 4) {
                    float y = 64.0f;
                    for (size_t i = 0; i < app.campaigns.size() && i < 3; ++i) {
                        if (Rect{6.0f, y, 308.0f, 32.0f}.contains(tx, ty)) {
                            app.campaignMgr.saveCampaign(app.campaign, &app.tracker);
                            app.campaignMgr.loadCampaign(app.campaigns[i].slug, app.campaign, &app.tracker);
                            app.partySelectedIdx = 0;
                            app.saveUserSettings();
                            app.sfx.play(SFX_TAP);
                            app.setStatus("Switched campaign.");
                            break;
                        }
                        y += 35.0f;
                    }
                    if (Rect{6.0f, 174.0f, 180.0f, 40.0f}.contains(tx, ty)) {
                        std::string cname = "New Campaign";
                        if (openSystemKeyboard(cname, 28, "Campaign Name")) {
                            app.campaignMgr.saveCampaign(app.campaign, &app.tracker);
                            app.campaignMgr.createCampaign(cname, app.campaign);
                            app.tracker.clear();
                            app.campaigns = app.campaignMgr.listCampaigns();
                            app.saveUserSettings();
                            app.setStatus("Created campaign.");
                        }
                    } else if (Rect{192.0f, 174.0f, 122.0f, 40.0f}.contains(tx, ty)) {
                        app.soundEnabled = !app.soundEnabled;
                        app.sfx.setEnabled(app.soundEnabled);
                        app.saveUserSettings();
                        if (app.soundEnabled) app.sfx.play(SFX_TAP);
                        app.setStatus(app.soundEnabled ? "Sound enabled." : "Sound muted.");
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
        if (app.showTitleSplash) {
            renderTopTitleSplash(topBuf, spriteSheet);
        } else if (app.mode == MODE_FIGHT) {
            renderTopFight(topBuf, app);
        } else if (app.mode == MODE_MANUAL) {
            renderTopManual(topBuf, app);
        } else if (app.mode == MODE_DICE) {
            renderTopDiceTray(topBuf, app);
        } else if (app.mode == MODE_RULES) {
            renderTopRules(topBuf, app);
        } else {
            renderTopTools(topBuf, app);
        }

        C2D_TargetClear(botTarget, theme::WALNUT);
        C2D_SceneBegin(botTarget);
        if (app.showTitleSplash) {
            renderBottomTitleSplash(botBuf, spriteSheet, app);
        } else {
            drawModeTabs(botBuf, spriteSheet, app.mode);
            if (app.mode == MODE_FIGHT)       renderBottomFight(botBuf, app);
            else if (app.mode == MODE_MANUAL) renderBottomManual(botBuf, app);
            else if (app.mode == MODE_DICE)   renderBottomDice(botBuf, app);
            else if (app.mode == MODE_RULES)  renderBottomRules(botBuf, app);
            else                              renderBottomTools(botBuf, app);

            if (app.statusFrames > 0 && !app.statusBanner.empty()) {
                Rect toast{16.0f, 206.0f, 288.0f, 26.0f};
                C2D_DrawRectSolid(toast.x + 2.0f, toast.y + 2.0f, 0.82f, toast.w, toast.h, rgba32(0, 0, 0, 150));
                C2D_DrawRectSolid(toast.x, toast.y, 0.85f, toast.w, toast.h, theme::GOLD_BRIGHT);
                C2D_DrawRectSolid(toast.x + 2.0f, toast.y + 2.0f, 0.88f, toast.w - 4.0f, toast.h - 4.0f, theme::FELT);
                drawTextCentered(botBuf, app.statusBanner.c_str(), toast, 0.38f, theme::WHITE, 0.92f);
            }
        }

        C3D_FrameEnd(0);
    }

    app.campaignMgr.saveCampaign(app.campaign, &app.tracker);
    app.saveUserSettings();

    app.sfx.shutdown();
    if (spriteSheet) C2D_SpriteSheetFree(spriteSheet);
    C2D_TextBufDelete(topBuf);
    C2D_TextBufDelete(botBuf);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    romfsExit();
    return 0;
}
