#include "Game.h"
#include <cstdio>
#include "../magic/SpellRegistry.h"
#include "../world/HexGrid.h"
#include "../world/FogOfWar.h"
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
    m_ui.flushText(ImGui::GetBackgroundDrawList());
    renderMageGuild();
    renderTavern();
    if (m_showCapturePopup) renderCapturePopup();
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

// ── Tavern — hire a hero ──────────────────────────────────────────────────────
void Game::renderTavern()
{
    const Town* town = m_townScreen.currentTown();
    if (!town || town->ownerId != 1) return;  // only in owned towns

    static constexpr int HIRE_COST = 2500;
    static constexpr int MAX_HEROES = 3;

    ImGui::SetNextWindowPos(ImVec2(310, 80), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(260, 0), ImGuiCond_Always);
    if (!ImGui::Begin("Tavern", nullptr,
                      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                      ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End(); return;
    }

    ImGui::Text("Gold: %d", m_playerResources.get(ResourceType::Gold));
    ImGui::Separator();

    if (static_cast<int>(m_heroes.size()) >= MAX_HEROES) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                           "Hero roster full (%d/%d).", MAX_HEROES, MAX_HEROES);
        ImGui::End(); return;
    }

    bool canAfford = m_playerResources.get(ResourceType::Gold) >= HIRE_COST;
    if (!canAfford) ImGui::BeginDisabled();

    char label[64];
    std::snprintf(label, sizeof(label), "Hire Hero  (%dg)", HIRE_COST);
    if (ImGui::Button(label, ImVec2(-1, 34))) {
        m_playerResources.add(ResourceType::Gold, -HIRE_COST);

        // Find a free adjacent tile to the town
        HexCoord spawnPos = town->pos;
        for (auto& nb : HexGrid::neighbors(town->pos)) {
            const HexTile* t = m_map.getTile(nb);
            if (t && t->terrain != Terrain::Water && t->heroId == 0) {
                spawnPos = nb;
                break;
            }
        }

        // Name pool per faction
        static const char* kNames[] = {
            "Alara", "Dren", "Korvas", "Mira", "Seld", "Thayne",
            "Vex", "Lyra", "Brant", "Cael", "Essen", "Fynn"
        };
        uint32_t nameIdx = static_cast<uint32_t>(m_heroes.size() + m_turns.day() * 7)
                           % 12;

        Hero hired;
        hired.id       = 200u + static_cast<uint32_t>(m_heroes.size());
        hired.name     = kNames[nameIdx];
        hired.faction  = town->faction;
        hired.pos      = spawnPos;
        hired.movePool = hired.maxMove;

        // Starting spell for faction
        static const int kStartSpell[] = {
            SPL::BLESS,        // HolyOrder
            SPL::BLOOD_FRENZY, // CrimsonWardens
            SPL::ENTANGLE,     // Thornkin
            SPL::CURSE,        // EternalEmpire
            SPL::BLOOD_FRENZY, // Bloodsworn
            SPL::ENTANGLE,     // Voidkin
            SPL::REINFORCE,    // IronAssembly
            SPL::MEND_FLESH,   // Amalgamate
            SPL::BLESS,        // Convergence
        };
        int fi = static_cast<int>(town->faction);
        if (fi >= 0 && fi < 9) hired.knownSpells.push_back(kStartSpell[fi]);

        // Starting army: faction-scaled from dwelling weekly growth
        // T1: full week's growth  T2: one-third week's growth
        for (int tier : {1, 2}) {
            // Find weekly growth for this tier's base dwelling
            int growth = 0;
            for (const auto& bd : m_registry.buildings()) {
                if (bd.faction == hired.faction && bd.tier == tier
                    && bd.category == BuildingCategory::UnitDwelling
                    && bd.path == UpgradePath::None) {
                    growth = (tier == 1) ? bd.weeklyGrowth : bd.weeklyGrowth / 3;
                    break;
                }
            }
            if (growth <= 0) continue;
            for (const auto& ud : m_registry.units()) {
                if (ud.faction == hired.faction && ud.tier == tier
                    && ud.path == UpgradePath::None) {
                    hired.army.push_back({ud.id, growth});
                    break;
                }
            }
        }

        m_heroes.push_back(hired);
        if (HexTile* ht = m_map.getTile(spawnPos)) ht->heroId = hired.id;
        FogOfWar::updateVision(m_map, hired);

        printf("Hired hero: %s\n", hired.name.c_str());
    }
    if (!canAfford) ImGui::EndDisabled();

    ImGui::TextDisabled("Roster: %d/%d heroes", (int)m_heroes.size(), MAX_HEROES);
    ImGui::End();
}

