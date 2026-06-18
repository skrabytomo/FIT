#include "WorldMapHUD.h"
#include <string>
#include <sstream>

bool WorldMapHUD::init(int screenW, int screenH)
{
    buildLayout(screenW, screenH);
    return true;
}

void WorldMapHUD::resize(int screenW, int screenH)
{
    m_screenW = screenW; m_screenH = screenH;
    buildLayout(screenW, screenH);
}

void WorldMapHUD::buildLayout(int sw, int sh)
{
    m_screenW = sw; m_screenH = sh;

    // Top resource bar — taller for readability
    m_topBar = {0, 0, (float)sw, 68.0f};

    // Bottom bar
    m_bottomBar = {0, (float)sh - 52.0f, (float)sw, 48.0f};

    // End turn button — bottom right
    m_endTurnBtn = Button("End Turn",
        {(float)sw - 160.0f, (float)sh - 46.0f, 150.0f, 36.0f},
        [this]{ if (onEndTurn) onEndTurn(); });
    m_endTurnBtn.colorBorder  = UIColor::hex(UITheme::GOLD);
    m_endTurnBtn.colorText    = UIColor::hex(UITheme::GOLD);

    // Hero panel — right side
    m_heroPanel = Panel({(float)sw - 180.0f, 54.0f, 176.0f, 220.0f});
    m_heroPanel.title = "Heroes";

    // Town panel — below hero panel
    m_townPanel = Panel({(float)sw - 180.0f, 282.0f, 176.0f, 160.0f});
    m_townPanel.title = "Towns";
}

void WorldMapHUD::draw(UIRenderer& rdr,
                        const Resources& playerRes,
                        const Resources& weeklyIncome,
                        const TurnManager& turns,
                        const std::vector<Hero>& heroes,
                        int selectedHeroIdx,
                        const std::vector<Town>& towns)
{
    // Keep end-turn button label in sync with day counter
    m_endTurnBtn.text = "End Turn  [Day " + std::to_string(turns.day()) + "]";

    drawResourceBar(rdr, playerRes, weeklyIncome);
    drawDatePanel(rdr, turns);
    drawHeroPanel(rdr, heroes, selectedHeroIdx);
    drawTownPanel(rdr, towns);
    m_endTurnBtn.draw(rdr);
    m_tooltip.draw(rdr);
}

void WorldMapHUD::drawResourceBar(UIRenderer& rdr, const Resources& res,
                                   const Resources& income)
{
    // Background
    rdr.drawRect(m_topBar,
        UIColor::hex(UITheme::BG_PANEL_DARK, 0.95f),
        UIColor::hex(UITheme::BORDER), 1.0f);

    // Resource icons + values
    float x = 10.0f;
    float y = 5.0f;
    float spacing = 185.0f;

    struct ResDisplay { ResourceType type; unsigned color; };
    static const ResDisplay displays[] = {
        {ResourceType::Gold,         UITheme::GOLD},
        {ResourceType::Iron,         UITheme::IRON_GREY},
        {ResourceType::FaithStones,  0xE8E4FF},
        {ResourceType::BloodEssence, UITheme::BLOOD_RED},
        {ResourceType::VerdantSap,   UITheme::NATURE_GREEN},
        {ResourceType::Mercury,      UITheme::DEATH_TEAL},
    };

    // Resource icons in atlas: row 4, cols 0-5 (indices 32-37), atlas is 8×6 cells
    static const int resIconIdx[] = { 32, 33, 34, 35, 36, 37 };
    auto drawResIcon = [&](int atlasIdx, float ix, float iy, float sz) {
        if (!m_iconTex) return;
        float col = static_cast<float>(atlasIdx % 8);
        float row = static_cast<float>(atlasIdx / 8);
        ImVec2 uv0 = { col / 8.0f,          row / 6.0f };
        ImVec2 uv1 = { (col + 1.0f) / 8.0f, (row + 1.0f) / 6.0f };
        ImGui::GetBackgroundDrawList()->AddImage(m_iconTex, {ix, iy}, {ix+sz, iy+sz}, uv0, uv1);
    };

    int iconCount = 0;
    for (auto& d : displays) {
        int val = res.get(d.type);
        int inc = income.get(d.type);
        // Resource icon (32×32 sprite scaled to 32px, centered in bar)
        float iconSz = 40.0f;
        float iconY  = y + (46.0f - iconSz) * 0.5f;
        if (m_iconTex)
            drawResIcon(resIconIdx[iconCount], x, iconY, iconSz);
        else
            rdr.drawRect({x, y+2, 12.0f, 14.0f}, UIColor::hex(d.color));
        float textX = x + (m_iconTex ? iconSz + 4.0f : 16.0f);
        // Name + value
        std::string label = std::string(resourceName(d.type)) + ": " + std::to_string(val);
        rdr.drawText(label, textX, y + 4.0f, UIColor::hex(d.color), 15.0f);
        // Income per week below
        if (inc > 0) {
            std::string incStr = "+" + std::to_string(inc) + "/wk";
            rdr.drawText(incStr, textX, y + 22.0f,
                         UIColor::rgba(0.55f, 0.85f, 0.55f), 12.0f);
        }
        x += spacing;
        ++iconCount;
        if (x + spacing > m_screenW - 200.0f) break;
    }
}

