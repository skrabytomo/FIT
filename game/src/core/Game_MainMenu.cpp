#include "Game.h"
#include <imgui.h>

void Game::updateMainMenu(float dt)
{
    (void)dt;
}

void Game::renderMainMenu()
{
    beginImGuiFrame();

    ImGuiIO& io = ImGui::GetIO();
    float cx = io.DisplaySize.x * 0.5f;
    float cy = io.DisplaySize.y * 0.5f;

    ImGui::SetNextWindowPos(ImVec2(cx, cy), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(340, 300), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.88f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar;
    if (ImGui::Begin("##mainmenu", nullptr, flags)) {
        float bw = ImGui::GetWindowWidth() - 32.0f;

        // Title
        const char* title = "UNNAMED STRATEGY";
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(title).x) * 0.5f);
        ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.2f, 1.0f), "%s", title);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("New Game", ImVec2(bw, 38))) {
            m_state = GameState::WorldMap;
        }
        ImGui::Spacing();

        if (ImGui::Button("Load Game", ImVec2(bw, 38))) {
            if (loadGame("saves/save0.json"))
                m_state = GameState::WorldMap;
        }
        ImGui::Spacing();

        if (ImGui::Button("Map Editor", ImVec2(bw, 38))) {
            enterEditor();
        }
        ImGui::Spacing();

        if (ImGui::Button("Quit", ImVec2(bw, 38))) {
            m_running = false;
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::SetCursorPosX(8.0f);
        ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.45f, 1.0f),
                           "F5 Save  F9 Load  F2 Editor  F4 Campaign");
    }
    ImGui::End();

    endImGuiFrame();
}
