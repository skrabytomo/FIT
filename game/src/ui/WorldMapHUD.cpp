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

    // Top resource bar
    m_topBar = {0, 0, (float)sw, 32.0f};

    // Bottom bar
    m_bottomBar = {0, (float)sh - 40.0f, (float)sw, 40.0f};

    // End turn button — bottom right
    m_endTurnBtn = Button("End Turn",
        {(float)sw - 130.0f, (float)sh - 35.0f, 120.0f, 30.0f},
        [this]{ if (onEndTurn) onEndTurn(); });
    m_endTurnBtn.colorBorder  = UIColor::hex(UITheme::GOLD);
    m_endTurnBtn.colorText    = UIColor::hex(UITheme::GOLD);

    // Hero panel — right side
    m_heroPanel = Panel({(float)sw - 140.0f, 36.0f, 136.0f, 200.0f});
    m_heroPanel.title = "Heroes";
}

void WorldMapHUD::draw(UIRenderer& rdr,
                        const Resources& playerRes,
                        const TurnManager& turns,
                        const std::vector<Hero>& heroes,
                        int selectedHeroIdx)
{
    drawResourceBar(rdr, playerRes);
    drawDatePanel(rdr, turns);
    drawHeroPanel(rdr, heroes, selectedHeroIdx);
    m_endTurnBtn.draw(rdr);
    m_tooltip.draw(rdr);
}

void WorldMapHUD::drawResourceBar(UIRenderer& rdr, const Resources& res)
{
    // Background
    rdr.drawRect(m_topBar,
        UIColor::hex(UITheme::BG_PANEL_DARK, 0.95f),
        UIColor::hex(UITheme::BORDER), 1.0f);

    // Resource icons + values
    float x = 8.0f;
    float y = 8.0f;
    float spacing = 120.0f;

    struct ResDisplay { ResourceType type; unsigned color; };
    static const ResDisplay displays[] = {
        {ResourceType::Gold,         UITheme::GOLD},
        {ResourceType::Iron,         UITheme::IRON_GREY},
        {ResourceType::FaithStones,  0xE8E4FF},
        {ResourceType::BloodEssence, UITheme::BLOOD_RED},
        {ResourceType::VerdantSap,   UITheme::NATURE_GREEN},
        {ResourceType::Mercury,      UITheme::DEATH_TEAL},
    };

    for (auto& d : displays) {
        int val = res.get(d.type);
        // Colored dot
        rdr.drawRect({x, y+2, 10.0f, 12.0f}, UIColor::hex(d.color));
        // Name + value
        std::string label = std::string(resourceName(d.type)) + ": " + std::to_string(val);
        rdr.drawText(label, x + 14.0f, y, UIColor::hex(d.color), 12.0f);
        x += spacing;
        if (x + spacing > m_screenW - 200.0f) break;
    }
}

void WorldMapHUD::drawDatePanel(UIRenderer& rdr, const TurnManager& turns)
{
    // Day/week display — top center
    std::string date = "Week " + std::to_string(turns.week()) +
                       "  Day " + std::to_string(turns.day());
    float tw = date.size() * 7.5f;
    float tx = (m_screenW - tw) * 0.5f;
    rdr.drawText(date, tx, 9.0f, UIColor::hex(UITheme::TEXT_SECONDARY), 12.0f);
}

void WorldMapHUD::drawHeroPanel(UIRenderer& rdr,
                                  const std::vector<Hero>& heroes, int sel)
{
    m_heroPanel.draw(rdr);

    float y = m_heroPanel.bounds.y + 28.0f;
    float x = m_heroPanel.bounds.x + 4.0f;
    float w = m_heroPanel.bounds.w - 8.0f;

    for (int i = 0; i < static_cast<int>(heroes.size()); ++i) {
        auto& h = heroes[i];
        Rect btn{x, y, w, 28.0f};

        UIColor bg = (i == sel) ?
            UIColor::hex(UITheme::BG_HOVER) :
            UIColor::hex(UITheme::BG_PANEL_DARK);
        UIColor brd = (i == sel) ?
            UIColor::hex(UITheme::GOLD) :
            UIColor::hex(UITheme::BORDER);

        rdr.drawRect(btn, bg, brd, 1.0f);

        // Hero name + level
        std::string label = h.name + " L" + std::to_string(h.level);
        rdr.drawText(label, x + 4.0f, y + 7.0f,
                     UIColor::hex(UITheme::TEXT_PRIMARY), 11.0f);

        // Movement bar
        float moveFrac = h.maxMove > 0 ?
            static_cast<float>(h.movePool) / h.maxMove : 0.0f;
        Rect movebar{x + 4.0f, y + 20.0f, w - 8.0f, 4.0f};
        rdr.drawBar(movebar, moveFrac,
                    UIColor::hex(UITheme::NATURE_GREEN),
                    UIColor::hex(UITheme::BG_DARK),
                    UIColor::hex(UITheme::BORDER));
        y += 32.0f;
    }
}

bool WorldMapHUD::onMouseMove(float x, float y) {
    m_endTurnBtn.onMouseMove(x, y);
    return false;
}
bool WorldMapHUD::onMouseDown(float x, float y) {
    return m_endTurnBtn.onMouseDown(x, y);
}
bool WorldMapHUD::onMouseUp(float x, float y) {
    return m_endTurnBtn.onMouseUp(x, y);
}
