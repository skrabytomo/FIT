#include "Game.h"
#include "../hero/LevelUpSystem.h"
#include "../hero/SkillRegistry.h"
#include "../hero/HeroClass.h"
#include "../hero/Artifacts.h"
#include "../magic/SpellRegistry.h"
#include "../world/HexGrid.h"
#include "../town/UnitDef.h"
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

// Build CombatUnits from hero's actual army; falls back to faction template if army empty
static std::vector<CombatUnit> makeHeroUnits(const Hero& hero,
    const std::vector<UnitDef>& defs, bool isPlayer)
{
    if (hero.army.empty())
        return makeFactionUnits(hero.faction, isPlayer);

    std::vector<CombatUnit> out;
    int nextId = isPlayer ? 1 : 50;
    for (const auto& stack : hero.army) {
        if (stack.count <= 0) continue;
        const UnitDef* ud = nullptr;
        for (const auto& d : defs) if (d.id == stack.defId) { ud = &d; break; }
        if (!ud) continue;
        CombatUnit u;
        u.id = nextId++;
        u.defId = ud->id;
        u.name = ud->name;
        u.count = stack.count;
        u.hp = u.maxHp = ud->hp;
        u.attack   = ud->attack;
        u.defense  = ud->defense;
        u.damageMin = ud->damage_min;
        u.damageMax = ud->damage_max;
        u.speed    = ud->speed;
        u.range    = ud->range;
        u.shots    = u.shotsLeft = ud->shots;
        u.flying      = ud->flying;
        u.vampiric    = ud->vampiric;
        u.regenerates = ud->regenerates;
        u.tags        = ud->tags;
        u.isPlayer    = isPlayer;
        u.hasSecondLife = (hero.faction == FactionId::EternalEmpire);
        out.push_back(u);
    }
    return out.empty() ? makeFactionUnits(hero.faction, isPlayer) : out;
}

// Estimated combat strength: sum(count * hp * attack) per stack
static int heroStrength(const Hero& hero, const std::vector<UnitDef>& defs)
{
    if (hero.army.empty()) {
        auto units = makeFactionUnits(hero.faction, true);
        int s = 0;
        for (auto& u : units) s += u.count * u.hp * u.attack;
        return s;
    }
    int s = 0;
    for (const auto& stack : hero.army) {
        if (stack.count <= 0) continue;
        for (const auto& d : defs)
            if (d.id == stack.defId) { s += stack.count * d.hp * d.attack; break; }
    }
    return s;
}

// ── World map update ──────────────────────────────────────────────────────────
void Game::updateWorldMap(float dt)
{
    m_mapTime += dt;
    m_hexRenderer.update(dt);

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

    // Clamp camera so the player can't scroll off the map edge into black void
    {
        const float hs  = m_hexRenderer.grid().hexSize();
        const float R   = static_cast<float>(m_map.radius());
        const float pad = hs * 3.0f;           // half-screen of padding at edge
        const float limX = R * hs * 1.8f + pad;
        const float limY = R * hs * 2.0f + pad;
        float cx = std::clamp(m_camera.x(), -limX, limX);
        float cy = std::clamp(m_camera.y(), -limY, limY);
        if (cx != m_camera.x() || cy != m_camera.y())
            m_camera.setPosition(cx, cy);
    }

    {
        float wx, wy;
        m_camera.screenToWorld(static_cast<float>(mouse.x),
                               static_cast<float>(mouse.y), wx, wy);
        HexCoord h = m_hexRenderer.grid().worldToHex(wx, wy);
        m_hovered = m_map.inBounds(h) ? h : HexCoord{-999,-999};
    }

    // Cursor: fight if enemy hovered, otherwise arrow
    if (m_cursorArrow && m_cursorFight) {
        bool fight = false;
        if (m_map.inBounds(m_hovered)) {
            const HexTile* ht = m_map.getTile(m_hovered);
            if (ht && ht->visible) {
                for (const auto& e : m_enemyHeroes)
                    if (e.id == ht->heroId) { fight = true; break; }
                if (!fight && ht->townId != 0)
                    for (const auto& t : m_towns)
                        if (t.id == ht->townId && t.ownerId > 1) { fight = true; break; }
            }
        }
        SDL_SetCursor(fight ? m_cursorFight : m_cursorArrow);
    }

    if (mouse.leftDown && !ImGui::GetIO().WantCaptureMouse) {
        bool uiHandled = m_worldHUD.onMouseDown(
            static_cast<float>(mouse.x), static_cast<float>(mouse.y));

        // Minimap click: pan camera to clicked map position
        if (!uiHandled && m_map.radius() > 0) {
            constexpr float MINI_W = 150.0f, MINI_H = 150.0f, PAD = 10.0f;
            const float mm_left = PAD;
            const float mm_top  = static_cast<float>(m_height) - MINI_H - PAD;
            const float mx = static_cast<float>(mouse.x);
            const float my = static_cast<float>(mouse.y);
            if (mx >= mm_left && mx <= mm_left + MINI_W &&
                my >= mm_top  && my <= mm_top  + MINI_H) {
                const float mm_cx  = mm_left + MINI_W * 0.5f;
                const float mm_cy  = mm_top  + MINI_H * 0.5f;
                const float R      = static_cast<float>(m_map.radius());
                const float scaleX = MINI_W * 0.5f / R;
                const float scaleY = MINI_H * 0.5f / R;
                const float hs     = m_hexRenderer.grid().hexSize();
                float q_f   = (mx - mm_cx) / scaleX;
                float rq_f  = (my - mm_cy) / scaleY;
                m_camera.setPosition(hs * 1.5f * q_f, hs * 1.7320508f * rq_f);
                uiHandled = true;
            }
        }

        if (!uiHandled && m_map.inBounds(m_hovered))
            onTileClicked(m_hovered);
        else if (!uiHandled)
            m_selected = {-999,-999};
    }

    m_worldHUD.onMouseMove(static_cast<float>(mouse.x),
                           static_cast<float>(mouse.y));

    updateHeroMovement(dt);

    // Advance pickup effects (float upward, fade out)
    for (auto& e : m_pickupEffects) e.t -= dt;
    m_pickupEffects.erase(
        std::remove_if(m_pickupEffects.begin(), m_pickupEffects.end(),
            [](const PickupEffect& ef){ return ef.t <= 0.0f; }),
        m_pickupEffects.end());

    // Update world-map hero animators (lazy-init on first seen)
    for (const auto& h : m_heroes) {
        if (m_heroMapAnimators.find(h.id) == m_heroMapAnimators.end()) {
            SpriteAnimator a;
            a.faction = std::min(static_cast<int>(h.faction), NUM_FACTIONS - 1);
            a.tier = 1;
            a.setState(AnimState::Idle);
            m_heroMapAnimators[h.id] = a;
        }
        m_heroMapAnimators[h.id].update(dt);
    }
    for (const auto& h : m_enemyHeroes) {
        if (m_heroMapAnimators.find(h.id) == m_heroMapAnimators.end()) {
            SpriteAnimator a;
            a.faction = std::min(static_cast<int>(h.faction), NUM_FACTIONS - 1);
            a.tier = 1; a.mirror = true;
            a.setState(AnimState::Idle);
            m_heroMapAnimators[h.id] = a;
        }
        m_heroMapAnimators[h.id].update(dt);
    }

    if (m_input.keyDown(SDLK_F6)) m_showHideoutScreen   = !m_showHideoutScreen;
    if (m_input.keyDown(SDLK_F7)) m_showArtifactPanel   = !m_showArtifactPanel;
    if (m_input.keyDown(SDLK_F8)) m_showHeroInspect     = !m_showHeroInspect;
    if (m_input.keyDown(SDLK_m))  m_showMinimap         = !m_showMinimap;

    // G — toggle garrison (hero digs in, blocks passage until defeated)
    if (m_input.keyDown(SDLK_g) && !m_heroes.empty()) {
        Hero& h = m_heroes[m_activeHeroIdx];
        h.isGarrisoned = !h.isGarrisoned;
        printf("Hero %s %s garrison\n", h.name.c_str(),
               h.isGarrisoned ? "dug in at" : "left");
    }

    // Tab — cycle to next player hero
    if (m_input.keyDown(SDLK_TAB) && !m_heroes.empty()) {
        m_activeHeroIdx = (m_activeHeroIdx + 1) % static_cast<int>(m_heroes.size());
        const Hero& nextHero = m_heroes[m_activeHeroIdx];
        float hx2, hy2;
        m_hexRenderer.grid().hexToWorld(nextHero.pos, hx2, hy2);
        m_camera.setPosition(hx2, hy2);
        m_selected = {-999, -999};
        auto costFn2 = [this, &nextHero](HexCoord c) -> int {
            const HexTile* t = m_map.getTile(c);
            if (!t || !nextHero.canEnter(t->terrain)) return 999;
            return nextHero.moveCost(t->terrain);
        };
        m_reachable = Pathfinder::reachable(m_map, nextHero.pos, costFn2, nextHero.movePool);
    }

    if (m_input.keyDown(SDLK_SPACE)) {
        doEndTurn();
    }
}

