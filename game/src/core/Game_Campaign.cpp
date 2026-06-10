#include "Game.h"
#include <stdio.h>

// ── Campaign update ───────────────────────────────────────────────────────────
void Game::updateCampaign(float dt)
{
    // Campaign runs on top of the world map — delegate all input and movement.
    // SPACE (end turn / week advance) and hero movement are handled there.
    updateWorldMap(dt);
}

// ── Campaign render ───────────────────────────────────────────────────────────
void Game::renderCampaign()
{
    // Render world map as backdrop, then campaign overlay
    m_hexRenderer.render(m_map, m_camera, m_hovered, {-999,-999});

    beginImGuiFrame();
    m_campaignHUD.render(m_campaign, m_lua);
    endImGuiFrame();
}

// ── State transitions ─────────────────────────────────────────────────────────
void Game::enterCampaign()
{
    m_state = GameState::Campaign;
    m_campaign.init();
    m_campaign.setEventCallback([this](CampaignEvent e) {
        if (e == CampaignEvent::MissionCompleted)
            printf("[Campaign] Mission complete!\n");
        else if (e == CampaignEvent::MissionFailed)
            printf("[Campaign] Mission failed.\n");
        else if (e == CampaignEvent::CampaignEnded) {
            bool convergenceOk = m_hideout.isConvergenceUnlocked();
            FactionId unlocked = m_campaign.unlockedFaction(convergenceOk);
            printf("[Campaign] Ended — faction unlocked: %d\n",
                   static_cast<int>(unlocked));
            if (convergenceOk)
                m_hideout.completeMilestone("convergence_unlock");
        }
    });
    printf("Entered Campaign (F4 to exit)\n");
}

void Game::exitCampaign()
{
    m_state = GameState::WorldMap;
    printf("Exited Campaign\n");
}
