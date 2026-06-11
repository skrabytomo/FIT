#include "Game.h"
#include "../hero/LevelUpSystem.h"
#include "../hero/SkillRegistry.h"
#include "../magic/SpellRegistry.h"
#include <imgui.h>
#include <cmath>
#include <algorithm>
#include <stdio.h>

static constexpr float MOVE_SPEED = 4.0f;

// ── Per-faction combat unit templates ─────────────────────────────────────────
static std::vector<CombatUnit> makeFactionUnits(FactionId faction, bool isPlayer)
{
    std::vector<CombatUnit> out;
    int nextId = isPlayer ? 1 : 50;
    auto add = [&](const char* name, int count, int hp, int atk, int def, int spd, int rng = 0) {
        CombatUnit u;
        u.id = nextId++; u.name = name; u.count = count;
        u.maxHp = u.hp = hp; u.attack = atk; u.defense = def;
        u.speed = spd; u.range = rng; u.shotsLeft = rng > 0 ? 8 : 0;
        u.isPlayer = isPlayer;
        out.push_back(u);
    };
    switch (faction) {
    case FactionId::HolyOrder:
        add("Penitent",       10, 6, 3, 2, 5);
        add("Priest",          5, 4, 2, 1, 6, 3);
        break;
    case FactionId::Bloodsworn:
        add("Raider",         10, 5, 4, 1, 7);
        add("Bone Lancer",     6, 8, 3, 3, 4);
        break;
    case FactionId::Thornkin:
        add("Thornling",      10, 4, 2, 3, 4);
        add("Bark Sentinel",   5, 12, 3, 5, 3);
        break;
    case FactionId::EternalEmpire:
        add("Skeleton",       10, 4, 2, 1, 4);
        add("Shadow Archer",   6, 4, 4, 1, 5, 4);
        break;
    case FactionId::CrimsonWardens:
        add("Warden Scout",    8, 5, 3, 2, 6);
        add("Iron Warden",     5, 10, 4, 4, 4);
        break;
    case FactionId::Voidkin:
        add("Void Wraith",     8, 5, 3, 1, 6);
        add("Rift Stalker",    4, 8, 5, 2, 7);
        break;
    case FactionId::IronAssembly:
        add("Automaton",       7, 8, 3, 3, 3);
        add("Rifleman",        6, 5, 4, 1, 5, 5);
        break;
    case FactionId::Amalgamate:
        add("Flesh Spawn",     9, 5, 2, 2, 4);
        add("Plague Bearer",   5, 8, 3, 2, 5);
        break;
    default:
        add("Soldier",        10, 5, 3, 2, 5);
        break;
    }
    return out;
}