// ── End Turn — full turn logic (SPACE key + HUD button) ───────────────────────
void Game::doEndTurn()
{
    // Reset per-day build limit for all towns
    for (auto& t : m_towns) t.builtToday = 0;

    // Restore hero movement pools and daily mana regen for enemy heroes
    for (auto& h : m_heroes)      h.movePool = h.maxMove;
    for (auto& h : m_enemyHeroes) {
        h.movePool = h.maxMove;
        int manaRegen = std::max(2, 2 + h.maxMana / 10);
        h.mana = std::min(h.maxMana, h.mana + manaRegen);
    }

        // Enemy hero AI — strength-aware, full move pool
        if (!m_heroes.empty()) {
            Hero& playerHero = m_heroes[m_activeHeroIdx];
            const auto& unitDefs = m_registry.units();
            bool combatTriggered = false;

            for (auto& eHero : m_enemyHeroes) {
                if (combatTriggered) break;

                // Recruit from any owned town within 1 tile (free for AI)
                for (auto& t : m_towns) {
                    if (t.ownerId != eHero.id) continue;
                    if (HexGrid::distance(eHero.pos, t.pos) > 1) continue;
                    for (auto& dw : t.dwellings) {
                        if (dw.available <= 0) continue;
                        for (const auto& ud : unitDefs) {
                            if (ud.faction == t.faction && ud.tier == dw.tier
                                && ud.path == dw.path) {
                                int recruited = dw.available;
                                dw.available = 0;
                                bool merged = false;
                                for (auto& s : eHero.army)
                                    if (s.defId == ud.id) { s.count += recruited; merged = true; break; }
                                if (!merged && eHero.army.size() < 7)
                                    eHero.army.push_back({ud.id, recruited});
                                break;
                            }
                        }
                    }
                }

                int eiStr = heroStrength(eHero, unitDefs);
                int plStr = heroStrength(playerHero, unitDefs);
                // Fight if we have ≥70% of player strength; otherwise focus economy
                bool aggressive = (eiStr * 10 >= plStr * 7);
                // Retreat to nearest owned town when at < 40% of player strength
                bool veryWeak   = (eiStr * 10 <  plStr * 4);

                while (eHero.movePool > 0) {
                    HexCoord goal = {};
                    bool goalSet = false;

                    auto tryGoal = [&](HexCoord pos, int bias = 0) {
                        int d = HexGrid::distance(eHero.pos, pos) - bias;
                        if (!goalSet || d < HexGrid::distance(eHero.pos, goal)) {
                            goal = pos; goalSet = true;
                        }
                    };

                    if (veryWeak) {
                        // Retreat: head to nearest owned town to regroup / garrison
                        for (const auto& t : m_towns)
                            if (t.ownerId == eHero.id) tryGoal(t.pos, 5);
                    } else {
                        // Unowned mines (always valuable)
                        for (const auto& r : m_resources) {
                            if (r.ownedBy == eHero.id) continue;
                            tryGoal(r.pos, 3);
                        }
                        // Valuable world objects (XP, spells, stat boosts, artifacts)
                        for (const auto& obj : m_worldObjects) {
                            if (obj.collected) continue;
                            if (obj.type == WorldObjectType::XPShrine     ||
                                obj.type == WorldObjectType::SpellScroll   ||
                                obj.type == WorldObjectType::StatShrine    ||
                                obj.type == WorldObjectType::ArtifactChest ||
                                obj.type == WorldObjectType::ForestShrine  ||
                                obj.type == WorldObjectType::SwampAltar) {
                                tryGoal(obj.pos, 2);
                            }
                        }
                        // Neutral towns
                        for (const auto& t : m_towns) {
                            if (t.ownerId != 0) continue;
                            tryGoal(t.pos);
                        }
                        // GhostWalk: enemy AI cannot target the player hero directly
                        bool playerGhostWalk = playerHero.ghostWalkSpecialty;
                        // Player towns / hero (only if aggressive or nothing else to do)
                        if (aggressive || !goalSet) {
                            if (!playerGhostWalk) tryGoal(playerHero.pos);
                            if (aggressive) {
                                for (const auto& t : m_towns)
                                    if (t.ownerId == 1) tryGoal(t.pos);
                            }
                        }
                    }

                    if (!goalSet) break;

                    auto costFn = [this, &eHero, aggressive](HexCoord c) -> int {
                        const HexTile* t = m_map.getTile(c);
                        if (!t || !eHero.canEnter(t->terrain)) return 999;
                        // Only block passage through player towns, not destination
                        if (!aggressive && t->townId != 0) {
                            for (const auto& town : m_towns)
                                if (town.id == t->townId && town.ownerId == 1) return 999;
                        }
                        return eHero.moveCost(t->terrain);
                    };
                    auto path = Pathfinder::find(m_map, eHero.pos, goal, costFn);
                    if (path.empty()) break;

                    HexCoord next = path[0];
                    const HexTile* nextTile = m_map.getTile(next);
                    if (!nextTile) break;
                    int cost = eHero.moveCost(nextTile->terrain);
                    if (eHero.movePool < cost) break;

                    // Move
                    if (HexTile* old = m_map.getTile(eHero.pos)) old->heroId = 0;
                    eHero.pos = next;
                    eHero.movePool -= cost;
                    if (HexTile* nT = m_map.getTile(eHero.pos)) nT->heroId = eHero.id;

                    // Combat with player?
                    if (eHero.pos == playerHero.pos) {
                        m_lastCombatEnemyId = eHero.id;
                        auto pUnits = makeHeroUnits(playerHero, unitDefs, true);
                        auto eUnits = makeHeroUnits(eHero, unitDefs, false);
                        enterCombat(playerHero, pUnits, eHero, eUnits);
                        combatTriggered = true;
                        break;
                    }

                    // Collect world objects — apply meaningful effects to enemy hero
                    for (auto& obj : m_worldObjects) {
                        if (obj.collected || obj.pos != eHero.pos) continue;
                        obj.collected = true;
                        if (obj.type == WorldObjectType::XPShrine) {
                            int prevLevel = eHero.level;
                            if (eHero.addXp(obj.value) && eHero.level > prevLevel) {
                                // Grant stat bonus on level up (alternating ATK/DEF)
                                int gained = eHero.level - prevLevel;
                                eHero.attack  += (gained + 1) / 2;
                                eHero.defense += gained / 2;
                            }
                            printf("Enemy %s gained %d XP from shrine\n",
                                   eHero.name.c_str(), obj.value);
                        } else if (obj.type == WorldObjectType::SpellScroll) {
                            bool already = false;
                            for (int sid : eHero.knownSpells)
                                if (sid == obj.value) { already = true; break; }
                            if (!already) eHero.knownSpells.push_back(obj.value);
                        } else if (obj.type == WorldObjectType::StatShrine) {
                            // Alternate ATK and DEF based on current stats
                            if (eHero.attack <= eHero.defense) eHero.attack++;
                            else eHero.defense++;
                        } else if (obj.type == WorldObjectType::ArtifactChest) {
                            eHero.artifactInventory.push_back(obj.value);
                        } else if (obj.type == WorldObjectType::ForestShrine) {
                            int prevLevel = eHero.level;
                            if (eHero.addXp(obj.value) && eHero.level > prevLevel) {
                                int gained = eHero.level - prevLevel;
                                eHero.attack  += (gained + 1) / 2;
                                eHero.defense += gained / 2;
                            }
                            printf("Enemy %s gained %d XP from forest shrine\n",
                                   eHero.name.c_str(), obj.value);
                        } else if (obj.type == WorldObjectType::SwampAltar) {
                            bool already = false;
                            for (int sid : eHero.knownSpells)
                                if (sid == obj.value) { already = true; break; }
                            if (!already) eHero.knownSpells.push_back(obj.value);
                        }
                    }

                    // Claim resource node (mine control)
                    if (nextTile->resourceId != 0) {
                        for (auto& r : m_resources) {
                            if (r.id == nextTile->resourceId) {
                                r.ownedBy = eHero.id;
                                break;
                            }
                        }
                    }

                    // Capture neutral towns / siege player towns / garrison at own towns
                    if (nextTile->townId != 0) {
                        for (auto& t : m_towns) {
                            if (t.id != nextTile->townId) continue;
                            if (t.ownerId == eHero.id && veryWeak && !eHero.army.empty()) {
                                // Retreating hero deposits their smallest stack as garrison
                                int weakIdx = 0;
                                for (int i = 1; i < (int)eHero.army.size(); ++i)
                                    if (eHero.army[i].count < eHero.army[weakIdx].count) weakIdx = i;
                                auto& stack = eHero.army[weakIdx];
                                int deposit = stack.count / 2;
                                if (deposit > 0 && t.garrison.size() < 7) {
                                    bool merged = false;
                                    for (auto& gs : t.garrison)
                                        if (gs.defId == stack.defId) { gs.count += deposit; merged = true; break; }
                                    if (!merged) t.garrison.push_back({stack.defId, deposit});
                                    stack.count -= deposit;
                                    if (stack.count == 0)
                                        eHero.army.erase(eHero.army.begin() + weakIdx);
                                }
                                eHero.movePool = 0; // done retreating for this turn
                            } else if (t.ownerId == 0) {
                                t.ownerId = eHero.id;
                                printf("Enemy %s captured %s\n", eHero.name.c_str(), t.name.c_str());
                            } else if (t.ownerId == 1) {
                                // Off-screen siege: compare attacker vs garrison strength
                                Hero garHero;
                                garHero.faction = t.faction;
                                garHero.army    = t.garrison;
                                int atkStr = heroStrength(eHero, unitDefs);
                                int defStr = heroStrength(garHero, unitDefs);
                                if (t.hasBuilding(BID::FORT)) defStr = defStr * 3 / 2;
                                if (atkStr > defStr) {
                                    t.ownerId = eHero.id;
                                    t.garrison.clear();
                                    m_lostTownName       = t.name;
                                    m_showTownLostPopup  = true;
                                    printf("Enemy %s sieged and captured your town %s!\n",
                                           eHero.name.c_str(), t.name.c_str());
                                } else {
                                    printf("Enemy %s failed to siege %s\n",
                                           eHero.name.c_str(), t.name.c_str());
                                }
                                eHero.movePool = 0; // siege exhausts movement
                            }
                            break;
                        }
                    }
                }
                if (combatTriggered) return;
            }
        }

        // Infestation specialty (Flesh Architect/Amalgamate): FleshZone spreads each turn
        auto applyInfestation = [&](std::vector<Hero>& heroList) {
            for (auto& hero : heroList) {
                if (!hero.infestationSpecialty) continue;
                constexpr int INFEST_RADIUS = 2;
                std::vector<HexCoord> toInfest;
                for (const auto& coord : m_map.coords()) {
                    if (HexGrid::distance(hero.pos, coord) > INFEST_RADIUS) continue;
                    HexTile* t = m_map.getTile(coord);
                    if (!t || t->terrain == Terrain::FleshZone) continue;
                    bool adjacentFlesh = false;
                    for (const auto& nb : HexGrid::neighbors(coord)) {
                        const HexTile* nt = m_map.getTile(nb);
                        if (nt && nt->terrain == Terrain::FleshZone) { adjacentFlesh = true; break; }
                    }
                    Terrain ter = t->terrain;
                    if (adjacentFlesh && (ter == Terrain::Plains || ter == Terrain::Wasteland
                                          || ter == Terrain::Corrupted || ter == Terrain::Barren)) {
                        toInfest.push_back(coord);
                    }
                }
                if (!toInfest.empty()) {
                    int converted = 0;
                    for (const auto& c : toInfest) {
                        if (converted >= 2) break;
                        HexTile* t = m_map.getTile(c);
                        if (t) { t->terrain = Terrain::FleshZone; converted++; }
                    }
                    if (converted > 0) {
                        char buf[48];
                        std::snprintf(buf, sizeof(buf), "Infestation: +%d FleshZone", converted);
                        pushPickupEffect(hero.pos, buf, IM_COL32(180, 100, 60, 255));
                    }
                }
            }
        };
        applyInfestation(m_heroes);
        applyInfestation(m_enemyHeroes);

        // BlightAura specialty (Blight Caller/Voidkin): Sacred terrain near the hero
        // is passively corrupted each turn. Applies to both player and enemy heroes.
        auto applyBlightAura = [&](std::vector<Hero>& heroList) {
            for (auto& hero : heroList) {
                if (!hero.blightAuraSpecialty) continue;
                constexpr int BLIGHT_RADIUS = 3;
                int corrupted = 0;
                for (const auto& coord : m_map.coords()) {
                    if (HexGrid::distance(hero.pos, coord) > BLIGHT_RADIUS) continue;
                    HexTile* t = m_map.getTile(coord);
                    if (t && t->terrain == Terrain::Sacred) {
                        t->terrain = Terrain::Corrupted;
                        corrupted++;
                    }
                }
                if (corrupted > 0) {
                    char buf[64];
                    std::snprintf(buf, sizeof(buf), "BlightAura: %d Sacred → Corrupted", corrupted);
                    pushPickupEffect(hero.pos, buf, IM_COL32(160, 80, 200, 255));
                }
            }
        };
        applyBlightAura(m_heroes);
        applyBlightAura(m_enemyHeroes);

        bool newWeek = m_turns.endTurn(m_towns, m_heroes,
                                       m_playerResources, m_registry);
        if (newWeek) {
            // Capture income totals for week summary popup before adding them
            m_weekSummaryIncome = m_turns.calculateWeeklyIncome(m_towns, 1);
            for (const auto& r : m_resources)
                if (r.ownedBy == 1) m_weekSummaryIncome.add(r.type, r.amount);
            m_cachedWeeklyIncome = m_weekSummaryIncome;
            m_weekSummaryWeek = m_turns.week();
            m_showWeekSummary = true;

            // Mine income for player-controlled resource nodes
            for (const auto& r : m_resources)
                if (r.ownedBy == 1) m_playerResources.add(r.type, r.amount);

            printf("New week %d — income applied\n", m_turns.week());

            // Enemy hero weekly reinforcements — scale with week number so they stay relevant
            {
                int week = m_turns.week();
                int reinforceCount = 2 + week;  // 3 on week 1, grows by 1 per week
                for (auto& eHero : m_enemyHeroes) {
                    if (eHero.army.empty()) continue;
                    // Count towns owned by this enemy hero
                    int ownedTowns = 0;
                    for (const auto& t : m_towns)
                        if (t.ownerId == eHero.id) ownedTowns++;
                    if (ownedTowns == 0) continue;  // no base → no reinforcements
                    // Add reinforceCount units to the smallest stack (per owned town)
                    int total = reinforceCount * ownedTowns;
                    int smallestIdx = 0;
                    for (int i = 1; i < (int)eHero.army.size(); ++i)
                        if (eHero.army[i].count < eHero.army[smallestIdx].count)
                            smallestIdx = i;
                    eHero.army[smallestIdx].count = std::min(50, eHero.army[smallestIdx].count + total);
                    printf("Enemy %s reinforced +%d units (week %d, %d towns)\n",
                           eHero.name.c_str(), total, week, ownedTowns);
                }
            }

            // Auto-save at start of each new week
            saveGame("saves/save" + std::to_string(m_activeSlot) + ".json");

            // ── AI town building: one building per week, priority dwellings ──────────
            {
                Resources richRes;
                richRes.add(ResourceType::Gold,         999999);
                richRes.add(ResourceType::Iron,            9999);
                richRes.add(ResourceType::FaithStones,     9999);
                richRes.add(ResourceType::BloodEssence,    9999);
                richRes.add(ResourceType::VerdantSap,      9999);
                richRes.add(ResourceType::Mercury,         9999);

                const auto& allBuildings = m_registry.buildings();

                for (auto& town : m_towns) {
                    if (town.ownerId <= 1) continue;
                    town.builtToday = 0;

                    bool built = false;
                    // Priority 1: lowest unbought base dwelling (tier 1-6)
                    for (int tier = 1; tier <= 6 && !built; ++tier) {
                        for (const auto& def : allBuildings) {
                            if (def.category != BuildingCategory::UnitDwelling) continue;
                            if (def.faction != town.faction) continue;
                            if (def.tier != tier) continue;
                            if (def.path != UpgradePath::None) continue;
                            Resources tmp = richRes;
                            if (town.build(def.id, allBuildings, tmp)) {
                                printf("AI %s built %s\n", town.name.c_str(), def.name.c_str());
                                built = true; break;
                            }
                        }
                    }
                    // Priority 2: fort or support
                    if (!built) {
                        for (const auto& def : allBuildings) {
                            if (def.faction != town.faction && def.faction != FactionId::None) continue;
                            if (def.category != BuildingCategory::Fort &&
                                def.category != BuildingCategory::Support) continue;
                            Resources tmp = richRes;
                            if (town.build(def.id, allBuildings, tmp)) {
                                printf("AI %s built %s\n", town.name.c_str(), def.name.c_str());
                                built = true; break;
                            }
                        }
                    }
                }
            }

            // ── Weekly random event ────────────────────────────────────────────
            m_weeklyEventHeadline.clear();
            m_weeklyEventBody.clear();
            // Use week number + a pseudo-hash for varied but deterministic events
            int evtRoll = ((m_turns.week() * 2654435761u) >> 8) % 20;
            switch (evtRoll) {
                case 0: { // no event
                    break;
                }
                case 1: { // Merchant's Gift — bonus gold
                    m_playerResources.add(ResourceType::Gold, 500);
                    m_weeklyEventHeadline = "A Merchant's Gift";
                    m_weeklyEventBody = "A wandering trader pays 500 Gold for safe passage through your lands.";
                    break;
                }
                case 2: { // Wandering Wizard — learn a random unknown spell
                    if (!m_heroes.empty()) {
                        Hero& h = m_heroes[m_activeHeroIdx];
                        for (int i = 0; i < SPELL_COUNT; ++i) {
                            int sid = ALL_SPELLS[i].id;
                            bool known = false;
                            for (int s : h.knownSpells) if (s == sid) { known = true; break; }
                            if (!known) {
                                h.knownSpells.push_back(sid);
                                m_weeklyEventHeadline = "Wandering Wizard";
                                m_weeklyEventBody = std::string("A sage teaches your hero: ")
                                                  + ALL_SPELLS[i].name + "!";
                                break;
                            }
                        }
                    }
                    break;
                }
                case 3: { // Bandit Raid — lose gold
                    int lost = std::min(200, m_playerResources.get(ResourceType::Gold));
                    m_playerResources.add(ResourceType::Gold, -lost);
                    m_weeklyEventHeadline = "Bandit Raid!";
                    m_weeklyEventBody = "Raiders struck your supply wagons, stealing "
                                      + std::to_string(lost) + " Gold.";
                    break;
                }
                case 4: { // Rich Harvest — bonus resources
                    m_playerResources.add(ResourceType::Gold, 200);
                    m_playerResources.add(ResourceType::Iron, 3);
                    m_weeklyEventHeadline = "Rich Harvest";
                    m_weeklyEventBody = "Abundant yields from your territories: +200 Gold, +3 Iron.";
                    break;
                }
                case 5: { // Heroic Inspiration — XP boost
                    if (!m_heroes.empty()) {
                        Hero& h = m_heroes[m_activeHeroIdx];
                        int xpGain = 150;
                        int oldLvl5 = h.level;
                        if (h.addXp(xpGain)) {
                            const HeroClassDef* cls = m_classRegistry.getClass(h.classId);
                            if (cls) {
                                std::vector<SkillDef> allSkills(SKILL_DEFS, SKILL_DEFS + SKILL_DEF_COUNT);
                                m_levelUpOffers = LevelUpSystem::generateOffers(
                                    *cls, h.skills, h.level, allSkills, h.faction);
                            }
                            if (m_levelUpOffers.empty())
                                m_levelUpOffers.push_back({SkillID::OFFENSE, false, false, "Learn Offense"});
                            m_pendingLevelUps = h.level - oldLvl5;
                            m_showLevelUpModal = true;
                            { ScriptContext lvCtx; lvCtx.heroId = h.id; m_triggers.fire(TriggerType::HeroLevel, lvCtx); }
                        }
                        m_weeklyEventHeadline = "Battle Hardened";
                        m_weeklyEventBody = "Tales of your deeds spread: +"
                                           + std::to_string(xpGain) + " XP.";
                    }
                    break;
                }
                case 6: { // Arcane Font — bonus mana for the hero
                    if (!m_heroes.empty()) {
                        Hero& h = m_heroes[m_activeHeroIdx];
                        h.maxMana = std::min(h.maxMana + 5, 99);
                        h.mana    = h.maxMana;
                        m_weeklyEventHeadline = "Arcane Font";
                        m_weeklyEventBody = "A ley-line resonance permanently expands your hero's mana pool by 5.";
                    }
                    break;
                }
                case 7: { // Ancient Armory — hero gains +1 Attack
                    if (!m_heroes.empty()) {
                        Hero& h = m_heroes[m_activeHeroIdx];
                        h.attack++;
                        m_weeklyEventHeadline = "Ancient Armory";
                        m_weeklyEventBody = "You unearth a cache of fine weapons from an old war. Your hero gains +1 Attack.";
                    }
                    break;
                }
                case 8: { // Rally! — strongest army stack grows
                    if (!m_heroes.empty()) {
                        Hero& h = m_heroes[m_activeHeroIdx];
                        int bestCount = 0; int bestIdx = -1;
                        for (int i = 0; i < (int)h.army.size(); ++i)
                            if (h.army[i].count > bestCount) { bestCount = h.army[i].count; bestIdx = i; }
                        if (bestIdx >= 0) {
                            h.army[bestIdx].count += 5;
                            m_weeklyEventHeadline = "Rally!";
                            m_weeklyEventBody = "Volunteers flock to your banner, reinforcing your ranks with 5 more fighters.";
                        }
                    }
                    break;
                }
                case 9: { // Magical Storm — enemy heroes lose mana
                    for (auto& eh : m_enemyHeroes) eh.mana = std::max(0, eh.mana - 5);
                    m_weeklyEventHeadline = "Magical Storm";
                    m_weeklyEventBody = "A surge of wild magic disperses spell reserves. Enemy heroes lose 5 mana.";
                    break;
                }
                case 10: { // Tribute from Vassals — multi-resource bonus
                    m_playerResources.add(ResourceType::Gold,        300);
                    m_playerResources.add(ResourceType::FaithStones,   2);
                    m_playerResources.add(ResourceType::VerdantSap,    2);
                    m_weeklyEventHeadline = "Tribute from Vassals";
                    m_weeklyEventBody = "Subject villages send tribute: +300 Gold, +2 Faith Stones, +2 Verdant Sap.";
                    break;
                }
                case 11: { // Plague — garrison defenders weakened
                    int lostTotal = 0;
                    for (auto& t : m_towns) {
                        if (t.ownerId != 1 || t.garrison.empty()) continue;
                        for (auto& s : t.garrison) {
                            int lost = std::max(0, s.count / 5);
                            s.count -= lost;
                            lostTotal += lost;
                        }
                    }
                    m_weeklyEventHeadline = "Plague Sweeps the Land!";
                    m_weeklyEventBody = "A virulent sickness culls your town garrisons. "
                        + (lostTotal > 0 ? std::to_string(lostTotal) + " garrison troops perished."
                                         : "Your towns were untouched — no garrison losses.");
                    break;
                }
                case 12: { // Fallen Knight — hero gains +1 Defense
                    if (!m_heroes.empty()) {
                        Hero& h = m_heroes[m_activeHeroIdx];
                        h.defense++;
                        m_weeklyEventHeadline = "Fallen Knight's Legacy";
                        m_weeklyEventBody = "You bury a fallen champion and claim his mantle. Your hero gains +1 Defense.";
                    }
                    break;
                }
                case 13: { // Mercenary Camp — strongest stack grows by 8
                    if (!m_heroes.empty()) {
                        Hero& h = m_heroes[m_activeHeroIdx];
                        int best = 0, bestIdx = -1;
                        for (int i = 0; i < (int)h.army.size(); ++i)
                            if (h.army[i].count > best) { best = h.army[i].count; bestIdx = i; }
                        if (bestIdx >= 0) {
                            h.army[bestIdx].count += 8;
                            m_weeklyEventHeadline = "Mercenary Camp";
                            m_weeklyEventBody = "Hired blades swell your ranks: +8 fighters join your strongest unit.";
                        }
                    }
                    break;
                }
                case 14: { // Scouting Report — enemy hero mana drained + player gets gold
                    for (auto& eh : m_enemyHeroes) eh.mana = std::max(0, eh.mana - 8);
                    m_playerResources.add(ResourceType::Gold, 150);
                    m_weeklyEventHeadline = "Spy Network Pays Off";
                    m_weeklyEventBody = "Your agents disrupt enemy supply lines: +150 Gold, enemy heroes lose 8 mana.";
                    break;
                }
                case 15: { // Alchemy — rare resources
                    m_playerResources.add(ResourceType::Mercury, 2);
                    m_playerResources.add(ResourceType::BloodEssence, 1);
                    m_weeklyEventHeadline = "Alchemist's Discovery";
                    m_weeklyEventBody = "A rogue alchemist delivers rare reagents: +2 Mercury, +1 Blood Essence.";
                    break;
                }
                case 16: { // Divine Favour — hero fully restores mana
                    if (!m_heroes.empty()) {
                        Hero& h = m_heroes[m_activeHeroIdx];
                        h.mana = h.maxMana;
                        m_weeklyEventHeadline = "Divine Favour";
                        m_weeklyEventBody = "A radiant vision renews your hero's magical reserves. Mana fully restored.";
                    }
                    break;
                }
                case 17: { // Enemy Deserters — XP + small unit join
                    if (!m_heroes.empty()) {
                        Hero& h = m_heroes[m_activeHeroIdx];
                        int xp = 100;
                        int oldLvl17 = h.level;
                        if (h.addXp(xp)) {
                            const HeroClassDef* cls = m_classRegistry.getClass(h.classId);
                            if (cls) {
                                std::vector<SkillDef> allSkills(SKILL_DEFS, SKILL_DEFS + SKILL_DEF_COUNT);
                                m_levelUpOffers = LevelUpSystem::generateOffers(
                                    *cls, h.skills, h.level, allSkills, h.faction);
                            }
                            if (m_levelUpOffers.empty())
                                m_levelUpOffers.push_back({SkillID::OFFENSE, false, false, "Learn Offense"});
                            m_pendingLevelUps = h.level - oldLvl17;
                            m_showLevelUpModal = true;
                            { ScriptContext lvCtx; lvCtx.heroId = h.id; m_triggers.fire(TriggerType::HeroLevel, lvCtx); }
                        }
                        // Also add 3 to hero's weakest stack
                        int least = INT32_MAX, leastIdx = -1;
                        for (int i = 0; i < (int)h.army.size(); ++i)
                            if (h.army[i].count > 0 && h.army[i].count < least)
                                { least = h.army[i].count; leastIdx = i; }
                        if (leastIdx >= 0) h.army[leastIdx].count += 3;
                    }
                    m_weeklyEventHeadline = "Enemy Deserters";
                    m_weeklyEventBody = "Enemy soldiers defect to your cause, bringing +100 XP and 3 recruits for your smallest unit.";
                    break;
                }
                case 18: { // Tax Revolt — gold halved (one-time penalty)
                    int lost = m_playerResources.get(ResourceType::Gold) / 2;
                    m_playerResources.add(ResourceType::Gold, -lost);
                    m_weeklyEventHeadline = "Tax Revolt!";
                    m_weeklyEventBody = "Overtaxed peasants revolt and seize half your treasury. Lost: "
                        + std::to_string(lost) + " Gold.";
                    break;
                }
                case 19: { // Titan's Favour — hero max HP +15
                    if (!m_heroes.empty()) {
                        Hero& h = m_heroes[m_activeHeroIdx];
                        h.heroMaxHp += 15;
                        h.heroHp = std::min(h.heroHp + 15, h.heroMaxHp);
                        m_weeklyEventHeadline = "Titan's Favour";
                        m_weeklyEventBody = "A titan spirit blesses your hero's endurance. Max HP permanently increased by 15.";
                    }
                    break;
                }
            }

            ScriptContext ctx; ctx.heroId = 0;
            m_triggers.fire(TriggerType::WeekStart, ctx);
            if (m_turns.week() >= 10)
                m_hideout.completeMilestone(Milestone::WEEK_10_REACHED);
            if (m_state == GameState::Campaign) {
                m_campaign.onWeekStart(m_turns.week(), m_lua);
                for (int rt = 0; rt < RESOURCE_COUNT; ++rt) {
                    auto type = static_cast<ResourceType>(rt);
                    m_campaign.onResourcesChecked(type, m_playerResources.get(type));
                }
            }
        }
        if (newWeek) {
            // Add weekly growth to unit dwellings
            for (auto& obj : m_worldObjects) {
                if (obj.type == WorldObjectType::UnitDwelling && !obj.collected) {
                    int tier = obj.value;
                    obj.available += 3 + tier;  // T1=4, T6=9 per week
                }
                // Observatory resets (allow re-use each week)
                if (obj.type == WorldObjectType::Observatory)
                    obj.collected = false;
                // HolyFountain / Oasis reset weekly
                if (obj.type == WorldObjectType::HolyFountain ||
                    obj.type == WorldObjectType::Oasis)
                    obj.collected = false;
            }

            // Auto-save at week start if enabled
            if (m_settingsAutoSave && m_activeSlot >= 0) {
                saveGame("saves/save" + std::to_string(m_activeSlot) + ".json");
            }
        }
    }

