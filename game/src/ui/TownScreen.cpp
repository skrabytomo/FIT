#include "TownScreen.h"
#include <sstream>
#include <algorithm>

bool TownScreen::init(int sw, int sh)
{
    buildLayout(sw, sh);
    return true;
}

void TownScreen::buildLayout(int sw, int sh)
{
    m_screenW = sw; m_screenH = sh;

    float pw = sw * 0.88f, ph = sh * 0.88f;
    float px = (sw - pw) * 0.5f, py = (sh - ph) * 0.5f;

    m_mainPanel = Panel({px, py, pw, ph});

    // Close button — larger and clearly labeled
    m_closeBtn = Button("EXIT TOWN", {px + pw - 110.0f, py + 4.0f, 104.0f, 28.0f},
                         [this]{ close(); if(onClose) onClose(); });
    m_closeBtn.colorBorder = UIColor::hex(UITheme::DANGER_RED, 0.7f);
    m_closeBtn.colorText   = UIColor::hex(UITheme::DANGER_RED);

    // Building tree — left 55%
    m_buildPanel = Panel({px + 4, py + 36, pw * 0.55f - 4, ph - 44});
    m_buildPanel.title = "Buildings";

    // Recruit panel — top right
    m_recruitPanel = Panel({px + pw*0.55f + 4, py + 36, pw*0.45f - 8, ph*0.55f});
    m_recruitPanel.title = "Recruit";

    // Income panel — bottom right
    m_incomePanel = Panel({px + pw*0.55f + 4, py + 36 + ph*0.55f + 4,
                           pw*0.45f - 8, ph*0.45f - 44});
    m_incomePanel.title = "Weekly Income";
}

void TownScreen::open(Town* town, Resources* playerRes, const BuildingRegistry* registry,
                      Hero* visitingHero, int currentWeek, int blueprintDiscount)
{
    m_town              = town;
    m_playerRes         = playerRes;
    m_registry          = registry;
    m_hero              = visitingHero;
    m_open              = true;
    m_currentWeek       = currentWeek;
    m_blueprintDiscount = blueprintDiscount;

    m_mainPanel.title = town->name + " — " + [town]{
        switch(town->faction) {
            case FactionId::HolyOrder:     return "Holy Order";
            case FactionId::Bloodsworn:    return "Bloodsworn";
            case FactionId::Thornkin:      return "Thornkin";
            case FactionId::EternalEmpire: return "Eternal Empire";
            case FactionId::CrimsonWardens:return "Crimson Wardens";
            case FactionId::Voidkin:       return "Voidkin";
            case FactionId::IronAssembly:  return "Iron Assembly";
            case FactionId::Amalgamate:    return "Amalgamate";
            case FactionId::Convergence:   return "Convergence";
            default: return "Unknown";
        }
    }();

    rebuildBuildingButtons();
    rebuildRecruitButtons();
}

