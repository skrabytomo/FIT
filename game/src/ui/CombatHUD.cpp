#include "CombatHUD.h"
#include <sstream>
#include <algorithm>

bool CombatHUD::init(int sw, int sh)
{
    buildLayout(sw, sh);
    return true;
}

void CombatHUD::resize(int sw, int sh)
{
    m_screenW = sw; m_screenH = sh;
    buildLayout(sw, sh);
}

void CombatHUD::buildLayout(int sw, int sh)
{
    float bh = 130.0f;  // bottom HUD height
    float by = sh - bh;

    m_bottomBar = {0, by, (float)sw, bh};

    // Active unit panel — left
    m_activeUnitPanel = Panel({0, by, 220.0f, bh});
    m_activeUnitPanel.title = "Active Unit";

    // Targeted unit panel
    m_targetUnitPanel = Panel({224.0f, by, 220.0f, bh});
    m_targetUnitPanel.title = "Target";

    // Log panel — center
    m_logPanel = Panel({448.0f, by, (float)sw - 448.0f - 180.0f, bh});
    m_logPanel.title = "Combat Log";

    // Action panel — right
    float ax = (float)sw - 176.0f;
    m_actionPanel = Panel({ax, by, 176.0f, bh});
    m_actionPanel.title = "Actions";

    float btnY = by + 30.0f;
    m_waitBtn    = Button("Wait",    {ax+8, btnY,      160.0f, 26.0f}, [this]{ if(onWait)     onWait();    });
    m_defendBtn  = Button("Defend",  {ax+8, btnY+30.0f,160.0f, 26.0f}, [this]{ if(onDefend)   onDefend();  });
    m_spellsBtn  = Button("Spells",  {ax+8, btnY+60.0f,160.0f, 26.0f}, [this]{ if(onSpells)   onSpells();  });
    m_retreatBtn = Button("Retreat", {ax+8, btnY+90.0f,160.0f, 26.0f}, [this]{ if(onEndCombat) onEndCombat(); });

    m_retreatBtn.colorBorder = UIColor::hex(UITheme::DANGER_RED, 0.7f);
    m_retreatBtn.colorText   = UIColor::hex(UITheme::DANGER_RED);

    // Turn order bar — top
    m_turnOrderBar = {0, 0, (float)sw, 44.0f};
}

void CombatHUD::draw(UIRenderer& rdr, const CombatEngine& engine)
{
    drawTurnOrder(rdr, engine);

    // Bottom HUD background
    rdr.drawRect(m_bottomBar,
        UIColor::hex(UITheme::BG_PANEL_DARK, 0.95f),
        UIColor::hex(UITheme::BORDER), 1.0f);

    auto* active = const_cast<CombatEngine&>(engine).activeUnit();
    drawUnitInfo(rdr, active, true);
    drawUnitInfo(rdr, m_hoveredUnit, false);
    drawCombatLog(rdr, engine);
    drawActionBar(rdr);

    // Phase display
    std::string phaseStr;
    switch (engine.phase()) {
        case CombatPhase::PlayerTurn: phaseStr = "YOUR TURN"; break;
        case CombatPhase::EnemyTurn:  phaseStr = "ENEMY TURN"; break;
        case CombatPhase::Victory:    phaseStr = "VICTORY!";   break;
        case CombatPhase::Defeat:     phaseStr = "DEFEAT";     break;
        default: phaseStr = "..."; break;
    }
    UIColor phaseColor = (engine.phase() == CombatPhase::Victory) ?
        UIColor::hex(UITheme::GOLD) :
        (engine.phase() == CombatPhase::Defeat) ?
        UIColor::hex(UITheme::DANGER_RED) :
        UIColor::hex(UITheme::TEXT_SECONDARY);

    float tx = (m_screenW - phaseStr.size() * 8.0f) * 0.5f;
    rdr.drawText(phaseStr, tx, 48.0f, phaseColor, 14.0f);

    m_tooltip.draw(rdr);
}