// ── World map render ──────────────────────────────────────────────────────────
void Game::renderWorldMap()
{
    m_hexRenderer.render(m_map, m_camera, m_hovered, m_selected, m_fogDisabled);

    float proj[16];
    m_camera.getMatrix(proj);
    m_batch.begin(proj);
    m_batch.end();

    for (auto& hero : m_heroes)
        drawHero(hero);
    for (auto& hero : m_enemyHeroes)
        drawHero(hero);

    beginImGuiFrame();
    m_ui.beginFrame();
    m_worldHUD.draw(m_ui, m_playerResources, m_cachedWeeklyIncome,
                    m_turns, m_heroes, m_activeHeroIdx);
    m_ui.endFrame();
    m_ui.flushText(ImGui::GetBackgroundDrawList());
    renderWorldOverlay();
    if (m_showLevelUpModal)   renderLevelUpModal();
    if (m_showHideoutScreen)  renderHideoutScreen();
    if (m_showArtifactPanel)  renderArtifactPanel();
    if (m_showHeroInspect)    renderHeroInspect();
    if (m_showUnitExchange)   renderUnitExchange();
    if (m_showDwellingPopup)    renderDwellingPopup();
    if (m_showStatShrinePopup)  renderStatShrinePopup();
    if (m_showQuestPopup)       renderQuestPopup();
    if (m_showTownLostPopup)  renderTownLostPopup();
    if (m_showWeekSummary)    renderWeekSummary();
    if (m_showPauseMenu)      renderPauseMenu();
    if (m_showCombatResult)   renderCombatResultPopup();
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

    // Left-click on a player-owned town opens the town screen directly
    if (tile->townId != 0) {
        for (auto& t : m_towns) {
            if (t.id == tile->townId && t.ownerId == 1) {
                enterTown(&t);
                return;
            }
        }
    }

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

        // Update tile hero IDs for this player hero
        if (HexTile* oldT = m_map.getTile(hero.pos)) oldT->heroId = 0;
        hero.pos = next;
        hero.movePool -= cost;
        hero.pathStep++;
        if (HexTile* newT = m_map.getTile(hero.pos)) newT->heroId = hero.id;

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

    if (m_state == GameState::Campaign)
        m_campaign.onTileReached(hero.pos);

    // ── Terrain traversal effects ────────────────────────────────────────────
    {
        // Count total army strength to scale danger
        int totalCount = 0;
        for (const auto& s : hero.army) totalCount += s.count;

        switch (tile->terrain) {
        case Terrain::Toxic: {
            // Poisonous vapors — kill one unit from the smallest non-empty stack
            int smallestCount = INT32_MAX, smallestIdx = -1;
            for (int i = 0; i < (int)hero.army.size(); ++i)
                if (hero.army[i].count > 0 && hero.army[i].count < smallestCount)
                    { smallestCount = hero.army[i].count; smallestIdx = i; }
            if (smallestIdx >= 0 && totalCount > 1) {
                hero.army[smallestIdx].count = std::max(0, hero.army[smallestIdx].count - 1);
                pushPickupEffect(hero.pos, "Toxic vapors — 1 unit lost!", IM_COL32(130, 200, 50, 255));
            }
            hero.mana = std::max(0, hero.mana - 1);
            break;
        }
        case Terrain::Volcanic: {
            // Lava heat — kill one unit from a random stack (not the last one)
            if (totalCount > 2) {
                int idx = static_cast<int>((hero.pos.q * 31 + hero.pos.r * 17) % (int)hero.army.size());
                for (int i = 0; i < (int)hero.army.size(); ++i) {
                    int try_ = (idx + i) % (int)hero.army.size();
                    if (hero.army[try_].count > 0) {
                        hero.army[try_].count--;
                        pushPickupEffect(hero.pos, "Volcanic heat — 1 unit slain!", IM_COL32(220, 80, 30, 255));
                        break;
                    }
                }
            }
            break;
        }
        case Terrain::Corrupted:
        case Terrain::CorruptedForest:
            // Dark energy — mana drain
            if (hero.mana > 0) {
                hero.mana = std::max(0, hero.mana - 2);
                pushPickupEffect(hero.pos, "Corrupted — -2 mana", IM_COL32(160, 60, 200, 255));
            }
            break;
        case Terrain::Sacred:
            // Holy ground — restore 1 unit to weakest stack and heal hero HP
            if (!hero.army.empty()) {
                int leastCount = INT32_MAX, leastIdx = -1;
                for (int i = 0; i < (int)hero.army.size(); ++i)
                    if (hero.army[i].count > 0 && hero.army[i].count < leastCount)
                        { leastCount = hero.army[i].count; leastIdx = i; }
                if (leastIdx >= 0) hero.army[leastIdx].count++;
            }
            hero.heroHp = std::min(hero.heroMaxHp, hero.heroHp + 5);
            pushPickupEffect(hero.pos, "Sacred ground — healing", IM_COL32(200, 255, 180, 255));
            break;
        case Terrain::Industrial:
            // Machine district — passive gold income
            m_playerResources.add(ResourceType::Gold, 10);
            pushPickupEffect(hero.pos, "+10 Gold", IM_COL32(255, 215, 50, 255));
            break;
        default: break;
        }
    }

    // World objects (scrolls, chests, shrines, etc.)
    for (auto& obj : m_worldObjects) {
        if (obj.pos != hero.pos) continue;

        switch (obj.type) {
        case WorldObjectType::SpellScroll:
            if (!obj.collected) {
                obj.collected = true;
                bool already = false;
                for (int sid : hero.knownSpells) if (sid == obj.value) { already = true; break; }
                if (!already) {
                    hero.knownSpells.push_back(obj.value);
                    const SpellDef* sp = findSpell(obj.value);
                    char sBuf[64];
                    std::snprintf(sBuf, sizeof(sBuf), "Learned: %s!", sp ? sp->name : "Spell");
                    pushPickupEffect(obj.pos, sBuf, IM_COL32(180, 120, 255, 255));
                    m_audio.playSound("spell");
                    printf("Hero learned spell %d from scroll\n", obj.value);
                }
            }
            break;
        case WorldObjectType::ArtifactChest:
            if (!obj.collected) {
                obj.collected = true;
                hero.artifactInventory.push_back(obj.value);
                pushPickupEffect(obj.pos, "Artifact found!", IM_COL32(255, 200, 80, 255));
                m_audio.playSound("pickup");
                printf("Hero picked up artifact %d\n", obj.value);
            }
            break;
        case WorldObjectType::XPShrine:
            if (!obj.collected) {
                obj.collected = true;
                char xpBuf[32]; std::snprintf(xpBuf, sizeof(xpBuf), "+%d XP", obj.value);
                pushPickupEffect(obj.pos, xpBuf, IM_COL32(160, 255, 160, 255));
                m_audio.playSound("pickup");
                {
                    int oldLvlXP = hero.level;
                    if (hero.addXp(obj.value)) {
                        const HeroClassDef* cls = m_classRegistry.getClass(hero.classId);
                        if (cls) {
                            std::vector<SkillDef> allSkills(SKILL_DEFS, SKILL_DEFS + SKILL_DEF_COUNT);
                            m_levelUpOffers = LevelUpSystem::generateOffers(
                                *cls, hero.skills, hero.level, allSkills, hero.faction);
                        }
                        if (m_levelUpOffers.empty())
                            m_levelUpOffers.push_back({SkillID::OFFENSE, false, false, "Learn Offense"});
                        m_pendingLevelUps = hero.level - oldLvlXP;
                        m_showLevelUpModal = true;
                        m_audio.playSound("levelup");
                        { ScriptContext lvCtx; lvCtx.heroId = hero.id; m_triggers.fire(TriggerType::HeroLevel, lvCtx); }
                    }
                }
                printf("Hero gained %d XP from shrine\n", obj.value);
            }
            break;
        case WorldObjectType::ResourceCache:
            if (!obj.collected) {
                obj.collected = true;
                m_playerResources.add(obj.resourceType, obj.value);
                char resBuf[48]; std::snprintf(resBuf, sizeof(resBuf), "+%d %s",
                    obj.value, resourceName(obj.resourceType));
                pushPickupEffect(obj.pos, resBuf, IM_COL32(255, 215, 80, 255));
                m_audio.playSound("pickup");
                printf("Hero found resource cache: %d %s\n", obj.value, resourceName(obj.resourceType));
            }
            break;
        case WorldObjectType::Observatory:
            if (!obj.collected) {
                obj.collected = true;  // will reset weekly
                // Reveal tiles in radius
                auto cells = HexGrid::range(hero.pos, obj.value);
                for (auto& c : cells) {
                    if (HexTile* t = m_map.getTile(c)) {
                        t->explored = true;
                        t->visible  = true;
                    }
                }
                pushPickupEffect(obj.pos, "Map revealed!", IM_COL32(220, 200, 120, 255));
                m_audio.playSound("pickup");
                printf("Observatory: revealed %d tiles in radius %d\n",
                       static_cast<int>(cells.size()), obj.value);
            }
            break;
        case WorldObjectType::StatShrine:
            if (obj.questState > 0) {
                m_pendingObjId = obj.id;
                m_showStatShrinePopup = true;
            }
            break;
        case WorldObjectType::BanditCamp:
            if (!obj.collected) {
                m_lastBanditCampId = obj.id;
                // Generate bandit army based on difficulty
                Hero banditHero;
                banditHero.id     = 0;
                banditHero.name   = "Bandit Leader";
                banditHero.faction = FactionId::None;
                int diff = obj.value;
                std::vector<CombatUnit> banditUnits;
                {
                    CombatUnit u;
                    u.id = 50; u.name = "Bandit"; u.count = 5 * diff;
                    u.maxHp = u.hp = 5; u.attack = 2 + diff; u.defense = 1 + diff;
                    u.speed = 5; u.range = 0; u.shotsLeft = 0;
                    u.isPlayer = false;
                    banditUnits.push_back(u);
                    if (diff >= 2) {
                        CombatUnit u2;
                        u2.id = 51; u2.name = "Bandit Archer"; u2.count = 3 * diff;
                        u2.maxHp = u2.hp = 4; u2.attack = 3; u2.defense = 1;
                        u2.speed = 4; u2.range = 4; u2.shotsLeft = u2.shots = 8;
                        u2.isPlayer = false;
                        banditUnits.push_back(u2);
                    }
                }
                m_lastCombatEnemyId = 0;
                m_pendingTownCaptureId = 0;
                auto pUnits = makeHeroUnits(hero, m_registry.units(), true);
                enterCombat(hero, pUnits, banditHero, banditUnits);
                return;
            }
            break;
        case WorldObjectType::UnitDwelling:
            if (obj.available > 0) {
                m_pendingObjId = obj.id;
                m_showDwellingPopup = true;
            }
            break;
        case WorldObjectType::QuestGiver:
            if (obj.questState == 0) {
                m_pendingObjId = obj.id;
                m_showQuestPopup = true;
            } else if (obj.questState == 1) {
                // Check if QuestTarget was collected
                for (const auto& other : m_worldObjects) {
                    if (other.id == obj.linkedId && other.collected) {
                        // Quest complete — reward scales with hero level
                        const_cast<WorldObject&>(obj).questState = 2;
                        if (m_heroes.empty()) break;
                        Hero& qHero = m_heroes[m_activeHeroIdx];
                        int goldReward = 300 + qHero.level * 100;
                        // Bonus: rare resource or XP
                        int xpReward = 50 + qHero.level * 20;
                        m_playerResources.add(ResourceType::Gold, goldReward);
                        int oldLvlQ = qHero.level;
                        char qBuf[48];
                        std::snprintf(qBuf, sizeof(qBuf), "+%dg +%dXP Quest!", goldReward, xpReward);
                        pushPickupEffect(obj.pos, qBuf, IM_COL32(255, 215, 50, 255));
                        m_audio.playSound("levelup");
                        if (qHero.addXp(xpReward)) {
                            const HeroClassDef* cls = m_classRegistry.getClass(qHero.classId);
                            if (cls) {
                                std::vector<SkillDef> allSkills(SKILL_DEFS, SKILL_DEFS + SKILL_DEF_COUNT);
                                m_levelUpOffers = LevelUpSystem::generateOffers(
                                    *cls, qHero.skills, qHero.level, allSkills, qHero.faction);
                            }
                            if (m_levelUpOffers.empty())
                                m_levelUpOffers.push_back({SkillID::OFFENSE, false, false, "Learn Offense"});
                            m_pendingLevelUps = qHero.level - oldLvlQ;
                            m_showLevelUpModal = true;
                            { ScriptContext lvCtx; lvCtx.heroId = qHero.id; m_triggers.fire(TriggerType::HeroLevel, lvCtx); }
                        }
                        printf("Quest complete! Rewarded %d gold + %d XP\n", goldReward, xpReward);
                        break;
                    }
                }
            }
            break;
        case WorldObjectType::QuestTarget:
            if (!obj.collected) {
                obj.collected = true;
                pushPickupEffect(obj.pos, "Target reached!", IM_COL32(180, 255, 140, 255));
                for (auto& other : m_worldObjects) {
                    if (other.id == obj.linkedId) {
                        if (other.questState == 1)
                            printf("Quest target reached! Return to quest giver.\n");
                        break;
                    }
                }
            }
            break;
        case WorldObjectType::ForestShrine:
            if (!obj.collected) {
                obj.collected = true;
                char buf[32]; std::snprintf(buf, sizeof(buf), "+%d XP", obj.value);
                pushPickupEffect(obj.pos, buf, IM_COL32(120, 220, 120, 255));
                m_audio.playSound("pickup");
                {
                    int oldLvlFS = hero.level;
                    if (hero.addXp(obj.value)) {
                        const HeroClassDef* cls = m_classRegistry.getClass(hero.classId);
                        if (cls) {
                            std::vector<SkillDef> allSkills(SKILL_DEFS, SKILL_DEFS + SKILL_DEF_COUNT);
                            m_levelUpOffers = LevelUpSystem::generateOffers(
                                *cls, hero.skills, hero.level, allSkills, hero.faction);
                        }
                        if (m_levelUpOffers.empty())
                            m_levelUpOffers.push_back({SkillID::OFFENSE, false, false, "Learn Offense"});
                        m_pendingLevelUps = hero.level - oldLvlFS;
                        m_showLevelUpModal = true;
                        { ScriptContext lvCtx; lvCtx.heroId = hero.id; m_triggers.fire(TriggerType::HeroLevel, lvCtx); }
                    }
                }
            }
            break;
        case WorldObjectType::HighlandRuin:
            if (!obj.collected) {
                obj.collected = true;
                auto cells = HexGrid::range(hero.pos, obj.value);
                for (auto& c : cells) {
                    if (HexTile* t = m_map.getTile(c)) { t->explored = true; t->visible = true; }
                }
                pushPickupEffect(obj.pos, "Revealed!", IM_COL32(200, 180, 120, 255));
            }
            break;
        case WorldObjectType::HolyFountain:
            if (!obj.collected) {
                obj.collected = true;
                hero.mana = hero.maxMana;
                pushPickupEffect(obj.pos, "Mana restored!", IM_COL32(100, 180, 255, 255));
                m_audio.playSound("spell");
            }
            break;
        case WorldObjectType::Oasis:
            if (!obj.collected) {
                obj.collected = true;
                hero.movePool = hero.maxMove;
                pushPickupEffect(obj.pos, "Movement!", IM_COL32(160, 220, 100, 255));
                m_audio.playSound("pickup");
            }
            break;
        case WorldObjectType::Campfire:
            if (!obj.collected) {
                obj.collected = true;
                m_playerResources.add(ResourceType::Gold, obj.value);
                char buf[32]; std::snprintf(buf, sizeof(buf), "+%d Gold", obj.value);
                pushPickupEffect(obj.pos, buf, IM_COL32(255, 215, 0, 255));
                m_audio.playSound("pickup");
            }
            break;
        case WorldObjectType::LavaCrystal:
            if (!obj.collected) {
                obj.collected = true;
                m_playerResources.add(obj.resourceType, obj.value);
                char resBuf2[32]; std::snprintf(resBuf2, sizeof(resBuf2), "+%d %s",
                    obj.value, resourceName(obj.resourceType));
                pushPickupEffect(obj.pos, resBuf2, IM_COL32(200, 80, 80, 255));
                m_audio.playSound("pickup");
            }
            break;
        case WorldObjectType::SwampAltar:
            if (!obj.collected) {
                obj.collected = true;
                bool already = false;
                for (int sid : hero.knownSpells) if (sid == obj.value) { already = true; break; }
                if (!already) hero.knownSpells.push_back(obj.value);
                pushPickupEffect(obj.pos, "Spell learned!", IM_COL32(180, 100, 255, 255));
                m_audio.playSound("spell");
            }
            break;
        }
    }

    // Resource node — claim mine (immediate payout + weekly income)
    if (tile->resourceId != 0) {
        for (auto& r : m_resources) {
            if (r.id == tile->resourceId && r.ownedBy != 1) {
                r.ownedBy = 1;
                m_playerResources.add(r.type, r.amount); // first-capture payout
                m_cachedWeeklyIncome.add(r.type, r.amount); // update income display
                char mineBuf[48];
                std::snprintf(mineBuf, sizeof(mineBuf), "+%d %s/week",
                              r.amount, resourceName(r.type));
                pushPickupEffect(hero.pos, mineBuf, IM_COL32(255, 220, 80, 255));
                m_audio.playSound("buy");
                printf("Claimed mine: +%d %s/week\n", r.amount, resourceName(r.type));
                break;
            }
        }
    }

    // Town entry / capture
    if (tile->townId != 0) {
        for (auto& t : m_towns) {
            if (t.id != tile->townId) continue;
            if (t.ownerId != 1) {
                // Fight the garrison if one exists
                if (!t.garrison.empty()) {
                    // Build garrison CombatUnits as the "enemy"
                    Hero garrisonHero; // dummy hero for the garrison
                    garrisonHero.id     = 0;
                    garrisonHero.name   = t.name + " Garrison";
                    garrisonHero.faction = t.faction;
                    garrisonHero.army   = t.garrison;
                    m_lastCombatEnemyId = 0; // no real enemy hero
                    m_pendingTownCaptureId = t.id;
                    auto pUnits = makeHeroUnits(hero, m_registry.units(), true);
                    auto gUnits = makeHeroUnits(garrisonHero, m_registry.units(), false);
                    enterCombat(hero, pUnits, garrisonHero, gUnits);
                    return;
                }
                // No garrison — capture immediately
                t.ownerId = 1;
                t.garrison.clear();
                printf("Captured town: %s\n", t.name.c_str());
                m_capturedTownName = t.name;
                m_showCapturePopup = true;
                m_hideout.completeMilestone(Milestone::FIRST_TOWN_CAPTURED);
                {
                    ScriptContext tCtx;
                    tCtx.townId = t.id;
                    m_triggers.fire(TriggerType::TownCaptured, tCtx);
                }
            }
            enterTown(&t);
            return;
        }
    }

    // Hero collision — player meets player → unit exchange; player meets enemy → combat
    if (tile->heroId != 0 && tile->heroId != hero.id) {
        // Check allied heroes first
        for (int i = 0; i < static_cast<int>(m_heroes.size()); ++i) {
            if (m_heroes[i].id == tile->heroId && i != m_activeHeroIdx) {
                m_showUnitExchange = true;
                m_exchangeHeroIdx  = i;
                m_exchangeSelSlotA = -1;
                m_exchangeSelSlotB = -1;
                return;
            }
        }
        // Enemy hero
        Hero* enemyPtr = nullptr;
        for (auto& e : m_enemyHeroes)
            if (e.id == tile->heroId) { enemyPtr = &e; break; }
        if (enemyPtr) {
            m_lastCombatEnemyId = enemyPtr->id;
            auto pUnits = makeHeroUnits(hero, m_registry.units(), true);
            auto eUnits = makeHeroUnits(*enemyPtr, m_registry.units(), false);
            enterCombat(hero, pUnits, *enemyPtr, eUnits);
        }
    }
}

