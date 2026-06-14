#pragma once
#include "Widgets.h"
#include "../data/Resources.h"
#include "../hero/Hero.h"
#include "../core/TurnManager.h"
#include <vector>
#include <functional>

class WorldMapHUD
{
public:
    bool init(int screenW, int screenH);
    void resize(int screenW, int screenH);

    void draw(UIRenderer& rdr,
              const Resources& playerRes,
              const Resources& weeklyIncome,
              const TurnManager& turns,
              const std::vector<Hero>& heroes,
              int  selectedHeroIdx);

    bool onMouseMove(float x, float y);
    bool onMouseDown(float x, float y);
    bool onMouseUp(float x, float y);

    UICallback onEndTurn;
    UIIntCallback onHeroClicked;  // index into heroes list

private:
    void buildLayout(int sw, int sh);
    void drawResourceBar(UIRenderer& rdr, const Resources& res, const Resources& income);
    void drawHeroPanel(UIRenderer& rdr, const std::vector<Hero>& heroes, int sel);
    void drawDatePanel(UIRenderer& rdr, const TurnManager& turns);

    int m_screenW = 1280, m_screenH = 720;

    // Top bar
    Rect m_topBar;

    // Bottom bar
    Rect m_bottomBar;
    Button m_endTurnBtn;

    // Hero list (right side)
    Panel m_heroPanel;
    std::vector<Button> m_heroBtns;
    int   m_heroCount = 0;  // updated each draw(); used for click detection

    TooltipWidget m_tooltip;
};