void CombatHUD::drawUnitInfo(UIRenderer& rdr, const CombatUnit* unit, bool isActive)
{
    Panel& panel = isActive ? m_activeUnitPanel : m_targetUnitPanel;
    panel.draw(rdr);
    if (!unit) return;

    float x = panel.bounds.x + 8.0f;
    float y = panel.bounds.y + 28.0f;
    float w = panel.bounds.w - 16.0f;

    // Unit name + count
    std::string header = unit->name + " x" + std::to_string(unit->count);
    UIColor nameColor = unit->isPlayer ?
        UIColor::hex(UITheme::NATURE_GREEN) :
        UIColor::hex(UITheme::BLOOD_RED);
    rdr.drawText(header, x, y, nameColor, 12.0f);
    y += 16.0f;

    // HP bar
    float hpFrac = unit->maxHp > 0 ?
        static_cast<float>(unit->hp) / unit->maxHp : 0.0f;
    UIColor hpColor = hpFrac > 0.5f ? UIColor::hex(UITheme::HP_GREEN) :
                      hpFrac > 0.25f ? UIColor::hex(UITheme::MORALE_GOLD) :
                                       UIColor::hex(UITheme::HP_LOW);
    rdr.drawText("HP: " + std::to_string(unit->hp) + "/" + std::to_string(unit->maxHp),
                 x, y, UIColor::hex(UITheme::TEXT_SECONDARY), 11.0f);
    y += 13.0f;
    rdr.drawBar({x, y, w, 6.0f}, hpFrac, hpColor,
                UIColor::hex(UITheme::BG_DARK), UIColor::hex(UITheme::BORDER));
    y += 10.0f;

    // Morale bar (if not immune)
    if (!unit->moraleImmune) {
        float moraleFrac = unit->morale / 100.0f;
        rdr.drawText("Morale", x, y, UIColor::hex(UITheme::TEXT_SECONDARY), 11.0f);
        y += 13.0f;
        rdr.drawBar({x, y, w, 5.0f}, moraleFrac,
                    UIColor::hex(UITheme::MORALE_GOLD),
                    UIColor::hex(UITheme::BG_DARK),
                    UIColor::hex(UITheme::BORDER));
        y += 9.0f;
    }

    // Stats
    std::string stats = "ATK:" + std::to_string(unit->attack) +
                        " DEF:" + std::to_string(unit->defense) +
                        " SPD:" + std::to_string(unit->speed);
    rdr.drawText(stats, x, y, UIColor::hex(UITheme::TEXT_SECONDARY), 11.0f);
    y += 14.0f;

    // Active buff/debuff status
    bool hasBuff = unit->roundAttackBonus != 0 || unit->roundDefenseBonus != 0;
    if (hasBuff) {
        std::string buffStr;
        if (unit->roundAttackBonus > 0)       buffStr += "ATK+" + std::to_string(unit->roundAttackBonus) + " ";
        else if (unit->roundAttackBonus < 0)   buffStr += "ATK" + std::to_string(unit->roundAttackBonus) + " ";
        if (unit->roundDefenseBonus > 0)       buffStr += "DEF+" + std::to_string(unit->roundDefenseBonus);
        else if (unit->roundDefenseBonus < 0)  buffStr += "DEF" + std::to_string(unit->roundDefenseBonus);
        UIColor buffCol = (unit->roundAttackBonus > 0 || unit->roundDefenseBonus > 0)
                          ? UIColor::hex(UITheme::MORALE_GOLD) : UIColor::hex(UITheme::BLOOD_RED);
        rdr.drawText(buffStr, x, y, buffCol, 11.0f);
        y += 13.0f;
    }

    // DoT status
    if (unit->poisonRounds > 0) {
        std::string ps = "Poison: " + std::to_string(unit->poisonDamage)
                       + "/rnd (" + std::to_string(unit->poisonRounds) + ")";
        rdr.drawText(ps, x, y, UIColor::rgba(0.31f, 0.86f, 0.31f), 11.0f);
        y += 13.0f;
    }
    if (unit->burnRounds > 0) {
        std::string bs = "Burn: " + std::to_string(unit->burnDamage)
                       + "/rnd (" + std::to_string(unit->burnRounds) + ")";
        rdr.drawText(bs, x, y, UIColor::rgba(1.0f, 0.47f, 0.16f), 11.0f);
        y += 13.0f;
    }

    // Shots remaining
    if (unit->range > 0) {
        rdr.drawText("Shots: " + std::to_string(unit->shotsLeft),
                     x, y, UIColor::hex(UITheme::MANA_BLUE), 11.0f);
    }
}