void Game::drawHero(const Hero& hero)
{
    // Hero markers (circles + name labels) are drawn in renderWorldOverlay()
    // via ImGui's background DrawList, with animated position for the active hero.
    // This function is reserved for sprite-batch rendering once a tileset exists.
    (void)hero;
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

    // Helper: draw one icon from the atlas centered at (sx,sy) with half-size hs
    const bool hasIcons = m_iconTex.ok();
    ImTextureID iconTex = hasIcons
        ? (ImTextureID)(uintptr_t)m_iconTex.id()
        : (ImTextureID)(uintptr_t)0;

    auto addIcon = [&](int idx, float sx, float sy, float hs) {
        if (!hasIcons) return;
        float col = static_cast<float>(idx % 8);
        float row = static_cast<float>(idx / 8);
        ImVec2 uv0 = { col / 8.0f,          row / 6.0f };
        ImVec2 uv1 = { (col + 1.0f) / 8.0f, (row + 1.0f) / 6.0f };
        dl->AddImage(iconTex, {sx - hs, sy - hs}, {sx + hs, sy + hs}, uv0, uv1);
    };

    // Icon atlas indices
    enum : int {
        ICO_HERO_PLAYER  = 0, ICO_HERO_ENEMY   = 1,
        ICO_TOWN_PLAYER  = 2, ICO_TOWN_ENEMY   = 3, ICO_TOWN_NEUTRAL = 4,
        ICO_SCROLL       = 5, ICO_ARTIFACT     = 6, ICO_XP           = 7,
        ICO_CACHE        = 8, ICO_RES_GOLD     = 9, ICO_RES_IRON     = 10,
        ICO_RES_FAITH    = 11,ICO_RES_BLOOD    = 12,ICO_RES_SAP      = 13,
        ICO_RES_MERCURY  = 14,
        ICO_OBSERVATORY  = 16, ICO_STAT_SHRINE = 17, ICO_BANDIT_CAMP = 18,
        ICO_DWELLING     = 19, ICO_QUEST_GIVER = 20, ICO_QUEST_TARGET = 21,
        ICO_FOREST_SHRINE = 22, ICO_HIGHLAND_RUIN = 23,
        ICO_HOLY_FOUNTAIN = 24, ICO_OASIS         = 25,
        ICO_CAMPFIRE      = 26, ICO_LAVA_CRYSTAL  = 27, ICO_SWAMP_ALTAR = 28,
    };

    // ── Road network ──────────────────────────────────────────────────────────
    // Draw dirt-road paths connecting towns — render before towns/icons so they
    // appear underneath map objects
    if (!m_roadHexes.empty()) {
        for (const auto& rc : m_roadHexes) {
            const HexTile* rt = m_map.getTile(rc);
            if (!rt || !rt->explored) continue;
            float sx, sy;
            project(rc, sx, sy);
            // Base dirt circle
            dl->AddCircleFilled({sx, sy}, 18.0f, IM_COL32(160, 130, 85, 140));
            // Draw line segments to each explored road neighbor for continuity
            for (const auto& nb : HexGrid::neighbors(rc)) {
                if (m_roadHexes.count(nb)) {
                    const HexTile* nt = m_map.getTile(nb);
                    if (!nt || !nt->explored) continue;
                    float nx, ny;
                    project(nb, nx, ny);
                    float mx = (sx + nx) * 0.5f, my = (sy + ny) * 0.5f;
                    dl->AddLine({sx, sy}, {mx, my}, IM_COL32(160, 130, 85, 120), 10.0f);
                }
            }
        }
    }

    // ── Movement range highlight ───────────────────────────────────────────────
    // Draw a soft green overlay on every hex the active hero can reach this turn
    if (!m_heroes.empty() && !m_reachable.empty()) {
        for (const auto& rc : m_reachable) {
            float sx, sy;
            project(rc, sx, sy);
            dl->AddCircleFilled({sx, sy}, 20.0f, IM_COL32(80, 220, 100, 35));
            dl->AddCircle({sx, sy}, 20.0f, IM_COL32(80, 220, 100, 110), 0, 1.2f);
        }
    }

    // ── Towns ─────────────────────────────────────────────────────────────────
    for (const auto& town : m_towns) {
        const HexTile* ttile = m_map.getTile(town.pos);
        if (!m_fogDisabled && ttile && !ttile->visible) continue;

        float sx, sy;
        project(town.pos, sx, sy);

        bool isPlayer = (town.ownerId == 1);
        bool isEnemy  = (town.ownerId > 1);
        int  fid      = std::clamp(static_cast<int>(town.faction), 0, NUM_FACTIONS - 1);

        ImU32 ringCol = isPlayer ? IM_COL32(120, 180, 255, 255)
                      : isEnemy  ? IM_COL32(255,  90,  90, 255)
                                 : IM_COL32(210, 165,  50, 255);
        ImU32 flagCol = isPlayer ? IM_COL32( 80, 140, 255, 230)
                      : isEnemy  ? IM_COL32(220,  60,  60, 230)
                                 : IM_COL32(190, 145,  30, 230);

        ImTextureID townArt = m_townTex[fid].ok()
            ? (ImTextureID)(uintptr_t)m_townTex[fid].id() : nullptr;

        const float CS   = townArt ? 46.0f : 44.0f;   // ~90px total, fits within one hex tile
        const float glow = 12.0f;

        // Outer glow
        dl->AddRectFilled({sx - CS - glow, sy - CS - glow},
                          {sx + CS + glow, sy + CS + glow},
                          (ringCol & 0x00FFFFFFu) | 0x28000000u, 10.0f);

        if (townArt) {
            // ── Faction art image ──────────────────────────────────────────
            dl->AddImageRounded(townArt, {sx - CS, sy - CS}, {sx + CS, sy + CS},
                                {0,0}, {1,1}, IM_COL32(255,255,255,230), 6.0f);
        } else {
            // ── Procedural silhouette fallback ─────────────────────────────
            ImU32 bgCol   = isPlayer ? IM_COL32(12, 22, 55, 240)
                          : isEnemy  ? IM_COL32(55, 12, 12, 240)
                                     : IM_COL32(45, 35, 10, 240);
            ImU32 wallCol = isPlayer ? IM_COL32(70,110,210,255)
                          : isEnemy  ? IM_COL32(210,65, 65, 255)
                                     : IM_COL32(185,150, 45, 255);
            dl->AddRectFilled({sx-CS, sy-CS}, {sx+CS, sy+CS}, bgCol, 5.0f);
            const float TW=13.f, TH=CS*0.85f, KW=CS*0.38f, KH=CS;
            dl->AddRectFilled({sx-CS+5,     sy-TH*.55f},{sx-CS+5+TW, sy+TH*.45f}, wallCol,2.f);
            dl->AddRectFilled({sx+CS-5-TW,  sy-TH*.55f},{sx+CS-5,    sy+TH*.45f}, wallCol,2.f);
            dl->AddRectFilled({sx-KW, sy-KH*.5f},{sx+KW, sy+KH*.5f}, wallCol, 2.f);
            for (float bx=sx-KW+2; bx<sx+KW-4; bx+=11.5f)
                dl->AddRectFilled({bx,sy-KH*.5f-7},{bx+5.5f,sy-KH*.5f}, wallCol);
            dl->AddRectFilled({sx-4.5f,sy+KH*.5f-14},{sx+4.5f,sy+KH*.5f},IM_COL32(8,8,8,220));
        }

        // ── Ownership border + flag pole ──────────────────────────────────
        dl->AddRect({sx - CS, sy - CS}, {sx + CS, sy + CS}, ringCol, 6.0f, 0, 2.5f);

        // Flag pole (top-center)
        float poleX = sx + CS - 10.f, poleY1 = sy - CS - 18.f, poleY2 = sy - CS + 2.f;
        dl->AddLine({poleX, poleY1}, {poleX, poleY2}, IM_COL32(180,160,100,220), 2.0f);
        dl->AddTriangleFilled({poleX, poleY1}, {poleX + 16.f, poleY1 + 6.f},
                              {poleX, poleY1 + 12.f}, flagCol);

        // ── Town name ─────────────────────────────────────────────────────
        float nameW = town.name.size() * 5.0f;
        float nameX = sx - nameW, nameY = sy + CS + 5.0f;
        dl->AddText(ImGui::GetFont(), 14.f, {nameX+1, nameY+1}, IM_COL32(0,0,0,200), town.name.c_str());
        dl->AddText(ImGui::GetFont(), 14.f, {nameX,   nameY},   IM_COL32(210,230,255,255), town.name.c_str());
    }

    // ── World objects ──────────────────────────────────────────────────────────
    for (int oi = 0; oi < static_cast<int>(m_worldObjects.size()); ++oi) {
        const auto& obj = m_worldObjects[oi];
        if (obj.collected) continue;
        const HexTile* otile = m_map.getTile(obj.pos);
        if (!m_fogDisabled && (!otile || !otile->explored)) continue;
        float sx, sy;
        project(obj.pos, sx, sy);
        int ico;
        switch (obj.type) {
        case WorldObjectType::SpellScroll:   ico = ICO_SCROLL;          break;
        case WorldObjectType::ArtifactChest: ico = ICO_ARTIFACT;        break;
        case WorldObjectType::XPShrine:      ico = ICO_XP;              break;
        case WorldObjectType::ResourceCache: ico = ICO_CACHE;           break;
        case WorldObjectType::Observatory:   ico = ICO_OBSERVATORY;     break;
        case WorldObjectType::StatShrine:    ico = ICO_STAT_SHRINE;     break;
        case WorldObjectType::BanditCamp:    ico = ICO_BANDIT_CAMP;     break;
        case WorldObjectType::UnitDwelling:  ico = ICO_DWELLING;        break;
        case WorldObjectType::QuestGiver:    ico = ICO_QUEST_GIVER;     break;
        case WorldObjectType::QuestTarget:   ico = ICO_QUEST_TARGET;    break;
        case WorldObjectType::ForestShrine:  ico = ICO_FOREST_SHRINE;   break;
        case WorldObjectType::HighlandRuin:  ico = ICO_HIGHLAND_RUIN;   break;
        case WorldObjectType::HolyFountain:  ico = ICO_HOLY_FOUNTAIN;   break;
        case WorldObjectType::Oasis:         ico = ICO_OASIS;           break;
        case WorldObjectType::Campfire:      ico = ICO_CAMPFIRE;        break;
        case WorldObjectType::LavaCrystal:   ico = ICO_LAVA_CRYSTAL;    break;
        case WorldObjectType::SwampAltar:    ico = ICO_SWAMP_ALTAR;     break;
        default:                             ico = 15;                   break;
        }
        // Idle glow pulse (each object offset slightly for variety)
        float pulse = 0.5f + 0.5f * sinf(m_mapTime * 2.0f + oi * 1.1f);
        float gR    = 10.0f + pulse * 3.0f;
        ImU32 glow  = IM_COL32(255, 240, 180, static_cast<int>(pulse * 90 + 40));
        addIcon(ico, sx, sy, 22.0f);
        dl->AddCircle({sx, sy}, gR + 8.0f, glow, 0, 1.5f);
    }

    // ── Resource nodes (mines) ────────────────────────────────────────────────
    for (const auto& r : m_resources) {
        const HexTile* rtile = m_map.getTile(r.pos);
        if (!m_fogDisabled && (!rtile || !rtile->explored)) continue;
        float sx, sy;
        project(r.pos, sx, sy);
        int ico;
        switch (r.type) {
        case ResourceType::Gold:         ico = ICO_RES_GOLD;    break;
        case ResourceType::Iron:         ico = ICO_RES_IRON;    break;
        case ResourceType::FaithStones:  ico = ICO_RES_FAITH;   break;
        case ResourceType::BloodEssence: ico = ICO_RES_BLOOD;   break;
        case ResourceType::VerdantSap:   ico = ICO_RES_SAP;     break;
        case ResourceType::Mercury:      ico = ICO_RES_MERCURY; break;
        default:                         ico = 15;               break;
        }
        // Glow backdrop so mine is visible against any terrain
        ImU32 bgGlow = IM_COL32(0, 0, 0, 150);
        dl->AddCircleFilled({sx, sy}, 30.0f, bgGlow);
        addIcon(ico, sx, sy, 28.0f);
        // Ownership ring
        ImU32 ring = r.ownedBy == 1 ? IM_COL32(120, 200, 255, 255)
                   : r.ownedBy >  1 ? IM_COL32(255, 100, 100, 255)
                                    : IM_COL32(255, 210,  60, 200);
        dl->AddCircle({sx, sy}, 30.0f, ring, 0, 2.0f);
        // Resource name + weekly amount always shown below the icon
        const char* resName = resourceName(r.type);
        char label[32];
        std::snprintf(label, sizeof(label), "%s +%d", resName, r.amount);
        float lw = strlen(label) * 5.5f;
        dl->AddRectFilled({sx - lw - 2, sy + 32}, {sx + lw + 2, sy + 44},
                          IM_COL32(0, 0, 0, 170), 3.0f);
        dl->AddText({sx - lw, sy + 33},
                    r.ownedBy == 1 ? IM_COL32(140, 210, 255, 255)
                                   : IM_COL32(255, 230, 120, 255),
                    label);
    }

    // BloodScent: any player hero with this specialty reveals Bloodsworn enemies
    bool playerHasBloodScent = false;
    for (const auto& ph : m_heroes) {
        if (ph.bloodScentSpecialty) { playerHasBloodScent = true; break; }
    }

    // ── Enemy heroes (only if tile is visible, or revealed by BloodScent; GhostWalk heroes are hidden) ─────
    for (const auto& hero : m_enemyHeroes) {
        // GhostWalk: Voidkin Shadow Stalker is invisible on the world map
        if (hero.ghostWalkSpecialty) continue;
        const HexTile* etile = m_map.getTile(hero.pos);
        bool revealedByBloodScent = playerHasBloodScent && hero.faction == FactionId::Bloodsworn;
        if (!m_fogDisabled && (!etile || (!etile->visible && !revealedByBloodScent))) continue;
        float sx, sy;
        project(hero.pos, sx, sy);

        int fac = std::min(static_cast<int>(hero.faction), NUM_FACTIONS - 1);
        auto ait = m_heroMapAnimators.find(hero.id);
        if (ait != m_heroMapAnimators.end() && m_unitTex[fac][0].ok()) {
            float u0, v0, u1, v1;
            ait->second.getUV(u0, v0, u1, v1);
            ImTextureID tex = (ImTextureID)(uintptr_t)m_unitTex[fac][0].id();
            dl->AddImage(tex, {sx - 16, sy - 20}, {sx + 16, sy + 12}, {u0,v0}, {u1,v1});
        } else {
            addIcon(ICO_HERO_ENEMY, sx, sy, 13.0f);
        }
        dl->AddCircle({sx, sy}, 14.0f, IM_COL32(255, 140, 140, 180), 0, 1.5f);
        dl->AddText({sx - (float)hero.name.size() * 3.0f, sy + 15},
                    IM_COL32(255, 160, 160, 200), hero.name.c_str());
        if (hero.isGarrisoned)
            dl->AddText({sx - 10.0f, sy - 30.0f}, IM_COL32(255, 80, 80, 255), "[G]");
    }

    // ── Player heroes ─────────────────────────────────────────────────────────
    for (int i = 0; i < static_cast<int>(m_heroes.size()); ++i) {
        const auto& hero = m_heroes[i];
        float wx, wy;
        if (i == m_activeHeroIdx && m_moveT < 1.0f) {
            wx = m_moveSrcX + (m_moveDstX - m_moveSrcX) * m_moveT;
            wy = m_moveSrcY + (m_moveDstY - m_moveSrcY) * m_moveT;
        } else {
            m_hexRenderer.grid().hexToWorld(hero.pos, wx, wy);
        }
        float sx, sy;
        m_camera.worldToScreen(wx, wy, sx, sy);

        bool  active = (i == m_activeHeroIdx);
        ImU32 ring   = active ? IM_COL32(255, 255, 160, 255) : IM_COL32(200, 200, 80, 200);

        int fac = std::min(static_cast<int>(hero.faction), NUM_FACTIONS - 1);
        auto ait = m_heroMapAnimators.find(hero.id);
        if (ait != m_heroMapAnimators.end() && m_unitTex[fac][0].ok()) {
            float u0, v0, u1, v1;
            ait->second.getUV(u0, v0, u1, v1);
            ImTextureID tex = (ImTextureID)(uintptr_t)m_unitTex[fac][0].id();
            dl->AddImage(tex, {sx - 16, sy - 20}, {sx + 16, sy + 12}, {u0,v0}, {u1,v1});
        } else {
            addIcon(ICO_HERO_PLAYER, sx, sy, 13.0f);
        }
        dl->AddCircle({sx, sy}, 14.0f, ring, 0, active ? 2.0f : 1.2f);
        dl->AddText({sx - (float)hero.name.size() * 3.0f, sy + 15},
                    active ? IM_COL32(255, 230, 100, 220) : IM_COL32(200, 200, 100, 160),
                    hero.name.c_str());
        if (hero.isGarrisoned)
            dl->AddText({sx - 10.0f, sy - 30.0f}, IM_COL32(255, 200, 60, 255), "[G]");
    }

    // ── Pickup effects (floating text) ────────────────────────────────────────
    for (const auto& e : m_pickupEffects) {
        float alpha = std::min(1.0f, e.t);
        if (alpha <= 0.0f) continue;
        float rise = (2.0f - e.t) * 30.0f;
        float sx, sy;
        m_camera.worldToScreen(e.wx, e.wy, sx, sy);
        sy -= rise;
        int   a   = static_cast<int>(alpha * 255);
        ImU32 col = (e.col & 0x00FFFFFFu) | (static_cast<ImU32>(a) << 24);
        dl->AddText({sx - static_cast<float>(e.text.size()) * 3.5f, sy}, col, e.text.c_str());
    }

    // ── Planned path visualization ────────────────────────────────────────────
    if (!m_heroes.empty()) {
        const Hero& activeHero = m_heroes[m_activeHeroIdx];
        if (!activeHero.path.empty() && activeHero.pathStep < static_cast<int>(activeHero.path.size())) {
            // Draw remaining path steps as small dots
            for (int pi = activeHero.pathStep; pi < static_cast<int>(activeHero.path.size()); ++pi) {
                float sx, sy;
                project(activeHero.path[pi], sx, sy);
                float alpha = 1.0f - static_cast<float>(pi - activeHero.pathStep)
                                     / static_cast<float>(activeHero.path.size() - activeHero.pathStep + 1);
                ImU32 dotCol = IM_COL32(255, 230, 80, static_cast<int>(alpha * 180));
                dl->AddCircleFilled({sx, sy}, 3.5f, dotCol);
                // Connector line to previous dot
                if (pi > activeHero.pathStep) {
                    float px, py;
                    project(activeHero.path[pi - 1], px, py);
                    dl->AddLine({px, py}, {sx, sy}, IM_COL32(255, 230, 80, static_cast<int>(alpha * 100)), 1.5f);
                } else {
                    // First dot: connect from hero position
                    float hx, hy;
                    m_hexRenderer.grid().hexToWorld(activeHero.pos, hx, hy);
                    float hsx, hsy;
                    m_camera.worldToScreen(hx, hy, hsx, hsy);
                    dl->AddLine({hsx, hsy}, {sx, sy}, IM_COL32(255, 230, 80, 80), 1.5f);
                }
            }
        }
    }

    // ── Hover tooltip: days to reach or Fight ─────────────────────────────────
    if (m_map.inBounds(m_hovered) && !m_heroes.empty()) {
        const HexTile* ht = m_map.getTile(m_hovered);
        const Hero& activeHero = m_heroes[m_activeHeroIdx];
        if (ht && ht->explored && m_hovered != activeHero.pos) {
            // Check if hovering over an enemy hero
            const Hero* enemyHovered = nullptr;
            if (ht->heroId != 0 && ht->visible) {
                for (const auto& e : m_enemyHeroes)
                    if (e.id == ht->heroId) { enemyHovered = &e; break; }
            }

            bool isFight = (enemyHovered != nullptr);
            if (!isFight && ht->townId != 0)
                for (const auto& t : m_towns)
                    if (t.id == ht->townId && t.ownerId > 1) { isFight = true; break; }

            ImGui::BeginTooltip();
            if (enemyHovered) {
                // Show enemy hero details
                ImGui::TextColored({1.0f, 0.4f, 0.4f, 1.0f}, "%s", enemyHovered->name.c_str());
                ImGui::TextColored({1.0f, 0.3f, 0.3f, 1.0f}, "Fight!");
                ImGui::Separator();
                ImGui::Text("Lvl %d  ATK:%d  DEF:%d", enemyHovered->level,
                            enemyHovered->attack, enemyHovered->defense);
                if (!enemyHovered->army.empty()) {
                    ImGui::Spacing();
                    ImGui::TextDisabled("Army:");
                    const auto& unitDefs = m_registry.units();
                    for (const auto& stack : enemyHovered->army) {
                        if (stack.count <= 0) continue;
                        const char* uname = "?";
                        for (const auto& ud : unitDefs)
                            if (ud.id == stack.defId) { uname = ud.name.c_str(); break; }
                        ImGui::Text("  %-22s x%d", uname, stack.count);
                    }
                }
            } else if (isFight) {
                ImGui::TextColored({1.0f, 0.3f, 0.3f, 1.0f}, "Fight!");
            } else {
                // Movement cost tooltip
                auto costFn = [this, &activeHero](HexCoord c) -> int {
                    const HexTile* t = m_map.getTile(c);
                    if (!t || !activeHero.canEnter(t->terrain)) return 999;
                    return activeHero.moveCost(t->terrain);
                };
                auto path = Pathfinder::find(m_map, activeHero.pos, m_hovered, costFn);
                if (!path.empty()) {
                    int totalCost = 0;
                    for (auto& c : path) {
                        const HexTile* t = m_map.getTile(c);
                        if (t) totalCost += activeHero.moveCost(t->terrain);
                    }
                    int spent     = std::min(totalCost, activeHero.movePool);
                    int remaining = totalCost - spent;
                    int days      = (remaining > 0)
                                    ? (remaining + activeHero.maxMove - 1) / activeHero.maxMove
                                    : 0;
                    if (days == 0) ImGui::Text("Reachable today  (%d MP)", totalCost);
                    else          ImGui::Text("%d day%s  (%d MP)", days, days == 1 ? "" : "s", totalCost);
                } else {
                    const HexTile* bt = m_map.getTile(m_hovered);
                    if (bt && !activeHero.canEnter(bt->terrain))
                        ImGui::TextColored({1.0f, 0.4f, 0.4f, 1.0f}, "Impassable terrain");
                    else
                        ImGui::TextColored({1.0f, 0.4f, 0.4f, 1.0f}, "Unreachable");
                }
                // Also show terrain type
                static const char* kTerrainNames[] = {
                    "Plains","Forest","Highland","Corrupted","Toxic","Sacred",
                    "Industrial","Rocky","Swamp","Water","Volcanic","Barren",
                    "Wasteland","Corrupted Forest","Flesh Zone"
                };
                int tidx = static_cast<int>(ht->terrain);
                if (tidx >= 0 && tidx < 15)
                    ImGui::TextDisabled("%s", kTerrainNames[tidx]);
            }
            ImGui::EndTooltip();
        }
    }

    // ── Minimap ────────────────────────────────────────────────────────────────
    if (m_showMinimap && m_map.radius() > 0) {
        constexpr float MINI_W = 150.0f, MINI_H = 150.0f;
        constexpr float PAD    = 10.0f;
        const ImVec2 disp = ImGui::GetIO().DisplaySize;
        const float mm_left = PAD;
        const float mm_top  = disp.y - MINI_H - PAD;
        const float mm_cx   = mm_left + MINI_W * 0.5f;
        const float mm_cy   = mm_top  + MINI_H * 0.5f;
        const float R       = static_cast<float>(m_map.radius());
        const float scaleX  = MINI_W * 0.5f / R;
        const float scaleY  = MINI_H * 0.5f / R;

        // Terrain color lookup (dim if explored-only, bright if currently visible)
        auto terrainColor = [](Terrain t, bool vis) -> ImU32 {
            uint8_t a = vis ? 255 : 110;
            switch (t) {
            case Terrain::Water:          return IM_COL32( 35,  65, 145, a);
            case Terrain::Plains:         return IM_COL32(100, 165,  72, a);
            case Terrain::Forest:         return IM_COL32( 38,  95,  44, a);
            case Terrain::Highland:       return IM_COL32(115, 125,  75, a);
            case Terrain::Rocky:          return IM_COL32(135, 125, 105, a);
            case Terrain::Swamp:          return IM_COL32( 85, 105,  55, a);
            case Terrain::Sacred:         return IM_COL32(215, 195, 145, a);
            case Terrain::Industrial:     return IM_COL32( 75,  75,  75, a);
            case Terrain::Corrupted:      return IM_COL32( 75,  35,  95, a);
            case Terrain::Toxic:          return IM_COL32(135, 155,  25, a);
            case Terrain::Volcanic:       return IM_COL32(125,  35,  15, a);
            case Terrain::Barren:         return IM_COL32(175, 150,  95, a);
            case Terrain::Wasteland:      return IM_COL32(125,  95,  55, a);
            case Terrain::CorruptedForest:return IM_COL32( 45,  65,  65, a);
            case Terrain::FleshZone:      return IM_COL32(155,  75,  75, a);
            default:                      return IM_COL32( 95,  95,  95, a);
            }
        };

        // Dark backdrop
        dl->AddRectFilled({mm_left-2, mm_top-2},
                          {mm_left+MINI_W+2, mm_top+MINI_H+2},
                          IM_COL32(0, 0, 0, 190));

        // Clip all minimap drawing to its bounds
        dl->PushClipRect({mm_left, mm_top}, {mm_left+MINI_W, mm_top+MINI_H}, true);

        // Draw explored terrain tiles as 2×2 dots
        for (const HexCoord& c : m_map.coords()) {
            const HexTile* t = m_map.getTile(c);
            if (!t || !t->explored) continue;
            float mx = mm_cx + static_cast<float>(c.q) * scaleX;
            float my = mm_cy + (static_cast<float>(c.r) + static_cast<float>(c.q) * 0.5f) * scaleY;
            dl->AddRectFilled({mx-1.f, my-1.f}, {mx+1.f, my+1.f},
                              terrainColor(t->terrain, t->visible));
        }

        // Towns: 4×4 colored square with white outline
        for (const auto& town : m_towns) {
            const HexTile* tt = m_map.getTile(town.pos);
            if (!tt || !tt->explored) continue;
            float mx = mm_cx + static_cast<float>(town.pos.q) * scaleX;
            float my = mm_cy + (static_cast<float>(town.pos.r) + static_cast<float>(town.pos.q) * 0.5f) * scaleY;
            ImU32 col = town.ownerId == 1 ? IM_COL32( 90, 150, 255, 255)
                      : town.ownerId >  1 ? IM_COL32(255,  70,  70, 255)
                                          : IM_COL32(210, 165,  45, 255);
            dl->AddRectFilled({mx-2.f, my-2.f}, {mx+2.f, my+2.f}, col);
            dl->AddRect({mx-2.f, my-2.f}, {mx+2.f, my+2.f}, IM_COL32(255, 255, 255, 220));
        }

        // Resource mines
        for (const auto& r : m_resources) {
            const HexTile* rt = m_map.getTile(r.pos);
            if (!rt || !rt->explored) continue;
            float mx = mm_cx + static_cast<float>(r.pos.q) * scaleX;
            float my = mm_cy + (static_cast<float>(r.pos.r) + static_cast<float>(r.pos.q) * 0.5f) * scaleY;
            ImU32 col = r.ownedBy == 1 ? IM_COL32(80, 220, 80, 200)
                      : r.ownedBy  > 1 ? IM_COL32(220, 80, 80, 200)
                                       : IM_COL32(200, 180, 80, 150);
            dl->AddCircleFilled({mx, my}, 1.5f, col);
        }

        // Enemy heroes (only when tile is visible)
        for (const auto& hero : m_enemyHeroes) {
            const HexTile* ht2 = m_map.getTile(hero.pos);
            if (!ht2 || !ht2->visible) continue;
            float mx = mm_cx + static_cast<float>(hero.pos.q) * scaleX;
            float my = mm_cy + (static_cast<float>(hero.pos.r) + static_cast<float>(hero.pos.q) * 0.5f) * scaleY;
            dl->AddCircleFilled({mx, my}, 2.5f, IM_COL32(255, 60, 60, 255));
        }

        // Player heroes: bright cyan circle
        for (const auto& ph : m_heroes) {
            float mx = mm_cx + static_cast<float>(ph.pos.q) * scaleX;
            float my = mm_cy + (static_cast<float>(ph.pos.r) + static_cast<float>(ph.pos.q) * 0.5f) * scaleY;
            dl->AddCircleFilled({mx, my}, 3.0f, IM_COL32(70, 200, 255, 255));
            dl->AddCircle({mx, my}, 3.5f, IM_COL32(255, 240, 80, 220));
        }

        // Camera viewport rectangle
        {
            constexpr float SQRT3 = 1.7320508f;
            const float hs   = m_hexRenderer.grid().hexSize();
            const float cx_w = m_camera.x();
            const float cy_w = m_camera.y();
            const float z    = m_camera.zoom();
            const float sw   = static_cast<float>(m_width);
            const float sh   = static_cast<float>(m_height);
            // half-extents of visible region in hex-axial units
            const float hx_q  = (sw * 0.5f / z) / (hs * 1.5f);
            const float hy_rq = (sh * 0.5f / z) / (hs * SQRT3);
            // camera center in axial units
            const float cx_q  = cx_w / (hs * 1.5f);
            const float cy_rq = cy_w / (hs * SQRT3);
            float vl = mm_cx + (cx_q - hx_q) * scaleX;
            float vr = mm_cx + (cx_q + hx_q) * scaleX;
            float vt = mm_cy + (cy_rq - hy_rq) * scaleY;
            float vb = mm_cy + (cy_rq + hy_rq) * scaleY;
            dl->AddRect({vl, vt}, {vr, vb}, IM_COL32(255, 255, 255, 210), 0.f, 0, 1.5f);
        }

        dl->PopClipRect();

        // Minimap border
        dl->AddRect({mm_left-2, mm_top-2},
                    {mm_left+MINI_W+2, mm_top+MINI_H+2},
                    IM_COL32(175, 155, 115, 230), 2.0f, 0, 1.5f);
        // "MAP [M]" label above
        dl->AddText({mm_left+2, mm_top-14}, IM_COL32(195, 175, 135, 220), "MAP [M]");
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
            const SkillDef* offerSd = findSkillDef(offer.skillId);
            ImGui::PushID(i);
            if (ImGui::Button(offer.label.c_str(), ImVec2(-1, 36))) {
                // Compute tier before applying offer (for delta calculation)
                int prevTier = 0;
                if (const SkillInstance* existing = hero.skills.getSkill(offer.skillId))
                    prevTier = static_cast<int>(existing->tier);

                LevelUpSystem::applyOffer(offer, hero.skills);

                // Apply immediate passive bonuses for Movement/Vision/Magic skills
                {
                    const SkillDef* sd = findSkillDef(offer.skillId);
                    if (sd) {
                        // v = incremental gain from this skill event
                        // values[] indexed as Basic=0, Advanced=1, Master=2
                        int v;
                        if (offer.isUpgrade) {
                            // prevTier is the index before upgrade (0=Basic, 1=Advanced)
                            v = sd->values[prevTier + 1] - sd->values[prevTier];
                        } else {
                            v = sd->values[0]; // fresh skill at Basic tier
                        }
                        if (sd->effectType == SkillEffectType::MovementBonus) {
                            hero.maxMove += v; hero.movePool = std::min(hero.movePool + v, hero.maxMove);
                        } else if (sd->effectType == SkillEffectType::VisionBonus) {
                            hero.visionRange += v;
                            FogOfWar::updateVision(m_map, hero);
                        } else if (sd->effectType == SkillEffectType::MagicSchoolBonus) {
                            if      (sd->statName == "lightPower")  hero.lightPower  += v;
                            else if (sd->statName == "bloodPower")  hero.bloodPower  += v;
                            else if (sd->statName == "deathPower")  hero.deathPower  += v;
                            else if (sd->statName == "naturePower") hero.naturePower += v;
                            else if (sd->statName == "forgePower")  hero.forgePower  += v;
                            else if (sd->statName == "fleshPower")  hero.fleshPower  += v;
                        }
                    }
                }

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
                    // HP grows 10 per level
                    hero.heroMaxHp += 10;
                    hero.heroHp = hero.heroMaxHp;
                }
                m_levelUpOffers.clear();
                // If more level-ups are queued, generate the next set of offers
                if (m_pendingLevelUps > 1) {
                    m_pendingLevelUps--;
                    const HeroClassDef* ncls = m_classRegistry.getClass(hero.classId);
                    if (ncls) {
                        std::vector<SkillDef> allSkills(SKILL_DEFS, SKILL_DEFS + SKILL_DEF_COUNT);
                        m_levelUpOffers = LevelUpSystem::generateOffers(
                            *ncls, hero.skills, hero.level, allSkills, hero.faction);
                    }
                    if (m_levelUpOffers.empty())
                        m_levelUpOffers.push_back({SkillID::OFFENSE, false, false, "Learn Offense"});
                    // Keep m_showLevelUpModal true so the next modal opens immediately
                } else {
                    m_pendingLevelUps = 0;
                    m_showLevelUpModal = false;
                }
                ImGui::CloseCurrentPopup();
            }
            // Show description below button
            if (offerSd) {
                ImGui::SameLine(0, 4);
                ImGui::TextDisabled("  %s", offerSd->description.c_str());
            }
            if (ImGui::IsItemHovered() && offerSd)
                ImGui::SetTooltip("%s", offerSd->description.c_str());
            ImGui::PopID();
            ImGui::Spacing();
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

    ImGui::SetNextWindowSize(ImVec2(480, 520), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Artifacts  [F7]", &m_showArtifactPanel)) { ImGui::End(); return; }

    static const char* slotNames[] = {
        "Helm","Armor","Weapon","Shield","Ring","Boots","Cloak","Misc"
    };

    // Helper: format non-zero bonus fields into a short string
    auto bonusStr = [](const ArtifactBonus& b) -> std::string {
        std::string s;
        auto app = [&](const char* label, int v) {
            if (v == 0) return;
            if (!s.empty()) s += "  ";
            if (v > 0) s += '+';
            s += std::to_string(v);
            s += ' ';
            s += label;
        };
        app("ATK",    b.attack);
        app("DEF",    b.defense);
        app("SPD",    b.moveBonus);
        app("HP",     b.hpBonus);
        app("Mana",   b.manaBonus);
        app("Light",  b.lightPower);
        app("Blood",  b.bloodPower);
        app("Death",  b.deathPower);
        app("Nature", b.naturePower);
        app("Forge",  b.forgePower);
        app("Flesh",  b.fleshPower);
        app("Vision", b.visionBonus);
        return s.empty() ? "no bonus" : s;
    };

    // Total equipped bonus summary
    ArtifactBonus total = m_artifactRegistry.totalBonus(hero.artifacts);
    std::string totalStr = bonusStr(total);
    if (!totalStr.empty() && totalStr != "no bonus")
        ImGui::TextColored({0.8f,0.8f,0.3f,1.0f}, "Total: %s", totalStr.c_str());
    ImGui::Separator();

    ImGui::Text("Equipped:");
    ImGui::Separator();
    for (int i = 0; i < HeroArtifacts::SLOT_COUNT; ++i) {
        int aid = hero.artifacts.equippedIds[i];
        const ArtifactDef* def = aid ? m_artifactRegistry.getDef(aid) : nullptr;
        ImGui::PushID(i);
        if (def) {
            std::string bs = bonusStr(def->bonus);
            ImGui::Text("%-8s : %-20s  %s", slotNames[i], def->name.c_str(), bs.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Unequip")) {
                hero.artifactInventory.push_back(aid);
                hero.artifacts.unequip(static_cast<ArtifactSlot>(i));
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", def->description.c_str());
        } else {
            ImGui::TextDisabled("%-8s : —", slotNames[i]);
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
            std::string bs = bonusStr(def->bonus);
            ImGui::Text("%-20s  %s", def->name.c_str(), bs.c_str());
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", def->description.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Equip")) {
                auto slot    = def->slot;
                int  slotIdx = static_cast<int>(slot);
                int  old     = hero.artifacts.equippedIds[slotIdx];
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
    const HeroClassDef* cls = m_classRegistry.getClass(hero.classId);
    if (cls) {
        ImGui::TextColored(ImVec4(0.8f, 0.7f, 0.4f, 1.0f), "%s", cls->name.c_str());
        if (!cls->specialtyDesc.empty()) {
            ImGui::TextDisabled("Specialty: %s", cls->specialtyDesc.c_str());
        }
    }
    ImGui::TextDisabled("Level %d  —  XP %d / %d", hero.level, hero.xp, hero.xpToNext);
    {
        float xpFrac = hero.xpToNext > 0 ? static_cast<float>(hero.xp) / hero.xpToNext : 1.0f;
        char xpLabel[32]; std::snprintf(xpLabel, sizeof(xpLabel), "XP %.0f%%", xpFrac * 100.0f);
        ImGui::ProgressBar(xpFrac, ImVec2(-1, 10), xpLabel);
    }
    ImGui::TextDisabled("Battles won: %d", hero.battlesWon);
    if (hero.isGarrisoned)
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "[Garrisoned — +2 DEF in combat]");
    ImGui::Separator();

    ImGui::Text("ATK %d   DEF %d   Vision %d", hero.attack, hero.defense, hero.visionRange);
    ImGui::Text("Mana %d / %d   Move %d / %d",
                hero.mana, hero.maxMana, hero.movePool, hero.maxMove);
    ImGui::Text("HP   %d / %d", hero.heroHp, hero.heroMaxHp);

    // Specialty progression stats
    if (cls) {
        // Veteran / Predator use specialtyAtk as their counter
        bool showGenericAtk = (cls->specialty == SpecialtyType::Veteran ||
                               cls->specialty == SpecialtyType::Predator);
        if (showGenericAtk && hero.specialtyAtk > 0)
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f),
                               "Specialty bonus: +%d ATK", hero.specialtyAtk);
        if (cls->specialty == SpecialtyType::Phylactery && hero.phylacteryUsed)
            ImGui::TextColored(ImVec4(0.7f, 0.5f, 1.0f, 1.0f), "Phylactery consumed");
        if (cls->specialty == SpecialtyType::Elixir && hero.elixirUsed)
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Elixir used this battle");
        if (cls->specialty == SpecialtyType::Recycler && hero.recyclerBonus > 0)
            ImGui::TextColored(ImVec4(0.7f, 0.85f, 0.4f, 1.0f),
                               "Recycler: +%d ATK to all units (%d/5)", hero.recyclerBonus, hero.recyclerBonus);
        if (cls->specialty == SpecialtyType::LivingRune && hero.livingRuneBonus > 0)
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f),
                               "Living Rune: +%d ATK/DEF to hero (%d/5)", hero.livingRuneBonus, hero.livingRuneBonus);
        if (cls->specialty == SpecialtyType::BloodScent)
            ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.4f, 1.0f),
                               "Blood Scent: Bloodsworn heroes always revealed on map");
    }
    ImGui::Spacing();

    {
        bool anyPower = hero.lightPower || hero.bloodPower || hero.deathPower ||
                        hero.naturePower || hero.forgePower || hero.fleshPower;
        ImGui::Text("Casting Power:");
        if (anyPower) {
            if (hero.lightPower)  ImGui::Text("  Light  +%d", hero.lightPower);
            if (hero.bloodPower)  ImGui::Text("  Blood  +%d", hero.bloodPower);
            if (hero.deathPower)  ImGui::Text("  Death  +%d", hero.deathPower);
            if (hero.naturePower) ImGui::Text("  Nature +%d", hero.naturePower);
            if (hero.forgePower)  ImGui::Text("  Forge  +%d", hero.forgePower);
            if (hero.fleshPower)  ImGui::Text("  Flesh  +%d", hero.fleshPower);
        } else {
            ImGui::TextDisabled("  (no school specialisation)");
        }
    }

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
            if (sp) {
                ImGui::Text("  %s  (%d mana)", sp->name, sp->manaCost);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", sp->desc);
            }
        }
    }

    // Artifacts equipped
    {
        ImGui::Spacing();
        ImGui::Text("Artifacts:");
        ImGui::Separator();
        bool anyEquipped = false;
        static const char* kSlotLabel[] = { "Helm","Armor","Wpn","Shld","Ring","Boots","Cloak","Misc" };
        for (int s = 0; s < static_cast<int>(ArtifactSlot::COUNT); ++s) {
            int artId = hero.artifacts.equippedIds[s];
            if (artId == 0) continue;
            const ArtifactDef* art = m_artifactRegistry.getDef(artId);
            if (!art) continue;
            ImGui::Text("  [%-5s] %s", kSlotLabel[s], art->name.c_str());
            if (ImGui::IsItemHovered() && !art->description.empty())
                ImGui::SetTooltip("%s", art->description.c_str());
            anyEquipped = true;
        }
        if (!anyEquipped)
            ImGui::TextDisabled("  — none equipped —");
        if (!hero.artifactInventory.empty()) {
            ImGui::TextDisabled("  Inventory: %zu artifact(s) unequipped",
                                hero.artifactInventory.size());
        }
    }

    if (!hero.army.empty()) {
        ImGui::Spacing();
        ImGui::Text("Army:");
        ImGui::Separator();
        const auto& unitDefs = m_registry.units();
        int totalUnits = 0, totalHp = 0;
        for (const auto& stack : hero.army) {
            if (stack.count <= 0) continue;
            const char* uname = "Unknown";
            int ud_atk = 0, ud_def = 0, ud_hp = 0;
            for (const auto& ud : unitDefs)
                if (ud.id == stack.defId) {
                    uname = ud.name.c_str();
                    ud_atk = ud.attack; ud_def = ud.defense; ud_hp = ud.hp; break;
                }
            ImGui::Text("  %-22s x%-4d  ATK %d  DEF %d  HP %d",
                        uname, stack.count, ud_atk, ud_def, ud_hp * stack.count);
            totalUnits += stack.count;
            totalHp    += ud_hp * stack.count;
        }
        ImGui::Spacing();
        ImGui::TextDisabled("  Total: %d units, %d HP", totalUnits, totalHp);
    }
    ImGui::End();
}