void WorldMapHUD::drawDatePanel(UIRenderer& rdr, const TurnManager& turns)
{
    // Day/week display — bottom bar, left side (avoids overlapping resource icons)
    std::string date = "Week " + std::to_string(turns.week()) +
                       "  Day " + std::to_string(turns.day());
    float ty = static_cast<float>(m_screenH) - 38.0f;
    rdr.drawText(date, 14.0f, ty, UIColor::hex(UITheme::TEXT_SECONDARY), 16.0f);
}

void WorldMapHUD::drawHeroPanel(UIRenderer& rdr,
                                  const std::vector<Hero>& heroes, int sel)
{
    m_heroCount = static_cast<int>(heroes.size());
    m_heroPanel.draw(rdr);

    // Hint: click selected hero again or press F8 to open hero details
    rdr.drawText("Click selected / F8 = details",
                 m_heroPanel.bounds.x + 4.0f, m_heroPanel.bounds.y + 13.0f,
                 UIColor::rgba(0.5f, 0.5f, 0.5f, 0.8f), 9.0f);

    float y = m_heroPanel.bounds.y + 28.0f;
    float x = m_heroPanel.bounds.x + 4.0f;
    float w = m_heroPanel.bounds.w - 8.0f;

    auto* dl = ImGui::GetBackgroundDrawList();
    for (int i = 0; i < static_cast<int>(heroes.size()); ++i) {
        auto& h = heroes[i];
        float portSz = 52.0f;
        Rect btn{x, y, w, portSz + 8.0f};

        UIColor bg = (i == sel) ?
            UIColor::hex(UITheme::BG_HOVER) :
            UIColor::hex(UITheme::BG_PANEL_DARK);
        UIColor brd = (i == sel) ?
            UIColor::hex(UITheme::GOLD) :
            UIColor::hex(UITheme::BORDER);

        rdr.drawRect(btn, bg, brd, 1.0f);

        // Portrait: faction unit image on the left
        float portX = x + 2.0f;
        float portY = y + 4.0f;
        int fid = static_cast<int>(h.faction);
        ImTextureID portTex = (fid >= 0 && fid < 9) ? m_portraitTex[fid] : nullptr;
        if (portTex) {
            dl->AddImage(portTex, {portX, portY}, {portX + portSz, portY + portSz});
            dl->AddRect({portX, portY}, {portX + portSz, portY + portSz},
                        IM_COL32(180, 150, 80, 200), 2.0f);
        } else {
            rdr.drawRect({portX, portY, portSz, portSz},
                         UIColor::hex(0x223344), UIColor::hex(UITheme::BORDER), 1.0f);
        }

        float tx = portX + portSz + 4.0f;
        float tw = btn.x + btn.w - tx - 2.0f;

        // Hero name + level
        std::string label = h.name + " L" + std::to_string(h.level);
        rdr.drawText(label, tx, y + 4.0f,
                     UIColor::hex(UITheme::TEXT_PRIMARY), 11.0f);

        // Army count + mana
        int armyTotal = 0;
        for (const auto& s : h.army) armyTotal += s.count;
        std::string stats = "A:" + std::to_string(armyTotal)
                          + " MP:" + std::to_string(h.mana) + "/" + std::to_string(h.maxMana);
        rdr.drawText(stats, tx, y + 18.0f,
                     UIColor::hex(0x88AAFF), 10.0f);

        // Move bar (green)
        float moveFrac = h.maxMove > 0 ?
            static_cast<float>(h.movePool) / h.maxMove : 0.0f;
        Rect movebar{tx, y + 34.0f, tw, 4.0f};
        rdr.drawBar(movebar, moveFrac,
                    UIColor::hex(UITheme::NATURE_GREEN),
                    UIColor::hex(UITheme::BG_DARK),
                    UIColor::hex(UITheme::BORDER));

        // XP bar (purple)
        float xpFrac = h.xpToNext > 0 ?
            static_cast<float>(h.xp) / h.xpToNext : 1.0f;
        Rect xpbar{tx, y + 42.0f, tw, 4.0f};
        rdr.drawBar(xpbar, xpFrac,
                    UIColor::hex(0xAA55FF),
                    UIColor::hex(UITheme::BG_DARK),
                    UIColor::hex(UITheme::BORDER));

        y += portSz + 12.0f;
    }
}

