#include "Game.h"
#include <cstdio>
#include <algorithm>
#include "../magic/SpellRegistry.h"
#include "../world/HexGrid.h"
#include "../world/FogOfWar.h"
#include "../hero/Artifacts.h"
#include "../hero/HeroClass.h"
#include "../hero/SkillRegistry.h"
#include <imgui.h>
#include <stdio.h>
#include <unordered_map>
#include <unordered_set>
#include <cstdio>

// ── Town update ───────────────────────────────────────────────────────────────
void Game::updateTown(float dt)
{
    (void)dt;
    const auto& mouse = m_input.mouse();
    bool imguiWants = ImGui::GetIO().WantCaptureMouse;

    if (mouse.leftDown && !imguiWants)
        m_townScreen.onMouseDown(static_cast<float>(mouse.x),
                                 static_cast<float>(mouse.y));
    if (mouse.leftUp && !imguiWants)
        m_townScreen.onMouseUp(static_cast<float>(mouse.x),
                               static_cast<float>(mouse.y));
    m_townScreen.onMouseMove(static_cast<float>(mouse.x),
                             static_cast<float>(mouse.y));
}

// ── Town render ───────────────────────────────────────────────────────────────
void Game::renderTown()
{
    beginImGuiFrame();          // must come BEFORE any ImGui calls in draw()
    m_ui.beginFrame();
    m_townScreen.draw(m_ui);
    m_ui.endFrame();
    m_ui.flushText(ImGui::GetBackgroundDrawList());

    // Service buttons bar — top strip above the town panel
    {
        const Town* town = m_townScreen.currentTown();
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2((float)m_width, 0), ImGuiCond_Always);
        ImGui::Begin("##TownServices", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoBackground);

        if (town && town->hasBuilding(BID::MAGE_GUILD)) {
            if (ImGui::Button(m_showMageGuildPanel ? "[Mage Guild X]" : "Mage Guild"))
                m_showMageGuildPanel = !m_showMageGuildPanel;
            ImGui::SameLine();
        }
        if (town && town->ownerId == 1) {
            if (ImGui::Button(m_showTavernPanel ? "[Tavern X]" : "Tavern"))
                m_showTavernPanel = !m_showTavernPanel;
            ImGui::SameLine();
        }
        if (town && town->hasBuilding(BID::MARKET)) {
            if (ImGui::Button(m_showMarketPanel ? "[Market X]" : "Market"))
                m_showMarketPanel = !m_showMarketPanel;
            ImGui::SameLine();
            if (ImGui::Button(m_showArtifactForgePanel ? "[Forge X]" : "Artifact Forge"))
                m_showArtifactForgePanel = !m_showArtifactForgePanel;
            ImGui::SameLine();
        }
        // Spacer + exit button pushed to the right
        ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - 130.0f);
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.5f, 0.05f, 0.05f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.1f, 0.1f, 1.0f));
        if (ImGui::Button("EXIT TOWN  [ESC]", ImVec2(125.0f, 0)))
            exitTown();
        ImGui::PopStyleColor(2);
        ImGui::End();
    }

    if (m_showMageGuildPanel)    renderMageGuild();
    if (m_showTavernPanel)       renderTavern();
    if (m_showMarketPanel)       renderMarketplace();
    if (m_showArtifactForgePanel) renderArtifactForge();
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
        case FactionId::Thornkin:      return {{SPL::BARKSKIN,4000},{SPL::ENTANGLE,4500},{SPL::SERPENT_VENOM,5500},{SPL::CALL_LIGHTNING,6000}};
        case FactionId::EternalEmpire: return {{SPL::CURSE,4000},{SPL::WITHER,5000},{SPL::DEATH_COIL,6000},{SPL::VENOMOUS_CLOUD,8000}};
        case FactionId::CrimsonWardens:return {{SPL::WITHER,4000},{SPL::VENOMOUS_CLOUD,6000},{SPL::DEATH_COIL,5000},{SPL::PLAGUE,8000}};
        case FactionId::Voidkin:       return {{SPL::CURSE,4000},{SPL::ENTANGLE,4500},{SPL::WITHER,5000},{SPL::VENOMOUS_CLOUD,7000}};
        case FactionId::IronAssembly:  return {{SPL::REINFORCE,4000},{SPL::OVERCLOCK,4500},{SPL::SHRAPNEL,6000},{SPL::NAPALM,7500}};
        case FactionId::Amalgamate:    return {{SPL::MEND_FLESH,4000},{SPL::FESTER,5000},{SPL::ACID_SPRAY,5500},{SPL::GROWTH,7000}};
        case FactionId::Convergence:   return {{SPL::BLESS,4000},{SPL::CURSE,4000},{SPL::REINFORCE,4500},{SPL::REGROWTH,5500}};
        default:                       return {};
        }
    };
    auto entries = getEntries(town->faction);
    if (entries.empty()) return;

    const auto& entries_ref = entries;

    // Tier determines spell access and cost discount
    int  available = 2;
    float costMult  = 1.0f;
    int  tierLevel  = 1;
    if      (town->hasBuilding(BID::MAGE_GUILD_T4)) { available=4; costMult=0.5f;  tierLevel=4; }
    else if (town->hasBuilding(BID::MAGE_GUILD_T3)) { available=4; costMult=0.70f; tierLevel=3; }
    else if (town->hasBuilding(BID::MAGE_GUILD_T2)) { available=4; costMult=1.0f;  tierLevel=2; }

    // T4 bonus: grant hero +5 max mana on visit (one-time per visit)
    if (tierLevel == 4) {
        static std::unordered_set<int> s_t4BonusGiven;
        if (s_t4BonusGiven.find(hero.id) == s_t4BonusGiven.end()) {
            hero.maxMana += 5;
            hero.mana     = std::min(hero.mana + 5, hero.maxMana);
            s_t4BonusGiven.insert(hero.id);
        }
    }

    char titleBuf[48];
    std::snprintf(titleBuf, sizeof(titleBuf), "Mage Guild (Tier %d)###MageGuild", tierLevel);
    ImGui::SetNextWindowPos(ImVec2(20, 32), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_Always);
    if (!ImGui::Begin(titleBuf, nullptr,
                      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End(); return;
    }

    ImGui::Text("Gold: %d", m_playerResources.get(ResourceType::Gold));
    if (costMult < 1.0f)
        ImGui::TextColored(ImVec4(0.4f,1.f,0.6f,1.f), "Tier %d discount: %.0f%% off",
                           tierLevel, (1.0f - costMult) * 100.0f);
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
            int cost = static_cast<int>(entries_ref[i].goldCost * costMult);
            bool canAfford = m_playerResources.get(ResourceType::Gold) >= cost;
            if (!canAfford) ImGui::BeginDisabled();
            char btn[80];
            if (costMult < 1.0f)
                std::snprintf(btn, sizeof(btn), "Learn %s  (%dg, was %dg)",
                              sp->name, cost, entries_ref[i].goldCost);
            else
                std::snprintf(btn, sizeof(btn), "Learn %s  (%dg)", sp->name, cost);
            if (ImGui::Button(btn, ImVec2(-1,0))) {
                hero.knownSpells.push_back(sp->id);
                m_playerResources.add(ResourceType::Gold, -cost);
            }
            if (!canAfford) ImGui::EndDisabled();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", sp->desc);
        ImGui::PopID();
    }
    ImGui::End();
}