// ── Combat result popup ───────────────────────────────────────────────────────
void Game::renderCombatResultPopup()
{
    const char* title = m_combatResultWon ? "Battle Won!" : "Battle Lost";
    ImGui::OpenPopup(title);
    ImVec2 centre = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(centre, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_Always);

    if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (m_combatResultWon) {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Victory!");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Defeat");
        }
        ImGui::Separator();
        ImGui::Spacing();

        if (m_combatResultKills > 0)
            ImGui::Text("Enemies defeated:  %d", m_combatResultKills);
        if (m_combatResultLost > 0)
            ImGui::Text("Units lost:        %d", m_combatResultLost);
        if (m_combatResultXp > 0)
            ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "XP gained:         +%d", m_combatResultXp);
        if (m_combatResultGold > 0)
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.15f, 1.0f), "Gold looted:       +%d", m_combatResultGold);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::Button("Continue", ImVec2(-1, 28))) {
            m_showCombatResult = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
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
            m_audio.playMusic("worldmap_music");
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
        if (m_finalDefeat) {
            ImGui::TextColored(ImVec4(1.0f, 0.15f, 0.15f, 1.0f), "Total defeat!");
            ImGui::Spacing();
            ImGui::TextWrapped("You have no heroes with armies and no towns. There is no way to continue.");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Your army was defeated!");
        }
        ImGui::Spacing();
        ImGui::TextDisabled("Day %d  Week %d", m_turns.day(), m_turns.week());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float bw = ImGui::GetWindowWidth() - 32.0f;
        if (!m_finalDefeat) {
            if (ImGui::Button("Continue (retreat)", ImVec2(bw * 0.55f, 36))) {
                m_showDefeat  = false;
                m_finalDefeat = false;
                m_audio.playMusic("worldmap_music");
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
        }
        if (ImGui::Button(m_finalDefeat ? "Load Last Save" : "Load Last Save", ImVec2(m_finalDefeat ? bw * 0.6f : -1, 36))) {
            m_showDefeat  = false;
            m_finalDefeat = false;
            loadGame("saves/save" + std::to_string(m_activeSlot) + ".json");
            m_audio.playMusic("worldmap_music");
            ImGui::CloseCurrentPopup();
        }
        if (m_finalDefeat) {
            ImGui::SameLine();
            if (ImGui::Button("Main Menu", ImVec2(-1, 36))) {
                m_showDefeat  = false;
                m_finalDefeat = false;
                m_state = GameState::MainMenu;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }
}

// ── Unit exchange overlay (two player heroes on same tile) ────────────────────
void Game::renderUnitExchange()
{
    if (!m_showUnitExchange) return;
    if (m_heroes.empty() || m_exchangeHeroIdx < 0
        || m_exchangeHeroIdx >= static_cast<int>(m_heroes.size())
        || m_exchangeHeroIdx == m_activeHeroIdx) {
        m_showUnitExchange = false;
        return;
    }
    Hero& heroA = m_heroes[m_activeHeroIdx];
    Hero& heroB = m_heroes[m_exchangeHeroIdx];
    const auto& unitDefs = m_registry.units();

    auto unitName = [&](int defId) -> std::string {
        for (const auto& ud : unitDefs)
            if (ud.id == defId) return ud.name;
        return "Unit";
    };

    ImGuiIO& io = ImGui::GetIO();
    float cx = io.DisplaySize.x * 0.5f, cy = io.DisplaySize.y * 0.5f;
    ImGui::SetNextWindowPos({cx, cy}, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({520, 0}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGuiWindowFlags wf = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
                        | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar;
    if (!ImGui::Begin("##exchange", nullptr, wf)) { ImGui::End(); return; }

    ImGui::TextColored({1.0f, 0.85f, 0.2f, 1.0f}, "Unit Exchange");
    ImGui::SameLine(ImGui::GetWindowWidth() - 60);
    if (ImGui::SmallButton("Close")) {
        m_showUnitExchange = false;
        m_exchangeSelSlotA = m_exchangeSelSlotB = -1;
    }
    ImGui::Separator();

    // Helper: draw one hero's army column
    // Returns true if the user selected a slot
    auto drawCol = [&](const char* label, Hero& h, int& selSlot, bool isA) {
        ImGui::BeginGroup();
        ImGui::TextColored({0.7f,0.85f,1.0f,1.0f}, "%s", label);
        ImGui::Text("%s", h.name.c_str());
        ImGui::Spacing();
        constexpr int MAX_SLOTS = 7;
        for (int i = 0; i < MAX_SLOTS; ++i) {
            ImGui::PushID(isA ? (i * 100) : (i * 100 + 50));
            bool occupied = i < static_cast<int>(h.army.size()) && h.army[i].count > 0;
            bool selected = (selSlot == i);
            if (selected)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.9f, 0.9f));
            else if (!occupied)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f,0.15f,0.15f,0.6f));
            else
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f,0.35f,0.25f,0.8f));

            char lbl[64];
            if (occupied)
                std::snprintf(lbl, sizeof(lbl), "%-16s x%d",
                    unitName(h.army[i].defId).c_str(), h.army[i].count);
            else
                std::snprintf(lbl, sizeof(lbl), "[ empty ]");

            if (ImGui::Button(lbl, {220, 26})) selSlot = (selSlot == i) ? -1 : i;
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
        ImGui::EndGroup();
    };

    drawCol("HERO A", heroA, m_exchangeSelSlotA, true);
    ImGui::SameLine(0, 16);

    // Transfer arrow buttons in the middle
    ImGui::BeginGroup();
    ImGui::Dummy({50, 60});
    bool canAtoB = m_exchangeSelSlotA >= 0
        && m_exchangeSelSlotA < static_cast<int>(heroA.army.size())
        && heroA.army[m_exchangeSelSlotA].count > 0;
    bool canBtoA = m_exchangeSelSlotB >= 0
        && m_exchangeSelSlotB < static_cast<int>(heroB.army.size())
        && heroB.army[m_exchangeSelSlotB].count > 0;

    if (!canAtoB) ImGui::BeginDisabled();
    if (ImGui::Button("A>>B", {50, 26})) {
        auto& srcSlot = heroA.army[m_exchangeSelSlotA];
        bool merged = false;
        for (auto& s : heroB.army)
            if (s.defId == srcSlot.defId) { s.count += srcSlot.count; merged = true; break; }
        bool added = merged;
        if (!merged && heroB.army.size() < 7) { heroB.army.push_back(srcSlot); added = true; }
        if (added)
            heroA.army.erase(heroA.army.begin() + m_exchangeSelSlotA);
        m_exchangeSelSlotA = -1;
    }
    if (!canAtoB) ImGui::EndDisabled();

    ImGui::Spacing();

    if (!canBtoA) ImGui::BeginDisabled();
    if (ImGui::Button("B>>A", {50, 26})) {
        auto& srcSlot = heroB.army[m_exchangeSelSlotB];
        bool merged = false;
        for (auto& s : heroA.army)
            if (s.defId == srcSlot.defId) { s.count += srcSlot.count; merged = true; break; }
        bool added = merged;
        if (!merged && heroA.army.size() < 7) { heroA.army.push_back(srcSlot); added = true; }
        if (added)
            heroB.army.erase(heroB.army.begin() + m_exchangeSelSlotB);
        m_exchangeSelSlotB = -1;
    }
    if (!canBtoA) ImGui::EndDisabled();

    ImGui::Spacing();

    // Split half
    if (canAtoB && heroA.army[m_exchangeSelSlotA].count >= 2) {
        if (ImGui::Button("A/2>B", {50, 26})) {
            auto& src = heroA.army[m_exchangeSelSlotA];
            int half = src.count / 2;
            bool merged = false;
            for (auto& s : heroB.army)
                if (s.defId == src.defId) { s.count += half; merged = true; break; }
            bool added = merged;
            if (!merged && heroB.army.size() < 7) {
                heroB.army.push_back({src.defId, half}); added = true;
            }
            if (added) src.count -= half;
            m_exchangeSelSlotA = -1;
        }
    }
    ImGui::Spacing();
    if (canBtoA && heroB.army[m_exchangeSelSlotB].count >= 2) {
        if (ImGui::Button("B/2>A", {50, 26})) {
            auto& src = heroB.army[m_exchangeSelSlotB];
            int half = src.count / 2;
            bool merged = false;
            for (auto& s : heroA.army)
                if (s.defId == src.defId) { s.count += half; merged = true; break; }
            bool added = merged;
            if (!merged && heroA.army.size() < 7) {
                heroA.army.push_back({src.defId, half}); added = true;
            }
            if (added) src.count -= half;
            m_exchangeSelSlotB = -1;
        }
    }
    ImGui::EndGroup();

    ImGui::SameLine(0, 16);
    drawCol("HERO B", heroB, m_exchangeSelSlotB, false);

    ImGui::End();
}

