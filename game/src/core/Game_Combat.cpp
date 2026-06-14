#include "Game.h"
#include "../hero/SkillRegistry.h"
#include "../hero/HeroClass.h"
#include "../magic/SpellRegistry.h"
#include <imgui.h>
#include <stdio.h>
#include <sstream>
#include <algorithm>
#include <cmath>

// ── Helper: resolve (faction, tier) for a CombatUnit ─────────────────────────
static std::pair<int,int> unitFactionTier(const CombatUnit& u,
                                          const std::vector<UnitDef>& defs)
{
    if (u.defId != 0) {
        for (const auto& d : defs)
            if (d.id == u.defId)
                return { static_cast<int>(d.faction), d.tier };
    }
    return { 0, 1 };   // fallback: HolyOrder T1
}

// ── Combat update ─────────────────────────────────────────────────────────────
void Game::updateCombat(float dt)
{
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

    // Advance sprite animators
    {
        const CombatUnit* active = m_combat.activeUnit();
        for (const auto& u : m_combat.grid().units()) {
            auto it = m_combatAnimators.find(u.id);
            if (it == m_combatAnimators.end()) continue;
            SpriteAnimator& anim = it->second;
            if (!u.alive) {
                anim.setState(AnimState::Dead);
            } else if (active && u.id == active->id && u.isPlayer) {
                // Active player unit shows attack pose
                anim.setState(AnimState::Attack);
            } else {
                anim.setState(AnimState::Idle);
            }
            anim.update(dt);
        }
    }

    if (m_combat.phase() == CombatPhase::EnemyTurn)
        m_combat.processAITurn();

    // Advance floating damage effect timers
    for (auto& ef : m_combatDmgEffects) ef.t -= dt;
    m_combatDmgEffects.erase(
        std::remove_if(m_combatDmgEffects.begin(), m_combatDmgEffects.end(),
            [](const CombatDmgEffect& e){ return e.t <= 0.f; }),
        m_combatDmgEffects.end());

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

    // Draw units (sprite or circle fallback)
    float hexR = hg.hexSize() * scale * 0.38f;
    float sprW = hexR * 1.8f;    // half-width of sprite quad
    float sprH = hexR * 2.8f;    // full height of sprite quad

    // Draw dead units first (behind living ones)
    for (const auto& u : grid.units()) {
        if (u.alive) continue;
        float wx, wy;
        hg.hexToWorld(u.pos, wx, wy);
        float sx = wx * scale + m_combatBoardOffX;
        float sy = wy * scale + m_combatBoardOffY;

        auto it = m_combatAnimators.find(u.id);
        if (it != m_combatAnimators.end()) {
            const SpriteAnimator& anim = it->second;
            int fi = anim.faction;
            if (fi >= 0 && fi < NUM_FACTIONS && m_spriteAtlas[fi].ok()) {
                float u0, v0, u1, v1;
                it->second.getUV(u0, v0, u1, v1);
                ImTextureID tid = (ImTextureID)(uintptr_t)m_spriteAtlas[fi].id();
                dl->AddImage(tid,
                    {sx - sprW, sy - sprH * 0.85f},
                    {sx + sprW, sy + sprH * 0.15f},
                    {u0, v0}, {u1, v1}, IM_COL32(255,255,255,90));
            }
        }
    }

    // Draw alive units
    for (const auto& u : grid.units()) {
        if (!u.alive) continue;
        float wx, wy;
        hg.hexToWorld(u.pos, wx, wy);
        float sx = wx * scale + m_combatBoardOffX;
        float sy = wy * scale + m_combatBoardOffY;

        bool  isActive = (active && u.id == active->id);
        ImU32 rimCol   = isActive ? IM_COL32(255, 205, 50, 255)
                                  : IM_COL32(210, 210, 210, 160);

        auto it = m_combatAnimators.find(u.id);
        bool drewSprite = false;
        if (it != m_combatAnimators.end()) {
            const SpriteAnimator& anim = it->second;
            int fi = anim.faction;
            if (fi >= 0 && fi < NUM_FACTIONS && m_spriteAtlas[fi].ok()) {
                float u0, v0, u1, v1;
                anim.getUV(u0, v0, u1, v1);
                ImTextureID tid = (ImTextureID)(uintptr_t)m_spriteAtlas[fi].id();
                dl->AddImage(tid,
                    {sx - sprW, sy - sprH * 0.85f},
                    {sx + sprW, sy + sprH * 0.15f},
                    {u0, v0}, {u1, v1});
                drewSprite = true;
            }
        }
        if (!drewSprite) {
            // Circle fallback if no sprite atlas loaded
            ImU32 fillCol = u.isPlayer ? IM_COL32(55, 155, 55, 230)
                                       : IM_COL32(185, 45, 45, 230);
            dl->AddCircleFilled({sx, sy}, hexR, fillCol);
        }

        // Activity ring
        dl->AddCircle({sx, sy}, hexR * 1.05f, rimCol, 0, isActive ? 2.5f : 1.2f);

        // HP bar — shows total stack HP as a fraction of starting HP for this stack
        int   stackMaxHp = u.count * u.maxHp + (u.maxHp - u.hp);  // approximate starting max
        float hpFrac     = (stackMaxHp > 0)
                            ? static_cast<float>(u.totalHp()) / static_cast<float>(u.count * u.maxHp + (u.maxHp - u.hp))
                            : 0.0f;
        // Simpler: just show top-unit fraction (more useful feedback per-unit)
        hpFrac = (u.maxHp > 0) ? static_cast<float>(u.hp) / static_cast<float>(u.maxHp) : 0.0f;
        float barW = hexR * 1.4f;
        float barY = sy + sprH * 0.15f + 2.0f;
        dl->AddRectFilled({sx - barW, barY}, {sx + barW, barY + 4}, IM_COL32(70, 10, 10, 200));
        ImU32 hpCol = hpFrac > 0.5f ? IM_COL32(50, 200, 50, 220)
                    : hpFrac > 0.25f ? IM_COL32(220, 180, 30, 220)
                                     : IM_COL32(220, 50, 50, 220);
        dl->AddRectFilled({sx - barW, barY}, {sx - barW + 2.0f*barW*hpFrac, barY + 4}, hpCol);

        // Buff/debuff indicators — small colored dots above the unit
        float dotY  = sy - sprH * 0.85f - 6.0f;
        float dotX  = sx - 6.0f;
        if (u.roundAttackBonus > 0) {
            dl->AddCircleFilled({dotX, dotY}, 4.0f, IM_COL32(255, 140, 20, 220));  // orange = atk buff
            dotX += 10.0f;
        } else if (u.roundAttackBonus < 0) {
            dl->AddCircleFilled({dotX, dotY}, 4.0f, IM_COL32(200, 60, 60, 220));   // red = atk debuff
            dotX += 10.0f;
        }
        if (u.roundDefenseBonus > 0) {
            dl->AddCircleFilled({dotX, dotY}, 4.0f, IM_COL32(60, 140, 255, 220));  // blue = def buff
            dotX += 10.0f;
        } else if (u.roundDefenseBonus < 0) {
            dl->AddCircleFilled({dotX, dotY}, 4.0f, IM_COL32(160, 60, 200, 220));  // purple = def debuff
            dotX += 10.0f;
        }
        if (u.poisonRounds > 0) {
            dl->AddCircleFilled({dotX, dotY}, 4.0f, IM_COL32(80, 220, 80, 220));   // green = poisoned
            dotX += 10.0f;
        }
        if (u.burnRounds > 0) {
            dl->AddCircleFilled({dotX, dotY}, 4.0f, IM_COL32(255, 120, 40, 220));  // orange = burning
        }

        // Stack count label (bottom-center)
        char buf[12];
        std::snprintf(buf, sizeof(buf), "%d", u.count);
        ImVec2 ts = ImGui::CalcTextSize(buf);
        float lx = sx - ts.x * 0.5f;
        float ly = sy + sprH * 0.02f;  // just below center
        dl->AddText({lx + 1, ly + 1}, IM_COL32(0, 0, 0, 200), buf);
        dl->AddText({lx, ly}, IM_COL32(255, 255, 255, 255), buf);
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

    // Floating damage numbers
    for (const auto& ef : m_combatDmgEffects) {
        float alpha = std::min(1.0f, ef.t);
        if (alpha <= 0.f) continue;
        float rise = (1.5f - ef.t) * 35.0f;
        char dmgBuf[16];
        std::snprintf(dmgBuf, sizeof(dmgBuf), "%d", ef.dmg);
        float fx = ef.bx - ImGui::CalcTextSize(dmgBuf).x * 0.5f;
        float fy = ef.by - rise;
        int   a  = static_cast<int>(alpha * 255);
        ImU32 shadow = IM_COL32(0, 0, 0, a);
        ImU32 col    = ef.isHeal ? IM_COL32(80, 255, 100, a) : IM_COL32(255, 80, 60, a);
        dl->AddText({fx + 1, fy + 1}, shadow, dmgBuf);
        dl->AddText({fx, fy}, col, dmgBuf);
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

    // Snapshot hero army for FIRST_AID post-combat calculation
    m_battleStartArmy = playerHero.army;

    // Garrison bonus: garrisoned hero grants +2 defense to all their units
    std::vector<CombatUnit> pUnitsGarr = playerUnits;
    if (playerHero.isGarrisoned)
        for (auto& u : pUnitsGarr) u.defense += 2;

    m_combat.startBattle(playerHero, pUnitsGarr, enemyHero, enemyUnits, false);

    // Apply equipped artifact bonuses to the engine's internal hero/unit copies
    ArtifactBonus pb = m_artifactRegistry.totalBonus(playerHero.artifacts);
    ArtifactBonus eb = m_artifactRegistry.totalBonus(enemyHero.artifacts);
    m_combat.applyArtifactBonuses(pb, eb);

    m_combat.setLogCallback([](const std::string& msg) {
        printf("[Combat] %s\n", msg.c_str());
    });
    m_combatDmgEffects.clear();
    m_combat.setDamageCallback([this](uint32_t targetId, int dmg, HexCoord pos) {
        // Convert hex pos to board pixel pos for floating text
        float wx, wy;
        m_combat.grid().hexGrid().hexToWorld(pos, wx, wy);
        float sx = wx * m_combatBoardScale + m_combatBoardOffX;
        float sy = wy * m_combatBoardScale + m_combatBoardOffY;
        m_combatDmgEffects.push_back({sx, sy, 1.5f, dmg, false});
        (void)targetId;
    });
    // Build sprite animators for every unit
    m_combatAnimators.clear();
    const auto& unitDefs = m_registry.units();
    for (const auto& u : m_combat.grid().units()) {
        SpriteAnimator anim;
        auto [fac, tier] = unitFactionTier(u, unitDefs);
        anim.faction = fac;
        anim.tier    = std::max(1, std::min(6, tier));
        anim.mirror  = !u.isPlayer;  // enemy faces left
        m_combatAnimators[u.id] = anim;
    }

    m_audio.playMusic("combat_music");
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

        // Apply FIRST_AID: restore % of casualties from each stack
        if (playerWon) {
            if (const SkillInstance* s = hero.skills.getSkill(SID::FIRST_AID)) {
                if (const SkillDef* def = findSkillDef(SID::FIRST_AID)) {
                    int healPct = def->values[static_cast<int>(s->tier)];
                    for (auto& stack : hero.army) {
                        int startCount = 0;
                        for (const auto& bs : m_battleStartArmy)
                            if (bs.defId == stack.defId) { startCount = bs.count; break; }
                        int lost   = std::max(0, startCount - stack.count);
                        int healed = lost * healPct / 100;
                        stack.count += healed;
                        if (healed > 0)
                            printf("First Aid: restored %d %s\n", healed, "units");
                    }
                }
            }

            // Apply NECROMANCY: raise % of killed enemies as Skeletons (defId 2001)
            if (const SkillInstance* s = hero.skills.getSkill(SID::NECROMANCY)) {
                if (const SkillDef* def = findSkillDef(SID::NECROMANCY)) {
                    int necroRaise = m_combat.enemyStartCount()
                                   * def->values[static_cast<int>(s->tier)] / 100;
                    if (necroRaise > 0) {
                        constexpr int SKELETON_DEF_ID = 2001;
                        bool merged = false;
                        for (auto& stack : hero.army)
                            if (stack.defId == SKELETON_DEF_ID) {
                                stack.count += necroRaise; merged = true; break;
                            }
                        if (!merged && hero.army.size() < 7)
                            hero.army.push_back({SKELETON_DEF_ID, necroRaise});
                        char necroBuf[48];
                        std::snprintf(necroBuf, sizeof(necroBuf),
                            "+%d Skeletons (Necromancy)", necroRaise);
                        pushPickupEffect(hero.pos, necroBuf, IM_COL32(180, 220, 255, 255));
                        printf("Necromancy: raised %d skeletons\n", necroRaise);
                    }
                }
            }
        }
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

        // Bandit camp reward
        if (m_lastBanditCampId != 0) {
            for (auto& obj : m_worldObjects) {
                if (obj.id == m_lastBanditCampId) {
                    int diff = obj.value;
                    int reward = 200 * diff;
                    m_playerResources.add(ResourceType::Gold, reward);
                    obj.collected = true;
                    char campBuf[32];
                    std::snprintf(campBuf, sizeof(campBuf), "+%d Gold!", reward);
                    pushPickupEffect(obj.pos, campBuf, IM_COL32(255, 215, 50, 255));
                    printf("Bandit camp cleared! Reward: %d gold\n", reward);
                    break;
                }
            }
            m_lastBanditCampId = 0;
        }

        // Remove defeated enemy hero from the world
        if (m_lastCombatEnemyId != 0) {
            // Loot the defeated hero — gold scales with enemy army strength
            {
                int lootGold = 100 + m_combat.xpEarned() * 3;
                m_playerResources.add(ResourceType::Gold, lootGold);
                // Find the hero position for the pickup effect
                for (const auto& eh : m_enemyHeroes) {
                    if (eh.id == m_lastCombatEnemyId) {
                        char lootBuf[32];
                        std::snprintf(lootBuf, sizeof(lootBuf), "+%d Gold (loot)", lootGold);
                        pushPickupEffect(eh.pos, lootBuf, IM_COL32(255, 215, 50, 255));
                        break;
                    }
                }
                printf("Enemy hero looted: %d gold\n", lootGold);
            }
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
        if (m_enemyHeroes.empty()) {
            m_showVictory = true;
            m_audio.playSound("victory");
        }

        // Award hero XP
        if (!m_heroes.empty()) {
            Hero& hero = m_heroes[m_activeHeroIdx];
            int xp = m_combat.xpEarned();
            if (xp > 0) {
                char xpBuf[32];
                std::snprintf(xpBuf, sizeof(xpBuf), "+%d XP", xp);
                pushPickupEffect(hero.pos, xpBuf, IM_COL32(160, 255, 160, 255));
            }
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

        // Apply hero specialties on victory
        if (!m_heroes.empty()) {
            Hero& hero = m_heroes[m_activeHeroIdx];
            const HeroClassDef* cls = m_classRegistry.getClass(hero.classId);
            if (cls) {
                // SoulHarvest (Death Herald): restore hero HP = enemies_killed × 3
                if (cls->specialty == SpecialtyType::SoulHarvest) {
                    int heal = m_combat.enemyStartCount() * 3;
                    hero.heroHp = std::min(hero.heroMaxHp, hero.heroHp + heal);
                    if (heal > 0) {
                        char buf[32];
                        std::snprintf(buf, sizeof(buf), "+%d Hero HP (Soul Harvest)", heal);
                        pushPickupEffect(hero.pos, buf, IM_COL32(180, 255, 200, 255));
                    }
                }
                // Veteran (Crusader): +1 attack per battle won, max +5
                if (cls->specialty == SpecialtyType::Veteran) {
                    hero.battlesWon++;
                    if (hero.specialtyAtk < 5) {
                        hero.specialtyAtk++;
                        hero.attack++;
                        char buf[40];
                        std::snprintf(buf, sizeof(buf), "+1 ATK (Veteran, total %d)", hero.specialtyAtk);
                        pushPickupEffect(hero.pos, buf, IM_COL32(255, 200, 80, 255));
                    }
                }
                // Predator (Assassin Lord): permanent +1 attack for each enemy hero killed
                if (cls->specialty == SpecialtyType::Predator && m_lastCombatEnemyId != 0) {
                    if (hero.specialtyAtk < 10) {
                        hero.specialtyAtk++;
                        hero.attack++;
                        char buf[44];
                        std::snprintf(buf, sizeof(buf), "+1 ATK (Predator, total %d)", hero.specialtyAtk);
                        pushPickupEffect(hero.pos, buf, IM_COL32(255, 100, 100, 255));
                    }
                }
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
        m_lastBanditCampId = 0;
        m_triggers.fire(TriggerType::BattleLost, ctx);
        m_showDefeat = true;
        m_audio.playSound("hit");   // thud indicates loss
    }
    m_audio.playMusic("worldmap_music");
    enterWorldMap();
}