void TownScreen::rebuildBuildingButtons()
{
    m_buildBtns.clear();
    if (!m_town || !m_registry || !m_playerRes) return;

    float x = m_buildPanel.bounds.x + 8;
    float y = m_buildPanel.bounds.y + 28;
    float bw = (m_buildPanel.bounds.w - 16) * 0.5f - 2;
    float bh = 40.0f;
    float colW = bw + 4;
    int col = 0;

    for (auto& def : m_registry->buildings()) {
        if (def.faction != FactionId::None && def.faction != m_town->faction) continue;

        BuildBtn bb;
        bb.buildingId = def.id;
        bb.built      = m_town->hasBuilding(def.id);
        bb.prereqMet  = m_town->canBuild(def.id, m_registry->buildings(),
                                         m_currentWeek, m_blueprintDiscount);
        bb.affordable = m_playerRes->canAfford(def.cost);

        // Show week requirement for locked buildings, and cost for unbought ones
        auto costStr = [&](const Resources& cost) -> std::string {
            std::string s;
            if (cost.get(ResourceType::Gold) > 0)
                s += std::to_string(cost.get(ResourceType::Gold)) + "g";
            for (int ri = 1; ri < RESOURCE_COUNT; ++ri) {
                auto rt = static_cast<ResourceType>(ri);
                int v = cost.get(rt);
                if (v > 0) { if (!s.empty()) s += " "; s += std::to_string(v) + resourceName(rt)[0]; }
            }
            return s.empty() ? "free" : s;
        };

        std::string label = def.name;
        if (bb.built) {
            label = "[BUILT] " + def.name;
        } else if (!bb.prereqMet && m_currentWeek > 0 && def.minWeek > 0) {
            int effectiveMin = std::max(1, def.minWeek - m_blueprintDiscount);
            if (m_currentWeek < effectiveMin)
                label = "[Wk " + std::to_string(effectiveMin) + "] " + def.name + "  " + costStr(def.cost);
        } else if (!bb.built) {
            label = def.name + "  [" + costStr(def.cost) + "]";
        }

        Rect btnR{x + col * colW, y, bw, bh};
        bb.btn = Button(label, btnR);
        bb.btn.enabled = bb.prereqMet && !bb.built;

        if (bb.built) {
            bb.btn.colorBorder = UIColor::hex(UITheme::NATURE_GREEN, 0.5f);
            bb.btn.colorText   = UIColor::hex(UITheme::TEXT_DISABLED);
        } else if (!bb.prereqMet) {
            bb.btn.colorBorder = UIColor::hex(UITheme::TEXT_DISABLED);
            bb.btn.colorText   = UIColor::hex(UITheme::TEXT_DISABLED);
        } else if (!bb.affordable) {
            bb.btn.colorBorder = UIColor::hex(UITheme::DANGER_RED, 0.6f);
            bb.btn.colorText   = UIColor::hex(UITheme::DANGER_RED);
        } else {
            bb.btn.colorBorder = UIColor::hex(UITheme::GOLD, 0.7f);
            bb.btn.colorText   = UIColor::hex(UITheme::GOLD);
        }

        int capturedId = def.id;
        bb.btn.onClick = [this, capturedId]{
            if (m_town && m_playerRes && m_registry) {
                m_town->build(capturedId, m_registry->buildings(), *m_playerRes);
                rebuildBuildingButtons();
                rebuildRecruitButtons();
            }
        };

        m_buildBtns.push_back(bb);
        col = 1 - col;
        if (col == 0) y += bh + 3;
        if (y + bh > m_buildPanel.bounds.bottom() - 4) break;
    }
}