void CombatHUD::drawTurnOrder(UIRenderer& rdr, const CombatEngine& engine)
{
    rdr.drawRect(m_turnOrderBar,
        UIColor::hex(UITheme::BG_PANEL_DARK, 0.90f),
        UIColor::hex(UITheme::BORDER), 1.0f);

    auto& order = engine.turnOrder();
    float x = 8.0f;
    float y = 4.0f;
    float slotW = 36.0f, slotH = 36.0f;

    for (int i = 0; i < static_cast<int>(order.size()) && x + slotW < m_screenW - 200; ++i) {
        const CombatUnit* u = engine.grid().getUnit(order[i]);  // const cast needed
        if (!u || !u->alive) continue;

        bool isActive = (i == engine.turnIndex());
        UIColor bg  = u->isPlayer ?
            UIColor::hex(UITheme::NATURE_GREEN, isActive ? 0.9f : 0.4f) :
            UIColor::hex(UITheme::BLOOD_RED,    isActive ? 0.9f : 0.4f);
        UIColor brd = isActive ? UIColor::hex(UITheme::GOLD) :
                                 UIColor::hex(UITheme::BORDER);

        rdr.drawRect({x, y, slotW, slotH}, bg, brd, isActive ? 2.0f : 1.0f);

        // Speed number
        rdr.drawText(std::to_string(u->speed),
                     x + 12.0f, y + 12.0f,
                     UIColor::hex(UITheme::TEXT_PRIMARY), 11.0f);
        x += slotW + 2.0f;
    }

    // Round counter — right side
    std::string rnd = "Round " + std::to_string(engine.round());
    rdr.drawText(rnd, m_screenW - 100.0f, 14.0f,
                 UIColor::hex(UITheme::TEXT_SECONDARY), 12.0f);
}

void CombatHUD::drawCombatLog(UIRenderer& rdr, const CombatEngine& engine)
{
    m_logPanel.draw(rdr);

    auto& log = engine.log();
    float x = m_logPanel.bounds.x + 6.0f;
    float y = m_logPanel.bounds.y + 28.0f;
    float maxY = m_logPanel.bounds.bottom() - 8.0f;

    // Show last N entries that fit
    int lineH = 14;
    int maxLines = static_cast<int>((maxY - y) / lineH);
    int start = std::max(0, static_cast<int>(log.size()) - maxLines);

    for (int i = start; i < static_cast<int>(log.size()) && y < maxY; ++i) {
        UIColor c = UIColor::hex(UITheme::TEXT_SECONDARY);
        if (log[i].message.find("VICTORY") != std::string::npos)
            c = UIColor::hex(UITheme::GOLD);
        else if (log[i].message.find("DEFEAT") != std::string::npos)
            c = UIColor::hex(UITheme::DANGER_RED);
        else if (log[i].message.find("killed") != std::string::npos ||
                 log[i].message.find("destroyed") != std::string::npos)
            c = UIColor::hex(UITheme::BLOOD_RED);
        rdr.drawText(log[i].message, x, y, c, 11.0f);
        y += lineH;
    }
}

void CombatHUD::drawActionBar(UIRenderer& rdr)
{
    m_actionPanel.draw(rdr);
    m_waitBtn.draw(rdr);
    m_defendBtn.draw(rdr);
    m_spellsBtn.draw(rdr);
    m_retreatBtn.draw(rdr);
}

bool CombatHUD::onMouseMove(float x, float y) {
    m_waitBtn.onMouseMove(x, y);
    m_defendBtn.onMouseMove(x, y);
    m_spellsBtn.onMouseMove(x, y);
    m_retreatBtn.onMouseMove(x, y);
    return false;
}
bool CombatHUD::onMouseDown(float x, float y) {
    if (m_waitBtn.onMouseDown(x, y))    return true;
    if (m_defendBtn.onMouseDown(x, y))  return true;
    if (m_spellsBtn.onMouseDown(x, y))  return true;
    if (m_retreatBtn.onMouseDown(x, y)) return true;
    return false;
}
bool CombatHUD::onMouseUp(float x, float y) {
    m_waitBtn.onMouseUp(x, y);
    m_defendBtn.onMouseUp(x, y);
    m_spellsBtn.onMouseUp(x, y);
    m_retreatBtn.onMouseUp(x, y);
    return false;
}