// ── Dwelling recruit popup ────────────────────────────────────────────────────
void Game::renderDwellingPopup()
{
    if (!m_showDwellingPopup) return;
    WorldObject* obj = nullptr;
    for (auto& o : m_worldObjects)
        if (o.id == m_pendingObjId) { obj = &o; break; }
    if (!obj) { m_showDwellingPopup = false; return; }

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f},
                            ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({380, 0}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGuiWindowFlags wf = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
                        | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar;
    if (!ImGui::Begin("##dwelling", nullptr, wf)) { ImGui::End(); return; }

    int tier = obj->value;
    int costPerUnit = tier * 50;
    FactionId fac = static_cast<FactionId>(obj->faction);

    // Find unit name for display
    const char* unitName = "Units";
    for (const auto& ud : m_registry.units())
        if (ud.faction == fac && ud.tier == tier && ud.path == UpgradePath::None) {
            unitName = ud.name.c_str(); break;
        }

    ImGui::TextColored({0.9f, 0.7f, 0.2f, 1.0f}, "Unit Dwelling — Tier %d  [%s]", tier, unitName);
    ImGui::Separator();
    ImGui::Text("Available: %d units   |   Cost: %d gold each", obj->available, costPerUnit);
    ImGui::Text("Gold on hand: %d", m_playerResources.get(ResourceType::Gold));
    ImGui::Spacing();

    if (m_heroes.empty()) { m_showDwellingPopup = false; ImGui::End(); return; }
    Hero& hero = m_heroes[m_activeHeroIdx];

    int maxAfford = (costPerUnit > 0) ? m_playerResources.get(ResourceType::Gold) / costPerUnit : obj->available;
    int canBuy    = std::min(maxAfford, obj->available);

    // Custom quantity slider
    static int s_dwellingQty = 0;
    if (ImGui::IsWindowAppearing()) s_dwellingQty = canBuy;
    s_dwellingQty = std::clamp(s_dwellingQty, 0, canBuy);

    ImGui::SetNextItemWidth(240.0f);
    ImGui::SliderInt("Quantity", &s_dwellingQty, 0, canBuy);
    ImGui::Text("Total cost: %d gold", s_dwellingQty * costPerUnit);
    ImGui::Spacing();

    auto doBuy = [&](int qty) {
        if (qty <= 0) return;
        int total = qty * costPerUnit;
        m_playerResources.add(ResourceType::Gold, -total);
        for (const auto& ud : m_registry.units()) {
            if (ud.faction == fac && ud.tier == tier && ud.path == UpgradePath::None) {
                bool merged = false;
                for (auto& s : hero.army)
                    if (s.defId == ud.id) { s.count += qty; merged = true; break; }
                if (!merged && hero.army.size() < 7)
                    hero.army.push_back({ud.id, qty});
                break;
            }
        }
        obj->available -= qty;
        char pickBuf[40];
        std::snprintf(pickBuf, sizeof(pickBuf), "+%d %s", qty, unitName);
        pushPickupEffect(obj->pos, pickBuf, IM_COL32(120, 220, 120, 255));
        m_audio.playSound("pickup");
    };

    if (s_dwellingQty > 0) {
        if (ImGui::Button("Buy", {100, 28})) { doBuy(s_dwellingQty); m_showDwellingPopup = false; }
        ImGui::SameLine();
        if (ImGui::Button("Buy All", {100, 28})) { doBuy(canBuy); m_showDwellingPopup = false; }
        ImGui::SameLine();
    }
    if (ImGui::Button("Close", {80, 28}))
        m_showDwellingPopup = false;

    ImGui::End();
}