void TownScreen::rebuildRecruitButtons()
{
    m_recruitBtns.clear();
    if (!m_town) return;

    float x = m_recruitPanel.bounds.x + 8;
    float y = m_recruitPanel.bounds.y + 28;
    float bw = m_recruitPanel.bounds.w - 16;

    for (auto& dw : m_town->dwellings) {
        if (dw.available <= 0) continue;

        // Look up unit name from registry
        std::string unitName = "T" + std::to_string(dw.tier);
        int costPerUnit = 0;
        if (m_registry) {
            for (const auto& ud : m_registry->units()) {
                if (ud.faction == m_town->faction && ud.tier == dw.tier
                    && ud.path == dw.path) {
                    unitName   = ud.name;
                    costPerUnit = ud.cost.get(ResourceType::Gold);
                    break;
                }
            }
        }
        bool efficient = m_hero && m_hero->efficientSpecialty;
        int effectiveCostPerUnit = efficient ? static_cast<int>(costPerUnit * 0.8f) : costPerUnit;

        RecruitBtn rb;
        rb.tier      = dw.tier;
        rb.available = dw.available;

        // Build stat tooltip
        if (m_registry) {
            for (const auto& ud : m_registry->units()) {
                if (ud.faction == m_town->faction && ud.tier == dw.tier && ud.path == dw.path) {
                    rb.defId = ud.id;
                    std::ostringstream ts;
                    ts << ud.name << "  |  ";
                    ts << "ATK " << ud.attack << "  DEF " << ud.defense;
                    ts << "  HP " << ud.hp << "  Dmg " << ud.damage_min << "-" << ud.damage_max;
                    ts << "  Spd " << ud.speed;
                    if (ud.range > 0) ts << "  Rng " << ud.range << " (" << ud.shots << " shots)";
                    if (ud.flying)     ts << "  [Flying]";
                    if (ud.vampiric)   ts << "  [Vampiric]";
                    if (ud.regenerates) ts << "  [Regenerates]";
                    if (efficient)
                        ts << "  |  " << effectiveCostPerUnit << "g each (-20% Efficient)";
                    else
                        ts << "  |  " << costPerUnit << "g each";
                    rb.statTip = ts.str();
                    break;
                }
            }
        }

        std::string label = unitName
            + "  x" + std::to_string(dw.available)
            + "  (" + std::to_string(effectiveCostPerUnit * dw.available) + "g)"
            + (efficient ? " [Efficient]" : "");
        rb.btn = Button(label, {x, y, bw, 26.0f});
        rb.btn.colorBorder = UIColor::hex(UITheme::NATURE_GREEN, 0.6f);

        int capturedTier = dw.tier;
        UpgradePath capturedPath = dw.path;
        rb.btn.onClick = [this, capturedTier, capturedPath]{
            if (!m_town || !m_playerRes || !m_registry || !m_hero) return;
            // Find matching UnitDef before spending gold — avoids charging for units
            // that can't be added (no def, or army full with no matching stack)
            const UnitDef* matchedUd = nullptr;
            for (const auto& ud : m_registry->units()) {
                if (ud.faction == m_town->faction && ud.tier == capturedTier
                    && ud.path == capturedPath) {
                    matchedUd = &ud; break;
                }
            }
            if (!matchedUd) return;
            bool alreadyHasStack = false;
            for (const auto& s : m_hero->army)
                if (s.defId == matchedUd->id) { alreadyHasStack = true; break; }
            if (!alreadyHasStack && m_hero->army.size() >= 7) return;

            float costMult = m_hero->efficientSpecialty ? 0.8f : 1.0f;
            int recruited = m_town->recruit(capturedTier, 999, *m_playerRes, m_registry->units(), costMult);
            if (recruited > 0) {
                bool merged = false;
                for (auto& s : m_hero->army)
                    if (s.defId == matchedUd->id) { s.count += recruited; merged = true; break; }
                if (!merged)
                    m_hero->army.push_back({matchedUd->id, recruited});
            }
            rebuildRecruitButtons();
        };

        m_recruitBtns.push_back(rb);
        y += 30.0f;
        if (y + 26 > m_recruitPanel.bounds.bottom() - 4) break;
    }
}

void TownScreen::draw(UIRenderer& rdr)
{
    if (!m_open) return;

    // Dim background
    rdr.drawRect({0,0,(float)m_screenW,(float)m_screenH},
                 UIColor::rgba(0,0,0,0.6f));

    m_mainPanel.draw(rdr);
    drawBuildingTree(rdr);
    drawRecruitPanel(rdr);
    drawIncomePanel(rdr);
    m_closeBtn.draw(rdr);
    m_tooltip.draw(rdr);
}

void TownScreen::drawBuildingTree(UIRenderer& rdr)
{
    m_buildPanel.draw(rdr);
    for (auto& bb : m_buildBtns) bb.btn.draw(rdr);
}

