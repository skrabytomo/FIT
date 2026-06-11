#include "Game.h"
#include "../hero/SkillRegistry.h"
#include "../magic/SpellRegistry.h"
#include <imgui.h>
#include <stdio.h>
#include <sstream>
#include <algorithm>
#include <cmath>

// ── Combat update ─────────────────────────────────────────────────────────────
void Game::updateCombat(float dt)
{
    (void)dt;
    const auto& mouse = m_input.mouse();
    float mx = static_cast<float>(mouse.x);
    float my = static_cast<float>(mouse.y);

    if (mouse.leftDown) {
        bool consumed = m_combatHUD.onMouseDown(mx, my);
        if (!consumed && m_combat.phase() == CombatPhase::PlayerTurn) {
            // Convert mouse → world → hex
            float wx = (mx - m_combatBoardOffX) / m_combatBoardScale;
            float wy = (my - m_combatBoardOffY) / m_combatBoardScale;
            HexCoord clicked = m_combat.grid().hexGrid().worldToHex(wx, wy);

            if (m_combat.grid().inBounds(clicked)) {
                CombatUnit* active = m_combat.activeUnit();
                if (active && active->isPlayer) {
                    const CombatUnit* tgt = m_combat.grid().getUnitAt(clicked);
                    if (tgt && !tgt->isPlayer && tgt->alive) {
                        // Attack or shoot
                        CombatAction act;
                        act.type         = (active->shotsLeft > 0 && active->range > 0)
                                           ? ActionType::Shoot
                                           : ActionType::Attack;
                        act.targetUnitId = tgt->id;
                        act.target       = clicked;
                        m_combat.submitAction(act);
                    } else if (!tgt) {
                        // Move to empty reachable tile
                        auto reach = m_combat.grid().reachable(
                            active->pos, active->speed, active->flying);
                        for (const auto& h : reach) {
                            if (h == clicked) {
                                CombatAction act;
                                act.type   = ActionType::Move;
                                act.target = clicked;
                                m_combat.submitAction(act);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    m_combatHUD.onMouseMove(mx, my);

    if (m_combat.phase() == CombatPhase::EnemyTurn)
        m_combat.processAITurn();

    if (m_combat.phase() == CombatPhase::Victory)
        exitCombat(true);
    else if (m_combat.phase() == CombatPhase::Defeat)
        exitCombat(false);
}

// ── Combat render ─────────────────────────────────────────────────────────────
void Game::renderCombat()
{
    m_ui.beginFrame();
    m_combatHUD.draw(m_ui, m_combat);
    m_ui.endFrame();

    beginImGuiFrame();
    m_ui.flushText(ImGui::GetBackgroundDrawList());
    renderCombatBoard();
    if (m_showSpellPanel) renderSpellPanel();
    endImGuiFrame();
}

// ── Combat board ──────────────────────────────────────────────────────────────
void Game::renderCombatBoard()
{
    if (!m_imguiReady) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const CombatGrid& grid = m_combat.grid();
    const HexGrid& hg = grid.hexGrid();
    const auto& coords = grid.allCoords();
    if (coords.empty()) return;

    // Compute world-space bounding box from hex corners
    float wxMin = 1e9f, wyMin = 1e9f, wxMax = -1e9f, wyMax = -1e9f;
    for (const auto& h : coords) {
        float corners[12];
        hg.hexCorners(h, corners);
        for (int i = 0; i < 6; ++i) {
            if (corners[i*2]   < wxMin) wxMin = corners[i*2];
            if (corners[i*2]   > wxMax) wxMax = corners[i*2];
            if (corners[i*2+1] < wyMin) wyMin = corners[i*2+1];
            if (corners[i*2+1] > wyMax) wyMax = corners[i*2+1];
        }
    }

    // Screen area between turn bar (44 px) and HUD (130 px)
    float areaX = 0.0f;
    float areaY = 44.0f;
    float areaW = static_cast<float>(m_width);
    float areaH = static_cast<float>(m_height) - 44.0f - 130.0f;

    float margin = 12.0f;
    float worldW = wxMax - wxMin;
    float worldH = wyMax - wyMin;
    if (worldW < 1.0f || worldH < 1.0f) return;

    float scale = std::min((areaW - 2.0f * margin) / worldW,
                           (areaH - 2.0f * margin) / worldH);

    float boardW = worldW * scale;
    float boardH = worldH * scale;
    m_combatBoardScale = scale;
    m_combatBoardOffX  = areaX + (areaW - boardW) * 0.5f - wxMin * scale;
    m_combatBoardOffY  = areaY + (areaH - boardH) * 0.5f - wyMin * scale;

    // Reachable tiles for active player unit
    std::vector<HexCoord> reach;
    const CombatUnit* active = const_cast<CombatEngine&>(m_combat).activeUnit();
    if (active && active->isPlayer && !active->hasMoved)
        reach = grid.reachable(active->pos, active->speed, active->flying);

    // Draw background
    dl->AddRectFilled(
        {areaX, areaY},
        {areaX + areaW, areaY + areaH},
        IM_COL32(18, 18, 28, 255));

    // Draw tiles
    for (const auto& h : coords) {
        float corners[12];
        hg.hexCorners(h, corners);

        ImVec2 pts[6];
        for (int i = 0; i < 6; ++i) {
            pts[i].x = corners[i*2]   * scale + m_combatBoardOffX;
            pts[i].y = corners[i*2+1] * scale + m_combatBoardOffY;
        }

        const CombatTile* tile = grid.getTile(h);
        ImU32 fill = IM_COL32(32, 32, 48, 255);
        if (tile) {
            switch (tile->type) {
                case CombatTileType::Attack:       fill = IM_COL32(70, 20, 20, 255); break;
                case CombatTileType::Defense:      fill = IM_COL32(20, 20, 70, 255); break;
                case CombatTileType::Speed:        fill = IM_COL32(20, 60, 20, 255); break;
                case CombatTileType::SpeedPenalty: fill = IM_COL32(55, 40, 10, 255); break;
                case CombatTileType::Obstacle:     fill = IM_COL32(55, 55, 55, 255); break;
                case CombatTileType::Wall:         fill = IM_COL32(90, 90, 90, 255); break;
                default: break;
            }
        }
        // Reachable highlight overrides terrain
        for (const auto& rh : reach)
            if (rh == h) { fill = IM_COL32(35, 80, 35, 220); break; }

        // Active unit tile highlight
        if (active && h == active->pos)
            fill = IM_COL32(80, 70, 20, 255);

        dl->AddConvexPolyFilled(pts, 6, fill);
        dl->AddPolyline(pts, 6, IM_COL32(55, 55, 75, 200),
                        ImDrawFlags_Closed, 1.0f);
    }

    // Draw units
    float hexR = hg.hexSize() * scale * 0.38f;
    for (const auto& u : grid.units()) {
        if (!u.alive) continue;
        float wx, wy;
        hg.hexToWorld(u.pos, wx, wy);
        float sx = wx * scale + m_combatBoardOffX;
        float sy = wy * scale + m_combatBoardOffY;

        ImU32 fillCol  = u.isPlayer ? IM_COL32(55, 155, 55, 230)
                                    : IM_COL32(185, 45, 45, 230);
        bool  isActive = (active && u.id == active->id);
        ImU32 rimCol   = isActive ? IM_COL32(255, 205, 50, 255)
                                  : IM_COL32(210, 210, 210, 200);

        dl->AddCircleFilled({sx, sy}, hexR, fillCol);
        dl->AddCircle({sx, sy}, hexR, rimCol, 0, isActive ? 2.5f : 1.5f);

        // Stack count
        char buf[12];
        std::snprintf(buf, sizeof(buf), "%d", u.count);
        ImVec2 ts = ImGui::CalcTextSize(buf);
        dl->AddText({sx - ts.x * 0.5f, sy - ts.y * 0.5f},
                    IM_COL32(255, 255, 255, 255), buf);
    }

    // Coordinate hint for hovered hex (debug feel)
    const auto& mouse = m_input.mouse();
    float mwx = (mouse.x - m_combatBoardOffX) / m_combatBoardScale;
    float mwy = (mouse.y - m_combatBoardOffY) / m_combatBoardScale;
    HexCoord mh = hg.worldToHex(mwx, mwy);
    if (grid.inBounds(mh)) {
        float corners[12];
        hg.hexCorners(mh, corners);
        ImVec2 pts[6];
        for (int i = 0; i < 6; ++i) {
            pts[i].x = corners[i*2]   * scale + m_combatBoardOffX;
            pts[i].y = corners[i*2+1] * scale + m_combatBoardOffY;
        }
        dl->AddPolyline(pts, 6, IM_COL32(220, 220, 120, 180),
                        ImDrawFlags_Closed, 1.5f);
    }
}

// ── Spell panel (ImGui) ───────────────────────────────────────────────────────
void Game::renderSpellPanel()
{
    if (m_heroes.empty()) return;
    const Hero& hero = m_heroes[m_activeHeroIdx];
    if (hero.knownSpells.empty()) {
        ImGui::Begin("Spells", &m_showSpellPanel, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::TextDisabled("No spells known.");
        ImGui::TextDisabled("Find spellbooks on the world map or build a mage tower.");
        ImGui::End();
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Spells", &m_showSpellPanel)) { ImGui::End(); return; }

    ImGui::Text("Mana: %d / %d", hero.mana, hero.maxMana);
    ImGui::Separator();

    // Target selector — pick from living enemy units
    if (ImGui::BeginCombo("Target", m_spellTargetId == 0 ? "— pick —" : [&]() -> const char* {
        auto* u = m_combat.grid().getUnit(m_spellTargetId);
        return u ? u->name.c_str() : "—";
    }()))
    {
        for (auto& u : m_combat.grid().units()) {
            if (!u.alive) continue;
            bool sel = (u.id == m_spellTargetId);
            std::string lbl = u.name + (u.isPlayer ? " [ally]" : " [enemy]");
            if (ImGui::Selectable(lbl.c_str(), sel))
                m_spellTargetId = u.id;
        }
        ImGui::EndCombo();
    }
    ImGui::Separator();

    CombatUnit* active = m_combat.activeUnit();
    bool isPlayerTurn  = active && active->isPlayer;

    for (int sid : hero.knownSpells) {
        const SpellDef* spell = findSpell(sid);
        if (!spell) continue;

        bool canAfford = hero.mana >= spell->manaCost;
        if (!isPlayerTurn || !canAfford) ImGui::BeginDisabled();

        char btnLabel[128];
        std::snprintf(btnLabel, sizeof(btnLabel), "%s  (%d mana)", spell->name, spell->manaCost);
        if (ImGui::Button(btnLabel, ImVec2(-1, 0))) {
            CombatAction act;
            act.type         = ActionType::UseAbility;
            act.spellId      = sid;
            act.targetUnitId = m_spellTargetId;
            m_combat.submitAction(act);
            m_showSpellPanel = false;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", spell->desc);

        if (!isPlayerTurn || !canAfford) ImGui::EndDisabled();
    }
    ImGui::End();
}

// ── State transitions ─────────────────────────────────────────────────────────
void Game::enterCombat(Hero& playerHero,
                       const std::vector<CombatUnit>& playerUnits,
                       const Hero& enemyHero,
                       const std::vector<CombatUnit>& enemyUnits)
{
    m_state = GameState::Combat;
    m_combat.startBattle(playerHero, playerUnits, enemyHero, enemyUnits, false);

    // Apply equipped artifact bonuses to the engine's internal hero/unit copies
    ArtifactBonus pb = m_artifactRegistry.totalBonus(playerHero.artifacts);
    ArtifactBonus eb = m_artifactRegistry.totalBonus(enemyHero.artifacts);
    m_combat.applyArtifactBonuses(pb, eb);

    m_combat.setLogCallback([](const std::string& msg) {
        printf("[Combat] %s\n", msg.c_str());
    });
    printf("Entered combat\n");
}

void Game::exitCombat(bool playerWon)
{
    printf("Combat ended — %s\n", playerWon ? "Victory" : "Defeat/Retreat");
    ScriptContext ctx; ctx.heroId = m_heroes.empty() ? 0 : (int)m_heroes[m_activeHeroIdx].id;

    // Sync surviving units back to their hero armies
    if (!m_heroes.empty()) {
        Hero& hero = m_heroes[m_activeHeroIdx];
        hero.army.clear();
        for (const auto& cu : m_combat.grid().units()) {
            if (!cu.alive || cu.isPlayer == false || cu.count <= 0) continue;
            bool merged = false;
            for (auto& s : hero.army)
                if (s.defId == cu.defId) { s.count += cu.count; merged = true; break; }
            if (!merged) hero.army.push_back({cu.defId, cu.count});
        }
        printf("Hero survivors: %zu stacks\n", hero.army.size());
    }

    if (playerWon) {
        // Capture town if this was a garrison fight
        Town* captured = nullptr;
        if (m_pendingTownCaptureId != 0) {
            for (auto& t : m_towns)
                if (t.id == m_pendingTownCaptureId) { captured = &t; break; }
            if (captured) {
                captured->ownerId = 1;
                captured->garrison.clear();
                m_capturedTownName = captured->name;
                m_showCapturePopup = true;
                printf("Captured town after garrison fight: %s\n", m_capturedTownName.c_str());
            }
            m_pendingTownCaptureId = 0;
        }

        // Remove defeated enemy hero from the world
        if (m_lastCombatEnemyId != 0) {
            // Release all mines owned by the defeated hero
            for (auto& r : m_resources)
                if (r.ownedBy == m_lastCombatEnemyId) r.ownedBy = 0;
            m_enemyHeroes.erase(
                std::remove_if(m_enemyHeroes.begin(), m_enemyHeroes.end(),
                    [&](const Hero& e){ return e.id == m_lastCombatEnemyId; }),
                m_enemyHeroes.end());
            m_map.forEach([&](HexTile& t){
                if (t.heroId == m_lastCombatEnemyId) t.heroId = 0;
            });
            m_lastCombatEnemyId = 0;
        }

        m_hideout.addXP(50);
        m_triggers.fire(TriggerType::BattleWon, ctx);
        if (m_enemyHeroes.empty()) m_showVictory = true;

        // Award hero XP
        if (!m_heroes.empty()) {
            Hero& hero = m_heroes[m_activeHeroIdx];
            int xp = m_combat.xpEarned();
            printf("Hero earns %d XP\n", xp);
            if (hero.addXp(xp)) {
                printf("Hero leveled up to %d!\n", hero.level);
                const HeroClassDef* cls = m_classRegistry.getClass(hero.classId);
                if (cls) {
                    std::vector<SkillDef> allSkills(SKILL_DEFS, SKILL_DEFS + SKILL_DEF_COUNT);
                    m_levelUpOffers = LevelUpSystem::generateOffers(
                        *cls, hero.skills, hero.level, allSkills, hero.faction);
                }
                if (m_levelUpOffers.empty())
                    m_levelUpOffers.push_back({SID::OFFENSE, false, false, "Learn Offense"});
                m_showLevelUpModal = true;
            }
        }

        // After a garrison victory, drop the player into the captured town
        if (captured) {
            enterTown(captured);
            return;
        }
    } else {
        // Sync surviving enemy army too (they won, they keep what's left)
        for (auto& eh : m_enemyHeroes) {
            if (eh.id != m_lastCombatEnemyId) continue;
            eh.army.clear();
            for (const auto& cu : m_combat.grid().units()) {
                if (!cu.alive || cu.isPlayer || cu.count <= 0) continue;
                bool merged = false;
                for (auto& s : eh.army)
                    if (s.defId == cu.defId) { s.count += cu.count; merged = true; break; }
                if (!merged) eh.army.push_back({cu.defId, cu.count});
            }
            break;
        }
        m_pendingTownCaptureId = 0;
        m_triggers.fire(TriggerType::BattleLost, ctx);
        m_showDefeat = true;
    }
    enterWorldMap();
}