// ── Stat shrine popup ─────────────────────────────────────────────────────────
void Game::renderStatShrinePopup()
{
    if (!m_showStatShrinePopup) return;
    WorldObject* obj = nullptr;
    for (auto& o : m_worldObjects)
        if (o.id == m_pendingObjId) { obj = &o; break; }
    if (!obj) { m_showStatShrinePopup = false; return; }

    // Stat shrine options — faction-aware bonus rotation (6 options)
    static const char* kStatNames[] = { "Attack", "Defense", "Move", "Mana", "Vision", "Hero HP" };
    const char* statName = kStatNames[obj->value % 6];

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f},
                            ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({340, 0}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGuiWindowFlags wf = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
                        | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar;
    if (!ImGui::Begin("##statshrine", nullptr, wf)) { ImGui::End(); return; }

    ImGui::TextColored({1.0f, 0.5f, 0.1f, 1.0f}, "Stat Shrine");
    ImGui::Separator();
    static const char* kStatAmounts[] = { "+1", "+1", "+2", "+5", "+1", "+10" };
    const char* amt = kStatAmounts[obj->value % 6];
    ImGui::Text("Spend 1000 gold for %s %s?", amt, statName);
    ImGui::Text("Gold: %d   Uses remaining: %d", m_playerResources.get(ResourceType::Gold), obj->questState);
    ImGui::Spacing();

    bool canAfford = m_playerResources.get(ResourceType::Gold) >= 1000;

    if (!canAfford) ImGui::BeginDisabled();
    if (ImGui::Button("Yes", {80, 28})) {
        if (!m_heroes.empty()) {
            Hero& hero = m_heroes[m_activeHeroIdx];
            m_playerResources.add(ResourceType::Gold, -1000);
            switch (obj->value % 6) {
            case 0: hero.attack  += 1; break;
            case 1: hero.defense += 1; break;
            case 2: hero.maxMove += 2; hero.movePool = std::min(hero.movePool + 2, hero.maxMove); break;
            case 3: hero.maxMana += 5; hero.mana = std::min(hero.mana + 5, hero.maxMana); break;
            case 4: hero.visionRange += 1; FogOfWar::updateVision(m_map, hero); break;
            case 5: hero.heroMaxHp += 10; hero.heroHp = std::min(hero.heroHp + 10, hero.heroMaxHp); break;
            }
            obj->questState--;
            printf("StatShrine: %s %s, uses left: %d\n", amt, statName, obj->questState);
        }
        m_showStatShrinePopup = false;
    }
    if (!canAfford) ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("No", {80, 28}))
        m_showStatShrinePopup = false;

    ImGui::End();
}

