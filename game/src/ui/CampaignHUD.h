#pragma once
#include "../campaign/CampaignManager.h"

// ImGui-based Campaign HUD.
// Shows: mission briefing (on start), objective tracker, alignment compass,
// decision modal (blocks input when a decision is pending), and end-campaign screen.
class CampaignHUD
{
public:
    void render(CampaignManager& mgr, LuaEngine& lua);

private:
    void drawObjectives(const CampaignMission& mission);
    void drawAlignmentCompass(const AlignmentSystem& align);
    void drawDecisionModal(CampaignManager& mgr, LuaEngine& lua);
    void drawEndScreen(CampaignManager& mgr, bool convergenceEligible);

    bool m_showBriefing   = false;
    int  m_lastMissionId  = -1;
};
