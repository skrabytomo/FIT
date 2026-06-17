#include "Game.h"
#include "../data/SaveLoad.h"
#include "../hero/HeroClass.h"
#include <imgui.h>
#include <string>
#include <cstdio>

// ── Save slot metadata ────────────────────────────────────────────────────────
struct SlotMeta { bool exists = false; std::string heroName; std::string factionName; int day = 0, week = 0; };

static const char* factionShortName(int factionId)
{
    switch (factionId) {
    case 0: return "Holy Order";
    case 1: return "Crimson Wardens";
    case 2: return "Thornkin";
    case 3: return "Eternal Empire";
    case 4: return "Bloodsworn";
    case 5: return "Voidkin";
    case 6: return "Iron Assembly";
    case 7: return "Amalgamate";
    case 8: return "Convergence";
    default: return "Unknown";
    }
}

static SlotMeta readSlotMeta(int slot)
{
    SlotMeta m;
    GameSaveData data;
    std::string path = "saves/save" + std::to_string(slot) + ".json";
    if (!SaveLoad::loadGame(path, data)) return m;
    m.exists      = true;
    m.day         = data.day;
    m.week        = data.week;
    if (!data.heroes.empty()) {
        m.heroName    = data.heroes[0].name;
        m.factionName = factionShortName(data.heroes[0].faction);
    } else {
        m.heroName    = "Unknown";
        m.factionName = "";
    }
    return m;
}

void Game::updateMainMenu(float dt) { (void)dt; }