// ── Pickup effect helper ──────────────────────────────────────────────────────
void Game::pushPickupEffect(HexCoord pos, const char* text, ImU32 col)
{
    float wx, wy;
    m_hexRenderer.grid().hexToWorld(pos, wx, wy);
    m_pickupEffects.push_back({wx, wy, 2.0f, text, col});
}

// ── Quest popup ───────────────────────────────────────────────────────────────
void Game::renderQuestPopup()
{
    if (!m_showQuestPopup) return;
    WorldObject* obj = nullptr;
    for (auto& o : m_worldObjects)
        if (o.id == m_pendingObjId) { obj = &o; break; }
    if (!obj) { m_showQuestPopup = false; return; }

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f},
                            ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({360, 0}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGuiWindowFlags wf = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
                        | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar;
    if (!ImGui::Begin("##quest", nullptr, wf)) { ImGui::End(); return; }

    ImGui::TextColored({1.0f, 0.85f, 0.1f, 1.0f}, "Quest Available!");
    ImGui::Separator();

    // Varied quest descriptions keyed by id
    static const char* kQuestDesc[] = {
        "A hooded traveller speaks in hushed tones: \"There is a relic hidden beyond these hills — I dare not retrieve it myself. Bring it back and I will reward you handsomely.\"",
        "An elder points toward the horizon: \"Something stirs in that forsaken place. Venture forth and confirm our fears. I shall compensate your bravery well.\"",
        "A wounded scout gasps: \"My companions fell near that cursed site. Find what drove them off and return with proof — gold awaits you.\"",
        "A merchant clutches his pack nervously: \"I lost my ledger at a strange landmark east of here. Retrieve it and five-hundred gold pieces are yours.\"",
        "A hermit emerges from shadows: \"The spirits show me a sign at a place I cannot name. You, traveller — find it and return to me. Your effort will not go unrewarded.\"",
        "A garrison captain frowns: \"We've had reports of unusual activity at a location I've marked. Scout it and report back; there's coin in it for you.\"",
        "A scholar waves a parchment: \"Ancient writings speak of an artifact at these coordinates. Recover it for study and I'll pay you fairly.\"",
        "A cloaked figure steps forward: \"Call it fate that brings you here. Travel to the marked spot, survive what you find, and claim your gold.\"",
    };
    int descIdx = static_cast<int>(obj->id) % (int)(sizeof(kQuestDesc) / sizeof(kQuestDesc[0]));
    ImGui::TextWrapped("%s\n\nReward: 500 gold.", kQuestDesc[descIdx]);
    ImGui::Spacing();

    if (ImGui::Button("Accept", {100, 28})) {
        obj->questState = 1;
        m_showQuestPopup = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Decline", {100, 28}))
        m_showQuestPopup = false;

    ImGui::End();
}

// ── Mini-map ──────────────────────────────────────────────────────────────────
// Minimap rendering is handled inside renderWorldOverlay(), toggled with M key.
void Game::renderMinimap() {}