void WorldMapHUD::drawTownPanel(UIRenderer& rdr, const std::vector<Town>& towns)
{
    // Collect only player-owned towns (ownerId == 1)
    std::vector<const Town*> playerTowns;
    for (const auto& t : towns)
        if (t.ownerId == 1) playerTowns.push_back(&t);

    m_townCount = static_cast<int>(playerTowns.size());
    if (m_townCount == 0) return;

    // Resize panel height to fit towns
    float rowH = 28.0f;
    float panelH = 26.0f + m_townCount * (rowH + 4.0f);
    m_townPanel.bounds.h = panelH;
    m_townPanel.draw(rdr);

    float y = m_townPanel.bounds.y + 26.0f;
    float x = m_townPanel.bounds.x + 4.0f;
    float w = m_townPanel.bounds.w - 8.0f;

    auto* dl = ImGui::GetBackgroundDrawList();
    const float icoSz = 20.0f;
    for (int i = 0; i < m_townCount; ++i) {
        const Town* t = playerTowns[i];
        Rect btn{x, y, w, rowH};
        rdr.drawRect(btn,
            UIColor::hex(UITheme::BG_PANEL_DARK),
            UIColor::hex(UITheme::GOLD), 1.0f);

        // Castle icon from atlas (ICO_TOWN_PLAYER = 2: col 2, row 0 in 8×6 grid)
        float iy = y + (rowH - icoSz) * 0.5f;
        if (m_iconTex) {
            ImVec2 uv0 = { 2.0f / 8.0f, 0.0f / 6.0f };
            ImVec2 uv1 = { 3.0f / 8.0f, 1.0f / 6.0f };
            dl->AddImage(m_iconTex, {x + 2.0f, iy}, {x + 2.0f + icoSz, iy + icoSz}, uv0, uv1);
        } else {
            // Fallback procedural castle dot
            dl->AddRectFilled({x + 3.0f, iy + 3.0f}, {x + 3.0f + 14.0f, iy + 14.0f},
                              IM_COL32(120, 180, 255, 200), 2.0f);
        }

        rdr.drawText(t->name, x + icoSz + 6.0f, y + 6.0f,
                     UIColor::hex(UITheme::TEXT_PRIMARY), 11.0f);
        y += rowH + 4.0f;
    }
}

bool WorldMapHUD::onMouseMove(float x, float y) {
    m_endTurnBtn.onMouseMove(x, y);
    return false;
}
bool WorldMapHUD::onMouseDown(float x, float y) {
    if (m_endTurnBtn.onMouseDown(x, y)) return true;

    // Hero panel click — pick which hero entry was hit
    if (onHeroClicked && m_heroCount > 0) {
        float px = m_heroPanel.bounds.x + 4.0f;
        float py = m_heroPanel.bounds.y + 28.0f;
        float pw = m_heroPanel.bounds.w - 8.0f;
        float ph = 48.0f;
        float gap = 52.0f;
        if (x >= px && x <= px + pw) {
            for (int i = 0; i < m_heroCount; ++i) {
                float ey = py + i * gap;
                if (y >= ey && y <= ey + ph) {
                    onHeroClicked(i);
                    return true;
                }
            }
        }
    }

    // Town panel click — jump camera to town
    if (onTownClicked && m_townCount > 0) {
        float px = m_townPanel.bounds.x + 4.0f;
        float py = m_townPanel.bounds.y + 26.0f;
        float pw = m_townPanel.bounds.w - 8.0f;
        float rowH = 28.0f;
        float gap  = rowH + 4.0f;
        if (x >= px && x <= px + pw) {
            for (int i = 0; i < m_townCount; ++i) {
                float ey = py + i * gap;
                if (y >= ey && y <= ey + rowH) {
                    onTownClicked(i);
                    return true;
                }
            }
        }
    }

    return false;
}
bool WorldMapHUD::onMouseUp(float x, float y) {
    return m_endTurnBtn.onMouseUp(x, y);
}