void Game::renderMainMenu()
{
    beginImGuiFrame();

    ImGuiIO& io = ImGui::GetIO();
    float cx = io.DisplaySize.x * 0.5f;
    float cy = io.DisplaySize.y * 0.5f;

    ImGui::SetNextWindowPos(ImVec2(cx, cy), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGuiWindowFlags wf = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                          ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize;

    if (!ImGui::Begin("##mainmenu", nullptr, wf)) { ImGui::End(); endImGuiFrame(); return; }

    float bw = ImGui::GetWindowWidth() - 32.0f;

    auto header = [&](const char* text) {
        float tw = ImGui::CalcTextSize(text).x;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - tw) * 0.5f);
        ImGui::TextColored({1.0f, 0.82f, 0.2f, 1.0f}, "%s", text);
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    };

    // ── 0: Main ──────────────────────────────────────────────────────────────
    if (m_menuMode == 0) {
        header("UNNAMED STRATEGY");

        if (ImGui::Button("New Game",   ImVec2(bw, 40))) m_menuMode = 1;
        ImGui::Spacing();
        if (ImGui::Button("Load Game",  ImVec2(bw, 40))) m_menuMode = 2;
        ImGui::Spacing();
        if (ImGui::Button("Settings",   ImVec2(bw, 40))) m_menuMode = 3;
        ImGui::Spacing();
        if (ImGui::Button("Map Editor", ImVec2(bw, 40))) { enterEditor(); }
        ImGui::Spacing();
        if (ImGui::Button("Quit",       ImVec2(bw, 40))) m_running = false;

        ImGui::Spacing(); ImGui::Separator();
        ImGui::TextColored({0.4f, 0.4f, 0.4f, 1.0f}, "F5 Save  F9 Load  F2 Editor  F4 Campaign");
    }
    // ── 1: New Game — setup + slot picker ────────────────────────────────────
    else if (m_menuMode == 1) {
        header("New Game");

        // Map size
        ImGui::Text("Map Size:");
        static const char* kMapSizeLabels[] = { "Small (24)", "Medium (36)", "Large (52)", "XLarge (72)" };
        for (int i = 0; i < 4; ++i) {
            if (i > 0) ImGui::SameLine();
            bool sel = (m_newGameMapSize == i);
            if (sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.3f, 0.1f, 1.f));
            char msLbl[32]; std::snprintf(msLbl, sizeof(msLbl), "%s##ms%d", kMapSizeLabels[i], i);
            if (ImGui::Button(msLbl, ImVec2((bw - 6) / 4.f, 26))) m_newGameMapSize = i;
            if (sel) ImGui::PopStyleColor();
        }
        ImGui::Spacing();

        // Faction
        ImGui::Text("Faction:");
        static const char* kFacNames[] = {
            "Holy Order","Crimson Wardens","Thornkin","Eternal Empire",
            "Bloodsworn","Voidkin","Iron Assembly","Amalgamate","Convergence"
        };
        for (int i = 0; i < 9; ++i) {
            if (i % 3 != 0) ImGui::SameLine();
            bool sel = (m_newGameFaction == i);
            if (sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.3f, 0.1f, 1.f));
            char fLbl[40]; std::snprintf(fLbl, sizeof(fLbl), "%s##fc%d", kFacNames[i], i);
            if (ImGui::Button(fLbl, ImVec2((bw - 4) / 3.f, 26))) {
                m_newGameFaction = i;
                m_newGameClassId = 0;  // reset class selection on faction change
            }
            if (sel) ImGui::PopStyleColor();
        }
        ImGui::Spacing();

        // Hero class selection for chosen faction
        {
            FactionId f = static_cast<FactionId>(m_newGameFaction);
            auto classes = m_classRegistry.getClassesForFaction(f);
            if (!classes.empty()) {
                ImGui::Text("Hero Class:");
                // Ensure m_newGameClassId is valid
                bool classValid = false;
                for (auto* c : classes) if (c->id == m_newGameClassId) { classValid = true; break; }
                if (!classValid) m_newGameClassId = classes[0]->id;

                for (int ci = 0; ci < static_cast<int>(classes.size()); ++ci) {
                    const HeroClassDef* cls = classes[ci];
                    if (ci % 2 != 0) ImGui::SameLine();
                    bool sel = (m_newGameClassId == cls->id);
                    if (sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.2f, 1.f));
                    char clbl[48]; std::snprintf(clbl, sizeof(clbl), "%s##cl%d", cls->name.c_str(), cls->id);
                    if (ImGui::Button(clbl, ImVec2((bw - 4) / 2.f, 26)))
                        m_newGameClassId = cls->id;
                    if (ImGui::IsItemHovered() && !cls->specialtyDesc.empty())
                        ImGui::SetTooltip("Specialty: %s", cls->specialtyDesc.c_str());
                    if (sel) ImGui::PopStyleColor();
                }
            }
        }
        ImGui::Spacing();

        // Difficulty
        ImGui::Text("Difficulty:");
        static const char* kDiffNames[]    = { "Easy", "Normal", "Hard" };
        static const char* kDiffTooltips[] = {
            "Easy: Player heroes gain +2 ATK/DEF, enemies are weaker. Good for learning.",
            "Normal: Balanced gameplay. Recommended for most players.",
            "Hard: Enemy heroes are stronger and more aggressive. For veterans."
        };
        for (int i = 0; i < 3; ++i) {
            if (i > 0) ImGui::SameLine();
            bool sel = (m_newGameDifficulty == i);
            if (sel) ImGui::PushStyleColor(ImGuiCol_Button,
                i == 0 ? ImVec4(0.1f, 0.4f, 0.1f, 1.f) :
                i == 1 ? ImVec4(0.4f, 0.3f, 0.1f, 1.f) :
                         ImVec4(0.5f, 0.1f, 0.1f, 1.f));
            char dlbl[24]; std::snprintf(dlbl, sizeof(dlbl), "%s##df%d", kDiffNames[i], i);
            if (ImGui::Button(dlbl, ImVec2((bw - 4) / 3.f, 26))) m_newGameDifficulty = i;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", kDiffTooltips[i]);
            if (sel) ImGui::PopStyleColor();
        }
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        ImGui::TextDisabled("Choose a slot. Existing save will be overwritten.");
        ImGui::Spacing();

        for (int s = 0; s < 3; ++s) {
            SlotMeta meta = readSlotMeta(s);
            char lbl[200];
            if (meta.exists)
                std::snprintf(lbl, sizeof(lbl),
                    "Slot %d  |  %s  (%s)  Day %d  Week %d  [overwrite]##ng%d",
                    s + 1, meta.heroName.c_str(), meta.factionName.c_str(),
                    meta.day, meta.week, s);
            else
                std::snprintf(lbl, sizeof(lbl), "Slot %d  |  Empty##ng%d", s + 1, s);

            ImVec4 tc = meta.exists ? ImVec4(1.0f, 0.65f, 0.15f, 1.0f)
                                    : ImVec4(0.5f, 0.9f,  0.5f,  1.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, tc);
            if (ImGui::Button(lbl, ImVec2(bw, 36))) {
                m_activeSlot = s;
                startNewGame();
                m_state    = GameState::WorldMap;
                m_menuMode = 0;
            }
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }
        ImGui::Separator(); ImGui::Spacing();
        if (ImGui::Button("Back##ng", ImVec2(bw, 30))) m_menuMode = 0;
    }
    // ── 2: Load Game — slot list ──────────────────────────────────────────────
    else if (m_menuMode == 2) {
        header("Load Game");
        bool anySave = false;

        for (int s = 0; s < 3; ++s) {
            SlotMeta meta = readSlotMeta(s);
            if (!meta.exists) {
                ImGui::TextDisabled("Slot %d  |  Empty", s + 1);
                ImGui::Spacing();
                continue;
            }
            anySave = true;
            char lbl[200];
            std::snprintf(lbl, sizeof(lbl),
                "Slot %d  |  %s  (%s)  Day %d  Week %d##ld%d",
                s + 1, meta.heroName.c_str(), meta.factionName.c_str(),
                meta.day, meta.week, s);
            if (ImGui::Button(lbl, ImVec2(bw, 36))) {
                m_activeSlot = s;
                std::string path = "saves/save" + std::to_string(s) + ".json";
                if (loadGame(path)) {
                    m_state    = GameState::WorldMap;
                    m_menuMode = 0;
                }
            }
            ImGui::Spacing();
        }
        if (!anySave) { ImGui::Spacing(); ImGui::TextDisabled("No saves found."); ImGui::Spacing(); }
        ImGui::Separator(); ImGui::Spacing();
        if (ImGui::Button("Back##ld", ImVec2(bw, 30))) m_menuMode = 0;
    }
    // ── 3: Settings ───────────────────────────────────────────────────────────
    else if (m_menuMode == 3) {
        header("Settings");

        ImGui::Text("Audio");
        ImGui::Separator();
        if (ImGui::SliderFloat("Music Volume", &m_settingsMasVol, 0.0f, 1.0f, "%.2f"))
            m_audio.setMusicVolume(m_settingsMasVol);
        if (ImGui::SliderFloat("SFX Volume",   &m_settingsSfxVol, 0.0f, 1.0f, "%.2f"))
            m_audio.setSfxVolume(m_settingsSfxVol);

        ImGui::Spacing();
        ImGui::Text("Display");
        ImGui::Separator();
        if (ImGui::Checkbox("Fullscreen", &m_settingsFullscreen))
            SDL_SetWindowFullscreen(m_window,
                m_settingsFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
        ImGui::Checkbox("Floating Combat Numbers", &m_settingsShowDmgNums);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Show damage numbers floating above units during combat.");

        ImGui::Spacing();
        ImGui::Text("Gameplay");
        ImGui::Separator();
        ImGui::Checkbox("Auto-Save at Week End", &m_settingsAutoSave);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Automatically save to the active slot at the start of each new week.");
        ImGui::SliderFloat("Combat Anim Speed", &m_settingsAnimSpeed, 0.5f, 2.0f, "%.1fx");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Adjust combat animation speed. 1.0 = normal, 2.0 = double speed.");

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        if (ImGui::Button("Save & Back", ImVec2(bw * 0.55f, 34))) {
            saveSettings();
            m_menuMode = 0;
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard", ImVec2(-1, 34))) {
            loadSettings();   // reload from disk to undo in-session changes
            m_menuMode = 0;
        }
    }

    ImGui::End();
    endImGuiFrame();
}