// ── World map update ──────────────────────────────────────────────────────────
void Game::updateWorldMap(float dt)
{
    const auto& mouse = m_input.mouse();

    if (mouse.wheelY != 0.0f)
        m_camera.zoomBy(mouse.wheelY > 0 ? 1.12f : 0.88f);

    if (mouse.middle)
        m_camera.pan(-static_cast<float>(mouse.dx), -static_cast<float>(mouse.dy));

    const float PAN = 200.0f * dt;
    if (m_input.keyHeld(SDLK_LEFT))  m_camera.pan(-PAN, 0);
    if (m_input.keyHeld(SDLK_RIGHT)) m_camera.pan( PAN, 0);
    if (m_input.keyHeld(SDLK_UP))    m_camera.pan(0, -PAN);
    if (m_input.keyHeld(SDLK_DOWN))  m_camera.pan(0,  PAN);

    {
        float wx, wy;
        m_camera.screenToWorld(static_cast<float>(mouse.x),
                               static_cast<float>(mouse.y), wx, wy);
        HexCoord h = m_hexRenderer.grid().worldToHex(wx, wy);
        m_hovered = m_map.inBounds(h) ? h : HexCoord{-999,-999};
    }

    if (mouse.leftDown) {
        bool uiHandled = m_worldHUD.onMouseDown(
            static_cast<float>(mouse.x), static_cast<float>(mouse.y));
        if (!uiHandled && m_map.inBounds(m_hovered))
            onTileClicked(m_hovered);
        else if (!uiHandled)
            m_selected = {-999,-999};
    }

    m_worldHUD.onMouseMove(static_cast<float>(mouse.x),
                           static_cast<float>(mouse.y));

    updateHeroMovement(dt);

    if (m_input.keyDown(SDLK_F6)) m_showHideoutScreen   = !m_showHideoutScreen;
    if (m_input.keyDown(SDLK_F7)) m_showArtifactPanel   = !m_showArtifactPanel;
    if (m_input.keyDown(SDLK_F8)) m_showHeroInspect     = !m_showHeroInspect;

    if (m_input.keyDown(SDLK_SPACE)) {
        // Restore hero movement pools
        for (auto& h : m_heroes)      h.movePool = h.maxMove;
        for (auto& h : m_enemyHeroes) h.movePool = h.maxMove;

        // Enemy hero AI — each moves one step toward active player hero
        if (!m_heroes.empty()) {
            Hero& playerHero = m_heroes[m_activeHeroIdx];
            for (auto& eHero : m_enemyHeroes) {
                if (eHero.movePool <= 0) continue;
                auto costFn = [this, &eHero](HexCoord c) -> int {
                    const HexTile* t = m_map.getTile(c);
                    if (!t || !eHero.canEnter(t->terrain)) return 999;
                    return eHero.moveCost(t->terrain);
                };
                auto path = Pathfinder::find(m_map, eHero.pos, playerHero.pos, costFn);
                if (!path.empty()) {
                    HexCoord next = path[0];
                    int cost = [&]() {
                        const HexTile* t = m_map.getTile(next);
                        return t ? eHero.moveCost(t->terrain) : 999;
                    }();
                    if (eHero.movePool >= cost) {
                        // Clear old tile
                        if (HexTile* old = m_map.getTile(eHero.pos)) old->heroId = 0;
                        eHero.pos = next;
                        eHero.movePool -= cost;
                        if (HexTile* newT = m_map.getTile(eHero.pos)) newT->heroId = eHero.id;

                        // Check collision with player
                        if (eHero.pos == playerHero.pos) {
                            m_lastCombatEnemyId = eHero.id;
                            auto pUnits = makeFactionUnits(playerHero.faction, true);
                            auto eUnits = makeFactionUnits(eHero.faction, false);
                            enterCombat(playerHero, pUnits, eHero, eUnits);
                            return;
                        }
                    }
                }
            }
        }

        bool newWeek = m_turns.endTurn(m_towns, m_heroes,
                                       m_playerResources, m_registry);
        if (newWeek) {
            printf("New week %d — income applied\n", m_turns.week());
            ScriptContext ctx; ctx.heroId = 0;
            m_triggers.fire(TriggerType::WeekStart, ctx);
            if (m_state == GameState::Campaign) {
                m_campaign.onWeekStart(m_turns.week(), m_lua);
                for (int rt = 0; rt < RESOURCE_COUNT; ++rt) {
                    auto type = static_cast<ResourceType>(rt);
                    m_campaign.onResourcesChecked(type, m_playerResources.get(type));
                }
            }
        }
    }
}

// ── World map render ──────────────────────────────────────────────────────────
void Game::renderWorldMap()
{
    m_hexRenderer.render(m_map, m_camera, m_hovered, m_selected);

    float proj[16];
    m_camera.getMatrix(proj);
    m_batch.begin(proj);
    m_batch.end();

    for (auto& hero : m_heroes)
        drawHero(hero);
    for (auto& hero : m_enemyHeroes)
        drawHero(hero);

    m_ui.beginFrame();
    m_worldHUD.draw(m_ui, m_playerResources, m_turns,
                    m_heroes, m_activeHeroIdx);
    m_ui.endFrame();

    beginImGuiFrame();
    renderWorldOverlay();
    if (m_showLevelUpModal)   renderLevelUpModal();
    if (m_showHideoutScreen)  renderHideoutScreen();
    if (m_showArtifactPanel)  renderArtifactPanel();
    if (m_showHeroInspect)    renderHeroInspect();
    if (m_showVictory)        renderVictoryModal();
    if (m_showDefeat)         renderDefeatModal();
    endImGuiFrame();
}

