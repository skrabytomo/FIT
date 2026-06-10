#include "Game.h"
#include <imgui.h>
#include <stdio.h>

// ── Editor update ─────────────────────────────────────────────────────────────
void Game::updateEditor(float dt)
{
    (void)dt;
    const auto& mouse = m_input.mouse();
    if (mouse.wheelY != 0.0f)
        m_camera.zoomBy(mouse.wheelY > 0 ? 1.12f : 0.88f);
    if (mouse.middle)
        m_camera.pan(-static_cast<float>(mouse.dx), -static_cast<float>(mouse.dy));

    const float PAN = 200.0f * (1.0f / 60.0f);
    if (m_input.keyHeld(SDLK_LEFT))  m_camera.pan(-PAN, 0);
    if (m_input.keyHeld(SDLK_RIGHT)) m_camera.pan( PAN, 0);
    if (m_input.keyHeld(SDLK_UP))    m_camera.pan(0, -PAN);
    if (m_input.keyHeld(SDLK_DOWN))  m_camera.pan(0,  PAN);

    float wx, wy;
    m_camera.screenToWorld(static_cast<float>(mouse.x),
                           static_cast<float>(mouse.y), wx, wy);
    HexCoord h = m_hexRenderer.grid().worldToHex(wx, wy);
    m_hovered = m_map.inBounds(h) ? h : HexCoord{-999,-999};

    if (mouse.leftDown && !ImGui::GetIO().WantCaptureMouse) {
        if (m_map.inBounds(m_hovered))
            m_editor.onHexClicked(m_hovered, m_map, m_towns,
                                  m_resources, m_heroStarts);
    }

    if (m_input.keyDown(SDLK_F3))
        m_simWindow.setOpen(!m_simWindow.isOpen());
}

// ── Editor render ─────────────────────────────────────────────────────────────
void Game::renderEditor()
{
    m_hexRenderer.render(m_map, m_camera, m_hovered, {-999,-999});

    for (auto& s : m_heroStarts) {
        float wx, wy;
        m_hexRenderer.grid().hexToWorld(s, wx, wy);
        (void)wx; (void)wy;
    }

    beginImGuiFrame();
    m_editor.renderImGui(m_map, m_towns, m_resources, m_heroStarts);
    m_simWindow.render();
    endImGuiFrame();
}

// ── State transitions ─────────────────────────────────────────────────────────
void Game::enterEditor()
{
    m_state = GameState::Editor;
    printf("Entered map editor (F2 to exit)\n");
}

void Game::exitEditor()
{
    m_state = GameState::WorldMap;
    printf("Exited map editor\n");
}