// ── Artifact Forge — craft Basic artifacts ────────────────────────────────────
void Game::renderArtifactForge()
{
    // Require MARKET to be built — the marketplace enables trading of goods/materials
    const Town* town = m_townScreen.currentTown();
    if (!town || !town->hasBuilding(BID::MARKET)) return;
    if (m_heroes.empty()) return;
    Hero& hero = m_heroes[m_activeHeroIdx];

    ImGui::SetNextWindowPos(ImVec2(680, 32), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(320, 0), ImGuiCond_Always);
    if (!ImGui::Begin("Artifact Forge", nullptr,
                      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End(); return;
    }

    ImGui::TextDisabled("Craft Basic artifacts (requires MARKET)");
    ImGui::Separator();

    // Show current resources
    ImGui::Text("Gold: %d", m_playerResources.get(ResourceType::Gold));
    ImGui::SameLine(160.0f);
    ImGui::Text("Iron: %d", m_playerResources.get(ResourceType::Iron));
    ImGui::Separator();

    auto craftables = m_artifactRegistry.getCraftable();
    if (craftables.empty()) {
        ImGui::TextDisabled("No craftable artifacts registered.");
        ImGui::End(); return;
    }

    static const char* kSlotNames[] = {
        "Helm","Armor","Weapon","Shield","Ring","Boots","Cloak","Misc"
    };

    for (const ArtifactDef* art : craftables) {
        // Check if hero already has it equipped
        int slotIdx = static_cast<int>(art->slot);
        bool alreadyEquipped = (hero.artifacts.equippedIds[slotIdx] == art->id);
        bool inInventory = false;
        for (int inv : hero.artifactInventory) if (inv == art->id) { inInventory = true; break; }

        ImGui::PushID(art->id);

        // Cost check
        bool canAfford = true;
        for (int rt = 0; rt < RESOURCE_COUNT && canAfford; ++rt) {
            int needed = art->craftCost.get(static_cast<ResourceType>(rt));
            if (needed > 0 && m_playerResources.get(static_cast<ResourceType>(rt)) < needed)
                canAfford = false;
        }

        if (alreadyEquipped) {
            ImGui::TextColored(ImVec4(0.4f,1.f,0.4f,1.f), "[equipped] %s", art->name.c_str());
        } else if (inInventory) {
            // Can equip from inventory
            char btnLabel[80];
            std::snprintf(btnLabel, sizeof(btnLabel), "Equip %s  [in bag]", art->name.c_str());
            if (ImGui::Button(btnLabel, ImVec2(-1, 0))) {
                hero.artifacts.equip(art->id, art->slot);
                hero.artifactInventory.erase(
                    std::remove(hero.artifactInventory.begin(),
                                hero.artifactInventory.end(), art->id),
                    hero.artifactInventory.end());
            }
        } else {
            if (!canAfford) ImGui::BeginDisabled();
            // Build cost label
            char costStr[128] = {};
            int off = 0;
            for (int rt = 0; rt < RESOURCE_COUNT; ++rt) {
                int needed = art->craftCost.get(static_cast<ResourceType>(rt));
                if (needed <= 0) continue;
                static const char* kRNames[] = {"g","Fe","Fa","Bl","Sap","Hg"};
                off += std::snprintf(costStr + off, sizeof(costStr) - off,
                                     "%d%s ", needed, (rt < 6 ? kRNames[rt] : "?"));
            }
            char btnLabel[96];
            std::snprintf(btnLabel, sizeof(btnLabel), "Craft %s  [%s]  (%s)",
                          art->name.c_str(), kSlotNames[slotIdx], costStr);
            if (ImGui::Button(btnLabel, ImVec2(-1, 0))) {
                // Deduct cost
                for (int rt = 0; rt < RESOURCE_COUNT; ++rt) {
                    int needed = art->craftCost.get(static_cast<ResourceType>(rt));
                    if (needed > 0) m_playerResources.add(static_cast<ResourceType>(rt), -needed);
                }
                // Auto-equip if slot is free, else add to inventory
                if (hero.artifacts.getEquipped(art->slot) == 0) {
                    hero.artifacts.equip(art->id, art->slot);
                } else {
                    hero.artifactInventory.push_back(art->id);
                }
            }
            if (!canAfford) ImGui::EndDisabled();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", art->description.c_str());

        ImGui::PopID();
    }
    ImGui::End();
}

// ── Tavern — hire a hero from candidates ────────────────────────────────────
void Game::renderTavern()
{
    const Town* town = m_townScreen.currentTown();
    if (!town || town->ownerId != 1) return;  // only in owned towns

    static constexpr int HIRE_COST  = 2500;
    static constexpr int MAX_HEROES = 3;
    static constexpr int NUM_CANDIDATES = 3;

    ImGui::SetNextWindowPos(ImVec2(350, 32), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(320, 0), ImGuiCond_Always);
    if (!ImGui::Begin("Tavern", nullptr,
                      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End(); return;
    }

    ImGui::Text("Gold: %d", m_playerResources.get(ResourceType::Gold));
    ImGui::Separator();

    if (static_cast<int>(m_heroes.size()) >= MAX_HEROES) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                           "Hero roster full (%d/%d).", MAX_HEROES, MAX_HEROES);
        ImGui::End(); return;
    }

    // Name pool
    static const char* kNames[] = {
        "Alara", "Dren", "Korvas", "Mira", "Seld", "Thayne",
        "Vex", "Lyra", "Brant", "Cael", "Essen", "Fynn",
        "Orin", "Yasha", "Wick", "Seren", "Gael", "Petra"
    };
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

    auto buildHero = [&](int candidateSlot) -> Hero {
        // Deterministic seed per town + week + candidate slot
        uint32_t seed = static_cast<uint32_t>(town->pos.q * 997u + town->pos.r * 491u
                                              + m_turns.week() * 6271u + candidateSlot * 1013u);
        auto classes = m_classRegistry.getClassesForFaction(town->faction);

        Hero h;
        h.id      = 300u + static_cast<uint32_t>(candidateSlot);
        h.faction = town->faction;
        h.pos     = town->pos;
        h.movePool = h.maxMove;
        h.name    = kNames[seed % 18];

        if (!classes.empty()) {
            const HeroClassDef* cls = classes[(seed / 18) % classes.size()];
            h.classId = cls->id;
            if (!cls->skillPool.empty())
                h.skills.learn(cls->skillPool[0]);
        }
        int fi = static_cast<int>(town->faction);
        if (fi >= 0 && fi < 9) h.knownSpells.push_back(kStartSpell[fi]);

        for (int tier : {1, 2}) {
            int growth = 0;
            for (const auto& bd : m_registry.buildings()) {
                if (bd.faction == h.faction && bd.tier == tier
                    && bd.category == BuildingCategory::UnitDwelling
                    && bd.path == UpgradePath::None) {
                    growth = (tier == 1) ? bd.weeklyGrowth : bd.weeklyGrowth / 3;
                    break;
                }
            }
            if (growth <= 0) continue;
            for (const auto& ud : m_registry.units()) {
                if (ud.faction == h.faction && ud.tier == tier
                    && ud.path == UpgradePath::None) {
                    h.army.push_back({ud.id, growth});
                    break;
                }
            }
        }
        return h;
    };

    ImGui::TextDisabled("Choose a hero to hire  (%dg each):", HIRE_COST);
    bool canAfford = m_playerResources.get(ResourceType::Gold) >= HIRE_COST;

    for (int c = 0; c < NUM_CANDIDATES; ++c) {
        Hero cand = buildHero(c);
        const HeroClassDef* cls = m_classRegistry.getClass(cand.classId);

        ImGui::PushID(c);
        ImGui::Separator();

        // Candidate header
        char hdr[64];
        std::snprintf(hdr, sizeof(hdr), "%s  [%s]", cand.name.c_str(),
                      cls ? cls->name : "Unknown");
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "%s", hdr);

        // Specialty description
        if (cls) ImGui::TextDisabled("  %s", cls->specialtyDesc.c_str());

        // Army preview
        char armyBuf[64] = {};
        int off2 = 0;
        for (const auto& s : cand.army) {
            for (const auto& ud : m_registry.units()) {
                if (ud.id == s.defId) {
                    off2 += std::snprintf(armyBuf + off2, sizeof(armyBuf) - off2,
                                          "%dx%s ", s.count, ud.name.c_str());
                    break;
                }
            }
        }
        if (off2 > 0) ImGui::TextDisabled("  Army: %s", armyBuf);

        // Hire button
        if (!canAfford) ImGui::BeginDisabled();
        char btnLabel[48];
        std::snprintf(btnLabel, sizeof(btnLabel), "Hire %s", cand.name.c_str());
        if (ImGui::Button(btnLabel, ImVec2(-1, 0))) {
            m_playerResources.add(ResourceType::Gold, -HIRE_COST);

            // Find spawn tile
            HexCoord spawnPos = town->pos;
            for (auto& nb : HexGrid::neighbors(town->pos)) {
                const HexTile* t2 = m_map.getTile(nb);
                if (t2 && t2->terrain != Terrain::Water && t2->heroId == 0) {
                    spawnPos = nb; break;
                }
            }
            cand.id     = 200u + static_cast<uint32_t>(m_heroes.size());
            cand.pos    = spawnPos;
            m_heroes.push_back(cand);
            if (HexTile* ht = m_map.getTile(spawnPos)) ht->heroId = cand.id;
            FogOfWar::updateVision(m_map, cand);
            printf("Hired hero: %s (%s)\n", cand.name.c_str(), cls ? cls->name : "?");
        }
        if (!canAfford) ImGui::EndDisabled();
        ImGui::PopID();
    }

    ImGui::Separator();
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

// ── Pause menu (Escape on world map) ─────────────────────────────────────────
void Game::renderPauseMenu()
{
    ImGui::OpenPopup("Paused");
    ImVec2 centre = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(centre, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(220, 0), ImGuiCond_Always);

    if (ImGui::BeginPopupModal("Paused", nullptr, ImGuiWindowFlags_AlwaysAutoResize |
                               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::TextColored(ImVec4(0.9f, 0.85f, 0.6f, 1.0f), "Game Paused");
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Resume  [Esc]", ImVec2(-1, 32))) {
            m_showPauseMenu = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::Spacing();

        if (ImGui::Button("Save Game  [F5]", ImVec2(-1, 32))) {
            saveGame("saves/save" + std::to_string(m_activeSlot) + ".json");
            m_showPauseMenu = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::Spacing();

        if (ImGui::Button("Main Menu", ImVec2(-1, 32))) {
            m_showPauseMenu = false;
            m_state = GameState::MainMenu;
            ImGui::CloseCurrentPopup();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Debug");
        ImGui::Checkbox("Disable Fog of War", &m_fogDisabled);
        ImGui::Spacing();
        ImGui::Separator();
        if (ImGui::Button("Quit to Desktop", ImVec2(-1, 28))) {
            m_running = false;
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

        // Weekly event section
        if (!m_weeklyEventHeadline.empty()) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "World Event:");
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.35f, 1.0f), "%s", m_weeklyEventHeadline.c_str());
            ImGui::Spacing();
            ImGui::TextWrapped("%s", m_weeklyEventBody.c_str());
        }

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
    // Entering a player-owned town restores hero HP fully
    if (hero && town->ownerId == 1 && hero->heroHp < hero->heroMaxHp) {
        hero->heroHp = hero->heroMaxHp;
        pushPickupEffect(town->pos, "Hero healed!", IM_COL32(180, 255, 180, 255));
    }
    // Compute BLUEPRINT discount for Iron Assembly heroes visiting their town
    int blueprintDiscount = 0;
    if (hero && town->faction == FactionId::IronAssembly) {
        if (const SkillInstance* s = hero->skills.getSkill(SkillID::BLUEPRINT))
            if (const SkillDef* def = findSkillDef(SkillID::BLUEPRINT))
                blueprintDiscount = def->values[static_cast<int>(s->tier)]; // 1/2/3
    }
    m_showMageGuildPanel     = false;
    m_showTavernPanel        = false;
    m_showArtifactForgePanel = false;
    m_townScreen.open(town, &m_playerResources, &m_registry, hero,
                      m_turns.week(), blueprintDiscount);
    // Wire faction art + unit textures for the recruit panel
    {
        int fid = std::clamp(static_cast<int>(town->faction), 0, NUM_FACTIONS - 1);
        m_townScreen.setTownBannerTex(m_townTex[fid].ok()
            ? (ImTextureID)(uintptr_t)m_townTex[fid].id() : nullptr);
        for (int t = 0; t < NUM_UNIT_TIERS; ++t)
            m_townScreen.setUnitTex(t, m_unitTex[fid][t].ok()
                ? (ImTextureID)(uintptr_t)m_unitTex[fid][t].id() : nullptr);
    }
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
    m_showMageGuildPanel     = false;
    m_showTavernPanel        = false;
    m_showArtifactForgePanel = false;
    m_showMarketPanel        = false;
    m_audio.playMusic("worldmap_music");
    enterWorldMap();
}

// ── Marketplace — resource exchange ───────────────────────────────────────────
void Game::renderMarketplace()
{
    const Town* town = m_townScreen.currentTown();
    if (!town || !town->hasBuilding(BID::MARKET)) return;

    ImGui::SetNextWindowPos(ImVec2(350, 32), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(380, 0), ImGuiCond_Always);
    if (!ImGui::Begin("Market - Resource Exchange", nullptr,
                      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End(); return;
    }

    // Trade rate: 4:1 standard (HoMM3 style)
    static const int SELL_RATE = 4;
    static const int BUY_RATE  = 1;

    static const char* kResNames[] = {
        "Gold", "Iron", "Faith Stones", "Blood Essence", "Verdant Sap", "Mercury"
    };

    ImGui::TextDisabled("Exchange rate: %d:1  (sell %d, receive 1)", SELL_RATE, SELL_RATE);
    ImGui::Separator();

    // Current resources row
    ImGui::Text("Your resources:");
    for (int i = 0; i < RESOURCE_COUNT; ++i) {
        int val = m_playerResources.get(static_cast<ResourceType>(i));
        ImGui::SameLine();
        ImGui::Text("%s:%d", kResNames[i], val);
    }
    ImGui::Separator();

    // Sell selector
    ImGui::Text("Sell:");
    ImGui::SameLine(60.0f);
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::BeginCombo("##sell", kResNames[m_marketSellType])) {
        for (int i = 0; i < RESOURCE_COUNT; ++i) {
            bool sel = (i == m_marketSellType);
            if (ImGui::Selectable(kResNames[i], sel)) {
                m_marketSellType = i;
                if (m_marketBuyType == i)
                    m_marketBuyType = (i + 1) % RESOURCE_COUNT;
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    // Buy selector
    ImGui::Text("Buy: ");
    ImGui::SameLine(60.0f);
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::BeginCombo("##buy", kResNames[m_marketBuyType])) {
        for (int i = 0; i < RESOURCE_COUNT; ++i) {
            if (i == m_marketSellType) continue;
            bool sel = (i == m_marketBuyType);
            if (ImGui::Selectable(kResNames[i], sel))
                m_marketBuyType = i;
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();

    // Quantity buttons
    int have = m_playerResources.get(static_cast<ResourceType>(m_marketSellType));
    int maxTrades = have / SELL_RATE;

    ImGui::Text("You have: %d %s  (max %d trades)", have, kResNames[m_marketSellType], maxTrades);

    if (maxTrades <= 0) {
        ImGui::TextDisabled("Not enough %s to trade.", kResNames[m_marketSellType]);
        ImGui::End();
        return;
    }

    // Quick-trade buttons: 1x, 5x, 10x, MAX
    auto doTrade = [&](int count) {
        if (count <= 0 || count > maxTrades) return;
        m_playerResources.add(static_cast<ResourceType>(m_marketSellType), -(count * SELL_RATE));
        m_playerResources.add(static_cast<ResourceType>(m_marketBuyType),   count * BUY_RATE);
    };

    ImGui::Text("Trade:");
    ImGui::SameLine();

    if (maxTrades >= 1) {
        if (ImGui::Button("x1"))  doTrade(1);
        ImGui::SameLine();
    }
    if (maxTrades >= 5) {
        if (ImGui::Button("x5"))  doTrade(5);
        ImGui::SameLine();
    }
    if (maxTrades >= 10) {
        if (ImGui::Button("x10")) doTrade(10);
        ImGui::SameLine();
    }
    char maxBtn[32];
    std::snprintf(maxBtn, sizeof(maxBtn), "Max (x%d)", maxTrades);
    if (ImGui::Button(maxBtn)) doTrade(maxTrades);

    // Preview result
    ImGui::Separator();
    ImGui::Text("Receive: %d %s per trade", BUY_RATE, kResNames[m_marketBuyType]);

    ImGui::End();
}