// ── State transition ──────────────────────────────────────────────────────────
void Game::enterWorldMap()
{
    m_state = GameState::WorldMap;
    printf("Entered world map\n");
}

// ── Tile click ────────────────────────────────────────────────────────────────
void Game::onTileClicked(HexCoord h)
{
    const HexTile* tile = m_map.getTile(h);
    if (!tile) return;

    Hero& hero = m_heroes[m_activeHeroIdx];
    if (!hero.canEnter(tile->terrain)) return;
    if (m_moveT < 1.0f) return;

    auto costFn = [this, &hero](HexCoord c) -> int {
        const HexTile* t = m_map.getTile(c);
        if (!t || !hero.canEnter(t->terrain)) return 999;
        return hero.moveCost(t->terrain);
    };

    auto path = Pathfinder::find(m_map, hero.pos, h, costFn);
    if (path.empty()) return;

    hero.path     = path;
    hero.pathStep = 0;
    m_selected    = h;

    m_reachable = Pathfinder::reachable(m_map, hero.pos, costFn, hero.movePool);
}

// ── Hero movement ─────────────────────────────────────────────────────────────
void Game::updateHeroMovement(float dt)
{
    if (m_heroes.empty()) return;
    Hero& hero = m_heroes[m_activeHeroIdx];

    if (m_moveT < 1.0f) {
        m_moveT = std::min(1.0f, m_moveT + dt * MOVE_SPEED);
        if (m_moveT >= 1.0f)
            checkTileEvents();
        return;
    }

    if (!hero.path.empty() && hero.pathStep < static_cast<int>(hero.path.size())) {
        HexCoord next = hero.path[hero.pathStep];
        const HexTile* tile = m_map.getTile(next);
        int cost = tile ? hero.moveCost(tile->terrain) : 999;

        if (hero.movePool < cost) {
            hero.path.clear();
            hero.pathStep = 0;
            return;
        }

        float sx, sy, dx, dy;
        m_hexRenderer.grid().hexToWorld(hero.pos, sx, sy);
        m_hexRenderer.grid().hexToWorld(next, dx, dy);

        m_moveSrcX = sx; m_moveSrcY = sy;
        m_moveDstX = dx; m_moveDstY = dy;
        m_moveT    = 0.0f;

        hero.pos = next;
        hero.movePool -= cost;
        hero.pathStep++;

        FogOfWar::updateVision(m_map, hero);

        if (hero.pathStep >= static_cast<int>(hero.path.size())) {
            hero.path.clear();
            hero.pathStep = 0;
            m_selected = {-999,-999};
        }
    }
}