// ── Capture notification popup ────────────────────────────────────────────────
void Game::renderCapturePopup()
{
    ImGui::OpenPopup("Town Captured!");
    ImVec2 centre = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(centre, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_Always);

    if (ImGui::BeginPopupModal("Town Captured!", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f),
                           "%s is now under your control!", m_capturedTownName.c_str());
        ImGui::Spacing();
        ImGui::TextDisabled("Weekly income from this town will begin next week.");
        ImGui::Spacing();
        if (ImGui::Button("Continue", ImVec2(-1, 32))) {
            m_showCapturePopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// ── Town lost notification (enemy captured player town) ───────────────────────
void Game::renderTownLostPopup()
{
    ImGui::OpenPopup("Town Lost!");
    ImVec2 centre = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(centre, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_Always);

    if (ImGui::BeginPopupModal("Town Lost!", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.2f, 1.0f),
                           "%s has fallen to the enemy!", m_lostTownName.c_str());
        ImGui::Spacing();
        ImGui::TextDisabled("Recapture it to restore your income.");
        ImGui::Spacing();
        if (ImGui::Button("Acknowledge", ImVec2(-1, 32))) {
            m_showTownLostPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// ── Week summary popup ────────────────────────────────────────────────────────
void Game::renderWeekSummary()
{
    ImGui::OpenPopup("Week Begins!");
    ImVec2 centre = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(centre, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_Always);

    if (ImGui::BeginPopupModal("Week Begins!", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.15f, 1.0f), "Week %d", m_weekSummaryWeek);
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Text("Resources received this week:");
        ImGui::Spacing();

        static const struct { ResourceType type; const char* icon; ImVec4 col; } kRes[] = {
            { ResourceType::Gold,         "Gold",        {1.00f, 0.85f, 0.20f, 1.f} },
            { ResourceType::Iron,         "Iron",        {0.70f, 0.70f, 0.75f, 1.f} },
            { ResourceType::FaithStones,  "Faith",       {0.88f, 0.84f, 1.00f, 1.f} },
            { ResourceType::BloodEssence, "Blood",       {0.90f, 0.25f, 0.25f, 1.f} },
            { ResourceType::VerdantSap,   "Sap",         {0.35f, 0.80f, 0.35f, 1.f} },
            { ResourceType::Mercury,      "Mercury",     {0.30f, 0.80f, 0.75f, 1.f} },
        };
        bool anyIncome = false;
        for (const auto& rd : kRes) {
            int amt = m_weekSummaryIncome.get(rd.type);
            if (amt <= 0) continue;
            anyIncome = true;
            ImGui::TextColored(rd.col, "  %-12s  +%d", rd.icon, amt);
        }
        if (!anyIncome)
            ImGui::TextDisabled("  (no towns or mines owned)");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::Button("Continue", ImVec2(-1, 30))) {
            m_showWeekSummary = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// ── State transitions ─────────────────────────────────────────────────────────
void Game::enterTown(Town* town)
{
    if (!town) return;
    m_state = GameState::Town;
    Hero* hero = m_heroes.empty() ? nullptr : &m_heroes[m_activeHeroIdx];
    m_townScreen.open(town, &m_playerResources, &m_registry, hero);
    // Play faction-specific theme; fall back to generic town_music
    int fid = static_cast<int>(town->faction);
    if (fid >= 0 && fid < 9) {
        char key[32]; std::snprintf(key, sizeof(key), "faction_music_%d", fid);
        m_audio.playMusic(key);
    } else {
        m_audio.playMusic("town_music");
    }
    printf("Entered town: %s\n", town->name.c_str());
}

void Game::exitTown()
{
    m_townScreen.close();
    m_audio.playMusic("worldmap_music");
    enterWorldMap();
}
