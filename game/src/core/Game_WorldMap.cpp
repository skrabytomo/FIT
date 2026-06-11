#include "Game.h"
#include "../hero/LevelUpSystem.h"
#include "../hero/SkillRegistry.h"
#include "../magic/SpellRegistry.h"
#include <imgui.h>
#include <cmath>
#include <algorithm>
#include <stdio.h>

static constexpr float MOVE_SPEED = 4.0f;

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
                            CombatUnit pUnit;
                            pUnit.id = 1; pUnit.name = "Penitent"; pUnit.count = 10;
                            pUnit.hp = 5; pUnit.maxHp = 5; pUnit.attack = 2;
                            pUnit.defense = 1; pUnit.speed = 5; pUnit.isPlayer = true;

                            CombatUnit eUnit;
                            eUnit.id = 2; eUnit.name = "Skeleton"; eUnit.count = 10;
                            eUnit.hp = 4; eUnit.maxHp = 4; eUnit.attack = 2;
                            eUnit.defense = 1; eUnit.speed = 4; eUnit.isPlayer = false;

                            enterCombat(playerHero, {pUnit}, eHero, {eUnit});
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

    m_ui.beginFrame();
    m_worldHUD.draw(m_ui, m_playerResources, m_turns,
                    m_heroes, m_activeHeroIdx);
    m_ui.endFrame();

    beginImGuiFrame();
    if (m_showLevelUpModal)   renderLevelUpModal();
    if (m_showHideoutScreen)  renderHideoutScreen();
    if (m_showArtifactPanel)  renderArtifactPanel();
    if (m_showHeroInspect)    renderHeroInspect();
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

    // Enemy hero collision — placeholder with minimal units
    if (tile->heroId != 0 && tile->heroId != hero.id) {
        Hero enemyHero;
        enemyHero.id      = tile->heroId;
        enemyHero.name    = "Enemy Hero";
        enemyHero.faction = FactionId::EternalEmpire;

        CombatUnit pUnit;
        pUnit.id = 1; pUnit.name = "Penitent"; pUnit.count = 10;
        pUnit.hp = 5; pUnit.maxHp = 5; pUnit.attack = 2; pUnit.defense = 1;
        pUnit.speed = 5; pUnit.isPlayer = true;

        CombatUnit eUnit;
        eUnit.id = 2; eUnit.name = "Skeleton"; eUnit.count = 10;
        eUnit.hp = 4; eUnit.maxHp = 4; eUnit.attack = 2; eUnit.defense = 1;
        eUnit.speed = 4; eUnit.isPlayer = false;

        enterCombat(hero, {pUnit}, enemyHero, {eUnit});
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
