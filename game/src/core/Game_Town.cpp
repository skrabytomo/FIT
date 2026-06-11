#include "Game.h"
#include "../magic/SpellRegistry.h"
#include <imgui.h>
#include <stdio.h>
#include <unordered_map>
#include <cstdio>

// ── Town update ───────────────────────────────────────────────────────────────
void Game::updateTown(float dt)
{
    (void)dt;
    const auto& mouse = m_input.mouse();

    if (mouse.leftDown)
        m_townScreen.onMouseDown(static_cast<float>(mouse.x),
                                 static_cast<float>(mouse.y));
    m_townScreen.onMouseMove(static_cast<float>(mouse.x),
                             static_cast<float>(mouse.y));
}

// ── Town render ───────────────────────────────────────────────────────────────
void Game::renderTown()
{
    m_ui.beginFrame();
    m_townScreen.draw(m_ui);
    m_ui.endFrame();

    beginImGuiFrame();
    renderMageGuild();
    endImGuiFrame();
}

// ── Mage Guild overlay ────────────────────────────────────────────────────────
void Game::renderMageGuild()
{
    // Only show when the active town has a Mage Guild built
    if (m_heroes.empty() || m_towns.empty()) return;
    const Town* town = m_townScreen.currentTown();
    if (!town || !town->hasBuilding(BID::MAGE_GUILD)) return;

    Hero& hero = m_heroes[m_activeHeroIdx];

    // Faction spell offerings — 4 spells per faction, guild T1 shows first 2
    struct GuildEntry { int spellId; int goldCost; };
    using GE = GuildEntry;

    auto getEntries = [](FactionId f) -> std::vector<GuildEntry> {
        switch (f) {
        case FactionId::HolyOrder:     return {{SPL::BLESS,4000},{SPL::SMITE,5000},{SPL::DIVINE_SHIELD,4000},{SPL::RADIANCE,8000}};
        case FactionId::Bloodsworn:    return {{SPL::BLOOD_FRENZY,4000},{SPL::DRAIN_LIFE,5000},{SPL::ENERVATE,4500},{SPL::HEMORRHAGE,7000}};
        case FactionId::Thornkin:      return {{SPL::BARKSKIN,4000},{SPL::REGROWTH,5500},{SPL::CALL_LIGHTNING,6000},{SPL::ENTANGLE,4500}};
        case FactionId::EternalEmpire: return {{SPL::CURSE,4000},{SPL::WITHER,5000},{SPL::DEATH_COIL,6000},{SPL::PLAGUE,8000}};
        case FactionId::CrimsonWardens:return {{SPL::BLOOD_FRENZY,4000},{SPL::DRAIN_LIFE,5000},{SPL::WITHER,5000},{SPL::ENERVATE,4000}};
        case FactionId::Voidkin:       return {{SPL::ENTANGLE,4000},{SPL::CALL_LIGHTNING,6000},{SPL::CURSE,4500},{SPL::WITHER,5000}};
        case FactionId::IronAssembly:  return {{SPL::REINFORCE,4000},{SPL::OVERCLOCK,4500},{SPL::SHRAPNEL,6000},{SPL::HARDENED_SHELL,7000}};
        case FactionId::Amalgamate:    return {{SPL::MEND_FLESH,4000},{SPL::FESTER,5000},{SPL::TOXIN,4500},{SPL::GROWTH,7000}};
        case FactionId::Convergence:   return {{SPL::BLESS,4000},{SPL::CURSE,4000},{SPL::REINFORCE,4500},{SPL::REGROWTH,5500}};
        default:                       return {};
        }
    };
    auto entries = getEntries(town->faction);
    if (entries.empty()) return;

    const auto& entries_ref = entries;
    int available = town->hasBuilding(BID::MAGE_GUILD_T2) ? 4 : 2;

    ImGui::SetNextWindowPos(ImVec2(20, 80), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(280, 0), ImGuiCond_Always);
    if (!ImGui::Begin("Mage Guild", nullptr,
                      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                      ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End(); return;
    }

    ImGui::Text("Gold: %d", m_playerResources.get(ResourceType::Gold));
    ImGui::Separator();

    for (int i = 0; i < available && i < static_cast<int>(entries_ref.size()); ++i) {
        const SpellDef* sp = findSpell(entries_ref[i].spellId);
        if (!sp) continue;

        bool alreadyKnown = false;
        for (int sid : hero.knownSpells) if (sid == sp->id) { alreadyKnown = true; break; }

        ImGui::PushID(i);
        if (alreadyKnown) {
            ImGui::TextColored(ImVec4(0.4f,1.f,0.4f,1.f), "[known] %s", sp->name);
        } else {
            bool canAfford = m_playerResources.get(ResourceType::Gold) >= entries_ref[i].goldCost;
            if (!canAfford) ImGui::BeginDisabled();
            char btn[64];
            std::snprintf(btn, sizeof(btn), "Learn %s  (%dg)", sp->name, entries_ref[i].goldCost);
            if (ImGui::Button(btn, ImVec2(-1,0))) {
                hero.knownSpells.push_back(sp->id);
                m_playerResources.add(ResourceType::Gold, -entries_ref[i].goldCost);
            }
            if (!canAfford) ImGui::EndDisabled();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", sp->desc);
        ImGui::PopID();
    }
    ImGui::End();
}

// ── State transitions ─────────────────────────────────────────────────────────
void Game::enterTown(Town* town)
{
    if (!town) return;
    m_state = GameState::Town;
    m_townScreen.open(town, &m_playerResources, &m_registry);
    printf("Entered town: %s\n", town->name.c_str());
}

void Game::exitTown()
{
    m_townScreen.close();
    enterWorldMap();
}