// ── Tile events ───────────────────────────────────────────────────────────────
void Game::checkTileEvents()
{
    if (m_heroes.empty()) return;
    Hero& hero = m_heroes[m_activeHeroIdx];
    const HexTile* tile = m_map.getTile(hero.pos);
    if (!tile) return;

    ScriptContext ctx;
    ctx.heroId     = static_cast<int>(hero.id);
    ctx.tileQ      = hero.pos.q;
    ctx.tileR      = hero.pos.r;
    ctx.playerSide = true;
    m_triggers.fireTileEnter(hero.pos, ctx);
    m_triggers.fire(TriggerType::EnterTile, ctx);

    // World objects (scrolls, chests, shrines)
    for (auto& obj : m_worldObjects) {
        if (obj.collected || !(obj.pos == hero.pos)) continue;
        obj.collected = true;
        switch (obj.type) {
        case WorldObjectType::SpellScroll: {
            bool already = false;
            for (int sid : hero.knownSpells) if (sid == obj.value) { already = true; break; }
            if (!already) {
                hero.knownSpells.push_back(obj.value);
                printf("Hero learned spell %d from scroll\n", obj.value);
            }
            break;
        }
        case WorldObjectType::ArtifactChest:
            hero.artifactInventory.push_back(obj.value);
            printf("Hero picked up artifact %d\n", obj.value);
            break;
        case WorldObjectType::XPShrine:
            if (hero.addXp(obj.value)) {
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
            printf("Hero gained %d XP from shrine\n", obj.value);
            break;
        case WorldObjectType::ResourceCache:
            m_playerResources.add(obj.resourceType, obj.value);
            printf("Hero found resource cache: %d %s\n", obj.value, resourceName(obj.resourceType));
            break;
        }
    }

    // Resource node pickup
    if (tile->resourceId != 0) {
        for (auto& r : m_resources) {
            if (r.id == tile->resourceId && !r.depleted) {
                m_playerResources.add(r.type, r.amount);
                printf("Picked up %d %s\n", r.amount, resourceName(r.type));
                r.depleted = true;
                if (HexTile* t2 = m_map.getTile(r.pos)) t2->resourceId = 0;
                break;
            }
        }
    }

    // Town entry
    if (tile->townId != 0) {
        for (auto& t : m_towns) {
            if (t.id == tile->townId) {
                enterTown(&t);
                return;
            }
        }
    }

    // Enemy hero collision
    if (tile->heroId != 0 && tile->heroId != hero.id) {
        Hero* enemyPtr = nullptr;
        for (auto& e : m_enemyHeroes)
            if (e.id == tile->heroId) { enemyPtr = &e; break; }
        if (enemyPtr) {
            m_lastCombatEnemyId = enemyPtr->id;
            auto pUnits = makeFactionUnits(hero.faction,        true);
            auto eUnits = makeFactionUnits(enemyPtr->faction,   false);
            enterCombat(hero, pUnits, *enemyPtr, eUnits);
        }
    }
}

void Game::drawHero(const Hero& hero)
{
    float wx, wy;
    if (&hero == &m_heroes[m_activeHeroIdx] && m_moveT < 1.0f) {
        wx = m_moveSrcX + (m_moveDstX - m_moveSrcX) * m_moveT;
        wy = m_moveSrcY + (m_moveDstY - m_moveSrcY) * m_moveT;
    } else {
        m_hexRenderer.grid().hexToWorld(hero.pos, wx, wy);
    }
    // Placeholder — sprite rendering added when tileset exists
    (void)wx; (void)wy;
}

// ── World entity overlay (ImGui DrawList markers) ─────────────────────────────
void Game::renderWorldOverlay()
{
    if (!m_imguiReady) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    auto project = [&](HexCoord h, float& sx, float& sy) {
        float wx, wy;
        m_hexRenderer.grid().hexToWorld(h, wx, wy);
        m_camera.worldToScreen(wx, wy, sx, sy);
    };

    // ── Towns ──────────────────────────────────────────────────────────────────
    for (const auto& town : m_towns) {
        float sx, sy;
        project(town.pos, sx, sy);
        constexpr float HS = 14.0f;
        dl->AddRectFilled({sx - HS, sy - HS}, {sx + HS, sy + HS},
                          IM_COL32(40, 80, 180, 210), 3.0f);
        dl->AddRect({sx - HS, sy - HS}, {sx + HS, sy + HS},
                    IM_COL32(120, 180, 255, 255), 3.0f, 0, 1.5f);
        dl->AddText({sx - 4, sy - 6}, IM_COL32(220, 240, 255, 255), "T");
        // Name label below
        dl->AddText({sx - town.name.size() * 3.5f, sy + HS + 2},
                    IM_COL32(200, 220, 255, 200), town.name.c_str());
    }

    // ── World objects ──────────────────────────────────────────────────────────
    for (const auto& obj : m_worldObjects) {
        if (obj.collected) continue;
        float sx, sy;
        project(obj.pos, sx, sy);
        ImU32 col;
        const char* lbl;
        switch (obj.type) {
        case WorldObjectType::SpellScroll:   col = IM_COL32(100,200,255,230); lbl = "S"; break;
        case WorldObjectType::ArtifactChest: col = IM_COL32(255,180, 50,230); lbl = "A"; break;
        case WorldObjectType::XPShrine:      col = IM_COL32(160, 80,255,230); lbl = "X"; break;
        case WorldObjectType::ResourceCache: col = IM_COL32(100,220,100,230); lbl = "R"; break;
        default:                             col = IM_COL32(200,200,200,180); lbl = "?"; break;
        }
        dl->AddCircleFilled({sx, sy}, 8.0f, col);
        dl->AddCircle({sx, sy}, 8.0f, IM_COL32(255,255,255,160), 0, 1.2f);
        dl->AddText({sx - 3, sy - 6}, IM_COL32(20, 20, 20, 255), lbl);
    }

    // ── Enemy heroes (only if tile is visible) ────────────────────────────────
    for (const auto& hero : m_enemyHeroes) {
        const HexTile* etile = m_map.getTile(hero.pos);
        if (!etile || !etile->visible) continue;
        float sx, sy;
        project(hero.pos, sx, sy);
        dl->AddCircleFilled({sx, sy}, 12.0f, IM_COL32(200, 40, 40, 220));
        dl->AddCircle({sx, sy}, 12.0f, IM_COL32(255, 140, 140, 255), 0, 1.5f);
        dl->AddText({sx - 4, sy - 6}, IM_COL32(255, 240, 240, 255), "E");
        dl->AddText({sx - hero.name.size() * 3.0f, sy + 14},
                    IM_COL32(255, 160, 160, 200), hero.name.c_str());
    }

    // ── Player heroes ─────────────────────────────────────────────────────────
    for (int i = 0; i < static_cast<int>(m_heroes.size()); ++i) {
        const auto& hero = m_heroes[i];
        float wx, wy;
        // Use animated position for active hero
        if (i == m_activeHeroIdx && m_moveT < 1.0f) {
            wx = m_moveSrcX + (m_moveDstX - m_moveSrcX) * m_moveT;
            wy = m_moveSrcY + (m_moveDstY - m_moveSrcY) * m_moveT;
        } else {
            m_hexRenderer.grid().hexToWorld(hero.pos, wx, wy);
        }
        float sx, sy;
        m_camera.worldToScreen(wx, wy, sx, sy);

        bool active = (i == m_activeHeroIdx);
        ImU32 fill = active ? IM_COL32(255, 220, 50, 230) : IM_COL32(200, 175, 40, 200);
        ImU32 ring = active ? IM_COL32(255, 255, 200, 255) : IM_COL32(200, 200, 100, 200);
        dl->AddCircleFilled({sx, sy}, 12.0f, fill);
        dl->AddCircle({sx, sy}, 12.0f, ring, 0, active ? 2.0f : 1.5f);
        dl->AddText({sx - 4, sy - 6}, IM_COL32(30, 20, 0, 255), "H");
        if (active)
            dl->AddText({sx - hero.name.size() * 3.0f, sy + 14},
                        IM_COL32(255, 230, 100, 220), hero.name.c_str());
    }
}

// ── Level-up modal ────────────────────────────────────────────────────────────
void Game::renderLevelUpModal()
{
    if (!m_showLevelUpModal) return;
    ImGui::OpenPopup("Level Up!");

    ImVec2 centre = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(centre, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Always);

    if (ImGui::BeginPopupModal("Level Up!", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        Hero& hero = m_heroes[m_activeHeroIdx];

        ImGui::Text("Congratulations! %s reached Level %d!", hero.name.c_str(), hero.level);
        ImGui::Separator();
        ImGui::Text("Choose a skill:");
        ImGui::Spacing();

        for (int i = 0; i < static_cast<int>(m_levelUpOffers.size()); ++i) {
            const auto& offer = m_levelUpOffers[i];
            ImGui::PushID(i);
            if (ImGui::Button(offer.label.c_str(), ImVec2(-1, 0))) {
                LevelUpSystem::applyOffer(offer, hero.skills);
                // Apply per-class stat growth
                const HeroClassDef* cls = m_classRegistry.getClass(hero.classId);
                if (cls) {
                    if (cls->scalesAttack) hero.attack  += 1;
                    else                   hero.defense += 1;
                    if (hero.level % 2 == 0) {
                        if (cls->scalesLightPower)  hero.lightPower  += 1;
                        if (cls->scalesBloodPower)  hero.bloodPower  += 1;
                        if (cls->scalesDeathPower)  hero.deathPower  += 1;
                        if (cls->scalesNaturePower) hero.naturePower += 1;
                        if (cls->scalesForgePower)  hero.forgePower  += 1;
                        if (cls->scalesFleshPower)  hero.fleshPower  += 1;
                    }
                    hero.maxMana += 1;
                    hero.mana = hero.maxMana;
                }
                m_levelUpOffers.clear();
                m_showLevelUpModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
        }

        ImGui::Spacing();
        ImGui::TextDisabled("XP: %d / %d", hero.xp, hero.xpToNext);
        ImGui::EndPopup();
    }
}

// ── Hideout screen ────────────────────────────────────────────────────────────
void Game::renderHideoutScreen()
{
    m_hideoutScreen.draw(m_hideout, m_showHideoutScreen);
}

// ── Artifact equip panel [F7] ─────────────────────────────────────────────────
void Game::renderArtifactPanel()
{
    if (m_heroes.empty()) return;
    Hero& hero = m_heroes[m_activeHeroIdx];

    ImGui::SetNextWindowSize(ImVec2(460, 500), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Artifacts  [F7]", &m_showArtifactPanel)) { ImGui::End(); return; }

    static const char* slotNames[] = {
        "Helm","Armor","Weapon","Shield","Ring","Boots","Cloak","Misc"
    };

    ImGui::Text("Equipped:");
    ImGui::Separator();
    for (int i = 0; i < HeroArtifacts::SLOT_COUNT; ++i) {
        int aid = hero.artifacts.equippedIds[i];
        const ArtifactDef* def = aid ? m_artifactRegistry.getDef(aid) : nullptr;
        ImGui::PushID(i);
        ImGui::Text("%-8s : %s", slotNames[i], def ? def->name.c_str() : "—");
        if (def) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Unequip")) {
                hero.artifactInventory.push_back(aid);
                hero.artifacts.unequip(static_cast<ArtifactSlot>(i));
            }
        }
        ImGui::PopID();
    }

    if (!hero.artifactInventory.empty()) {
        ImGui::Spacing();
        ImGui::Text("Inventory:");
        ImGui::Separator();
        for (int j = 0; j < static_cast<int>(hero.artifactInventory.size()); ++j) {
            int aid = hero.artifactInventory[j];
            const ArtifactDef* def = m_artifactRegistry.getDef(aid);
            if (!def) continue;
            ImGui::PushID(j + 1000);
            ImGui::Text("%s", def->name.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", def->description.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Equip")) {
                auto slot = def->slot;
                int  slotIdx = static_cast<int>(slot);
                int  old = hero.artifacts.equippedIds[slotIdx];
                if (old) hero.artifactInventory.push_back(old);
                hero.artifacts.equip(aid, slot);
                hero.artifactInventory.erase(hero.artifactInventory.begin() + j);
            }
            ImGui::PopID();
        }
    }
    ImGui::End();
}

// ── Hero inspect panel [F8] ───────────────────────────────────────────────────
void Game::renderHeroInspect()
{
    if (m_heroes.empty()) return;
    const Hero& hero = m_heroes[m_activeHeroIdx];

    ImGui::SetNextWindowSize(ImVec2(340, 460), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Hero  [F8]", &m_showHeroInspect)) { ImGui::End(); return; }

    ImGui::Text("%s", hero.name.c_str());
    ImGui::TextDisabled("Level %d  —  XP %d / %d", hero.level, hero.xp, hero.xpToNext);
    ImGui::Separator();

    ImGui::Text("ATK %d   DEF %d   Vision %d", hero.attack, hero.defense, hero.visionRange);
    ImGui::Text("Mana %d / %d   Move %d / %d",
                hero.mana, hero.maxMana, hero.movePool, hero.maxMove);
    ImGui::Spacing();

    ImGui::Text("Casting Power:");
    if (hero.lightPower)  ImGui::Text("  Light  +%d", hero.lightPower);
    if (hero.bloodPower)  ImGui::Text("  Blood  +%d", hero.bloodPower);
    if (hero.deathPower)  ImGui::Text("  Death  +%d", hero.deathPower);
    if (hero.naturePower) ImGui::Text("  Nature +%d", hero.naturePower);
    if (hero.forgePower)  ImGui::Text("  Forge  +%d", hero.forgePower);
    if (hero.fleshPower)  ImGui::Text("  Flesh  +%d", hero.fleshPower);

    if (!hero.skills.slots.empty()) {
        ImGui::Spacing();
        ImGui::Text("Skills:");
        for (auto& s : hero.skills.slots) {
            if (s.defId == 0) continue;
            const SkillDef* sd = findSkillDef(s.defId);
            const char* tierStr[] = {"Basic","Advanced","Master"};
            int t = static_cast<int>(s.tier);
            ImGui::Text("  %s (%s)", sd ? sd->name.c_str() : "?", (t >= 0 && t <= 2) ? tierStr[t] : "?");
        }
    }

    if (!hero.knownSpells.empty()) {
        ImGui::Spacing();
        ImGui::Text("Spells:");
        for (int sid : hero.knownSpells) {
            const SpellDef* sp = findSpell(sid);
            if (sp) ImGui::Text("  %s  (%d mana)", sp->name, sp->manaCost);
        }
    }
    ImGui::End();
}

// ── Victory modal ─────────────────────────────────────────────────────────────
void Game::renderVictoryModal()
{
    ImGui::OpenPopup("Victory!");
    ImVec2 centre = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(centre, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_Always);

    if (ImGui::BeginPopupModal("Victory!", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.1f, 1.0f),
                           "All enemy heroes have been defeated!");
        ImGui::Spacing();
        ImGui::TextDisabled("Day %d  Week %d  |  Gold: %d",
                            m_turns.day(), m_turns.week(),
                            m_playerResources.get(ResourceType::Gold));
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float bw = ImGui::GetWindowWidth() - 32.0f;
        if (ImGui::Button("Continue Exploring", ImVec2(bw * 0.55f, 36))) {
            m_showVictory = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Main Menu", ImVec2(-1, 36))) {
            m_showVictory = false;
            m_state = GameState::MainMenu;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// ── Defeat modal ──────────────────────────────────────────────────────────────
void Game::renderDefeatModal()
{
    ImGui::OpenPopup("Defeat");
    ImVec2 centre = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(centre, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_Always);

    if (ImGui::BeginPopupModal("Defeat", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Your army was defeated!");
        ImGui::Spacing();
        ImGui::TextDisabled("Day %d  Week %d", m_turns.day(), m_turns.week());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float bw = ImGui::GetWindowWidth() - 32.0f;
        if (ImGui::Button("Continue (retreat)", ImVec2(bw * 0.55f, 36))) {
            m_showDefeat = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Last Save", ImVec2(-1, 36))) {
            m_showDefeat = false;
            loadGame("saves/save0.json");
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