void TownScreen::drawRecruitPanel(UIRenderer& rdr)
{
    m_recruitPanel.draw(rdr);
    if (m_recruitBtns.empty()) {
        rdr.drawText("No units available",
                     m_recruitPanel.bounds.x + 8,
                     m_recruitPanel.bounds.y + 40,
                     UIColor::hex(UITheme::TEXT_DISABLED), 12.0f);
    }
    for (auto& rb : m_recruitBtns) rb.btn.draw(rdr);

    // Hero army summary below recruit buttons
    if (m_hero && !m_hero->army.empty()) {
        float ax = m_recruitPanel.bounds.x + 8;
        float ay = m_recruitPanel.bounds.bottom() - 16.0f
                 - static_cast<float>(m_hero->army.size()) * 14.0f;
        rdr.drawText("Army:", ax, ay - 14.0f,
                     UIColor::hex(UITheme::TEXT_SECONDARY), 11.0f);
        for (auto& s : m_hero->army) {
            if (s.count <= 0) continue;
            std::string line = "  x" + std::to_string(s.count);
            if (m_registry) {
                for (const auto& ud : m_registry->units())
                    if (ud.id == s.defId) { line = ud.name + " x" + std::to_string(s.count); break; }
            }
            rdr.drawText(line, ax, ay, UIColor::hex(UITheme::TEXT_PRIMARY), 11.0f);
            ay += 14.0f;
        }
    }
}

void TownScreen::drawIncomePanel(UIRenderer& rdr)
{
    m_incomePanel.draw(rdr);
    if (!m_town) return;

    float x = m_incomePanel.bounds.x + 8;
    float y = m_incomePanel.bounds.y + 28;

    for (int i = 0; i < RESOURCE_COUNT; ++i) {
        int income = m_town->weeklyIncome.amounts[i];
        if (income == 0) continue;
        std::string line = std::string(resourceName(static_cast<ResourceType>(i)))
                         + ": +" + std::to_string(income) + "/week";
        rdr.drawText(line, x, y, UIColor::hex(UITheme::GOLD), 12.0f);
        y += 15.0f;
    }
}

bool TownScreen::onMouseMove(float x, float y) {
    if (!m_open) return false;
    m_closeBtn.onMouseMove(x, y);

    m_tooltip.hide();
    for (auto& bb : m_buildBtns) {
        bb.btn.onMouseMove(x, y);
        if (!bb.built && m_registry && bb.btn.bounds.contains(x, y)) {
            const BuildingDef* bd = m_registry->getBuildingDef(bb.buildingId);
            if (bd) {
                std::string tip = bd->name + ": ";
                if (!bd->description.empty()) tip += bd->description + "  |  ";
                tip += "Cost: ";
                bool first = true;
                for (int i = 0; i < RESOURCE_COUNT; ++i) {
                    int v = bd->cost.amounts[i];
                    if (v <= 0) continue;
                    if (!first) tip += ", ";
                    tip += std::to_string(v) + " " + resourceName(static_cast<ResourceType>(i));
                    first = false;
                }
                if (first) tip += "free";
                m_tooltip.show(tip, x, y - 10.0f);
            }
        }
    }

    for (auto& rb : m_recruitBtns) {
        rb.btn.onMouseMove(x, y);
        if (!rb.statTip.empty() && rb.btn.bounds.contains(x, y))
            m_tooltip.show(rb.statTip, x, y - 10.0f);
    }
    return m_mainPanel.bounds.contains(x, y);
}

bool TownScreen::onMouseDown(float x, float y) {
    if (!m_open) return false;
    if (m_closeBtn.onMouseDown(x, y)) return true;
    for (auto& bb : m_buildBtns)   if (bb.btn.onMouseDown(x, y)) return true;
    for (auto& rb : m_recruitBtns) if (rb.btn.onMouseDown(x, y)) return true;
    return m_mainPanel.bounds.contains(x, y);
}

bool TownScreen::onMouseUp(float x, float y) {
    if (!m_open) return false;
    m_closeBtn.onMouseUp(x, y);
    for (auto& bb : m_buildBtns)   bb.btn.onMouseUp(x, y);
    for (auto& rb : m_recruitBtns) rb.btn.onMouseUp(x, y);
    return m_mainPanel.bounds.contains(x, y);
}
