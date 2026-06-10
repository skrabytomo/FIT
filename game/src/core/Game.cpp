#include "Game.h"
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include <stdio.h>
#include <cmath>
#include <algorithm>
extern "C" {
#include <lua.h>
}

static constexpr float MOVE_SPEED  = 4.0f;
static constexpr const char* SAVE_PATH    = "saves/save0.json";
static constexpr const char* HIDEOUT_PATH = "saves/hideout.db";

// ── Init ──────────────────────────────────────────────────────────────────────
bool Game::init(const std::string& title, int width, int height)
{
    m_width  = width;
    m_height = height;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    m_window = SDL_CreateWindow(title.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!m_window) { fprintf(stderr, "Window: %s\n", SDL_GetError()); return false; }

    m_glCtx = SDL_GL_CreateContext(m_window);
    if (!m_glCtx) { fprintf(stderr, "GL context: %s\n", SDL_GetError()); return false; }

    SDL_GL_SetSwapInterval(1);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_camera.setViewport(width, height);
    m_camera.setPosition(0.0f, 0.0f);

    if (!m_batch.init())           { fprintf(stderr, "SpriteBatch failed\n"); return false; }
    if (!m_hexRenderer.init(40.0f)){ fprintf(stderr, "HexRenderer failed\n"); return false; }
    if (!m_ui.init(width, height)) { fprintf(stderr, "UIRenderer failed\n"); return false; }

    // Building registry
    m_registry.init();

    // Build map
    m_mapSize = MapSize::Small;
    m_map.create(m_mapSize);
    m_map.forEach([](HexTile& t) {
        int q = t.coord.q, r = t.coord.r;
        if ((q + r) % 5 == 0)      t.terrain = Terrain::Forest;
        else if ((q * r) % 7 == 0) t.terrain = Terrain::Water;
        else if (q % 4 == 0)       t.terrain = Terrain::Highland;
        else if (r % 6 == 0)       t.terrain = Terrain::Sacred;
    });

    // Player resources
    m_playerResources.set(ResourceType::Gold, 5000);
    m_playerResources.set(ResourceType::Iron, 20);

    // Create player hero
    Hero hero;
    hero.id       = 1;
    hero.name     = "Player Hero";
    hero.faction  = FactionId::HolyOrder;
    hero.pos      = {0, 0};
    hero.movePool = hero.maxMove;
    m_heroes.push_back(hero);
    m_activeHeroIdx = 0;

    // Place a sample town
    Town town;
    town.id      = 1;
    town.name    = "Sanctuary";
    town.faction = FactionId::HolyOrder;
    town.pos     = {3, -2};
    town.ownerId = 1;
    m_towns.push_back(town);
    if (HexTile* t = m_map.getTile(town.pos)) t->townId = town.id;

    // Fog of war
    FogOfWar::hideAll(m_map);
    FogOfWar::updateVision(m_map, m_heroes[0]);

    // Snap camera to hero
    float hx, hy;
    m_hexRenderer.grid().hexToWorld(m_heroes[0].pos, hx, hy);
    m_camera.setPosition(hx, hy);

    m_moveT  = 1.0f;
    m_state  = GameState::WorldMap;

    // Wire WorldMapHUD callbacks
    m_worldHUD.init(width, height);
    m_worldHUD.onEndTurn = [this]() {
        bool newWeek = m_turns.endTurn(m_towns, m_heroes,
                                       m_playerResources, m_registry);
        if (newWeek) {
            printf("New week %d — income applied\n", m_turns.week());
            ScriptContext ctx; ctx.heroId = 0;
            m_triggers.fire(TriggerType::WeekStart, ctx);
        }
    };
    m_worldHUD.onHeroClicked = [this](int idx) {
        if (idx >= 0 && idx < static_cast<int>(m_heroes.size())) {
            m_activeHeroIdx = idx;
            float hx2, hy2;
            m_hexRenderer.grid().hexToWorld(m_heroes[idx].pos, hx2, hy2);
            m_camera.setPosition(hx2, hy2);
        }
    };

    // Wire TownScreen callbacks
    m_townScreen.init(width, height);
    m_townScreen.onClose = [this]() { exitTown(); };

    // Wire CombatHUD callbacks
    m_combatHUD.init(width, height);
    m_combatHUD.onWait      = [this]() { m_combat.wait(); };
    m_combatHUD.onDefend    = [this]() { m_combat.skipUnit(); };
    m_combatHUD.onEndCombat = [this]() { exitCombat(false); };

    // Open hideout DB (non-fatal if it fails)
    m_hideout.open(HIDEOUT_PATH);

    // Scripting
    if (m_lua.init()) {
        m_triggers.setEngine(&m_lua);

        // Expose turn state to Lua
        m_lua.registerGameFunc("getDay", [](lua_State* L) -> int {
            lua_pushinteger(L, 1); return 1;  // placeholder until binding ptr available
        });

        // Load startup scripts if they exist
        m_lua.execFile("scripts/autoload.lua");
    }

    // ImGui (non-fatal if fails)
    if (initImGui())
        m_editor.init(width, height);

    m_running = true;
    printf("Game initialized: %dx%d\n", width, height);
    return true;
}

// ── Main loop ─────────────────────────────────────────────────────────────────
void Game::run()
{
    Uint64 prev = SDL_GetPerformanceCounter();
    const Uint64 freq = SDL_GetPerformanceFrequency();
    while (m_running) {
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = static_cast<float>(now - prev) / static_cast<float>(freq);
        if (dt > 0.1f) dt = 0.1f;
        prev = now;
        m_input.beginFrame();
        processEvents();
        update(dt);
        render();
    }
}

// ── Events ────────────────────────────────────────────────────────────────────
void Game::processEvents()
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (m_imguiReady) ImGui_ImplSDL2_ProcessEvent(&e);
        m_input.handleEvent(e);

        if (e.type == SDL_QUIT) m_running = false;
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
            if (m_state == GameState::Town)
                exitTown();
            else if (m_state == GameState::Combat)
                exitCombat(false);
            else
                m_running = false;
        }

        if (e.type == SDL_WINDOWEVENT &&
            e.window.event == SDL_WINDOWEVENT_RESIZED) {
            m_width  = e.window.data1;
            m_height = e.window.data2;
            m_camera.setViewport(m_width, m_height);
            m_ui.resize(m_width, m_height);
            m_worldHUD.resize(m_width, m_height);
            m_combatHUD.resize(m_width, m_height);
            glViewport(0, 0, m_width, m_height);
        }
    }
}

// ── Update dispatch ───────────────────────────────────────────────────────────
void Game::update(float dt)
{
    if (m_input.keyDown(SDLK_F5)) saveGame(SAVE_PATH);
    if (m_input.keyDown(SDLK_F9)) loadGame(SAVE_PATH);
    if (m_input.keyDown(SDLK_F2)) {
        if (m_state == GameState::Editor) exitEditor();
        else enterEditor();
    }

    switch (m_state) {
        case GameState::WorldMap: updateWorldMap(dt); break;
        case GameState::Combat:   updateCombat(dt);   break;
        case GameState::Town:     updateTown(dt);     break;
        case GameState::Editor:   updateEditor(dt);   break;
        default: break;
    }
}

// ── Render dispatch ───────────────────────────────────────────────────────────
void Game::render()
{
    glViewport(0, 0, m_width, m_height);
    glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    switch (m_state) {
        case GameState::WorldMap: renderWorldMap(); break;
        case GameState::Combat:   renderCombat();   break;
        case GameState::Town:     renderTown();     break;
        case GameState::Editor:   renderEditor();   break;
        default: break;
    }

    SDL_GL_SwapWindow(m_window);
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

    if (m_input.keyDown(SDLK_SPACE)) {
        bool newWeek = m_turns.endTurn(m_towns, m_heroes,
                                       m_playerResources, m_registry);
        if (newWeek) {
            printf("New week %d — income applied\n", m_turns.week());
            ScriptContext ctx; ctx.heroId = 0;
            m_triggers.fire(TriggerType::WeekStart, ctx);
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
}

// ── Combat update ─────────────────────────────────────────────────────────────
void Game::updateCombat(float dt)
{
    (void)dt;
    const auto& mouse = m_input.mouse();

    if (mouse.leftDown)
        m_combatHUD.onMouseDown(static_cast<float>(mouse.x),
                                static_cast<float>(mouse.y));
    m_combatHUD.onMouseMove(static_cast<float>(mouse.x),
                            static_cast<float>(mouse.y));

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
}

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
}

// ── State transitions ─────────────────────────────────────────────────────────
void Game::enterWorldMap()
{
    m_state = GameState::WorldMap;
    printf("Entered world map\n");
}

void Game::enterCombat(Hero& playerHero,
                       const std::vector<CombatUnit>& playerUnits,
                       const Hero& enemyHero,
                       const std::vector<CombatUnit>& enemyUnits)
{
    m_state = GameState::Combat;
    m_combat.startBattle(playerHero, playerUnits, enemyHero, enemyUnits, false);
    m_combat.setLogCallback([](const std::string& msg) {
        printf("[Combat] %s\n", msg.c_str());
    });
    printf("Entered combat\n");
}

void Game::enterTown(Town* town)
{
    if (!town) return;
    m_state = GameState::Town;
    m_townScreen.open(town, &m_playerResources, &m_registry);
    printf("Entered town: %s\n", town->name.c_str());
}

void Game::exitCombat(bool playerWon)
{
    printf("Combat ended — %s\n", playerWon ? "Victory" : "Defeat/Retreat");
    ScriptContext ctx; ctx.heroId = m_heroes.empty() ? 0 : (int)m_heroes[m_activeHeroIdx].id;
    if (playerWon) {
        m_hideout.addXP(50);
        m_triggers.fire(TriggerType::BattleWon, ctx);
    } else {
        m_triggers.fire(TriggerType::BattleLost, ctx);
    }
    enterWorldMap();
}

void Game::exitTown()
{
    m_townScreen.close();
    enterWorldMap();
}

// ── World map helpers ─────────────────────────────────────────────────────────
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

void Game::checkTileEvents()
{
    if (m_heroes.empty()) return;
    Hero& hero = m_heroes[m_activeHeroIdx];
    const HexTile* tile = m_map.getTile(hero.pos);
    if (!tile) return;

    // Fire Lua tile-enter trigger
    ScriptContext ctx;
    ctx.heroId     = static_cast<int>(hero.id);
    ctx.tileQ      = hero.pos.q;
    ctx.tileR      = hero.pos.r;
    ctx.playerSide = true;
    m_triggers.fireTileEnter(hero.pos, ctx);
    m_triggers.fire(TriggerType::EnterTile, ctx);

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

// ── Save / Load ───────────────────────────────────────────────────────────────
void Game::saveGame(const std::string& path)
{
    // Ensure saves/ directory by writing a marker file
    SDL_RWops* f = SDL_RWFromFile("saves/.keep", "w");
    if (f) SDL_RWclose(f);

    GameSaveData data = SaveLoad::packState(
        m_map, m_heroes, m_towns,
        m_playerResources,
        m_turns.day(), m_turns.week(),
        m_mapSize);

    if (SaveLoad::saveGame(path, data))
        printf("Game saved to %s\n", path.c_str());
    else
        fprintf(stderr, "Save failed: %s\n", path.c_str());
}

bool Game::loadGame(const std::string& path)
{
    GameSaveData data;
    if (!SaveLoad::loadGame(path, data)) {
        fprintf(stderr, "Load failed: %s\n", path.c_str());
        return false;
    }

    m_mapSize = static_cast<MapSize>(data.mapSizeEnum);
    m_map.create(m_mapSize);

    int day = 1, week = 1;
    SaveLoad::unpackState(data, m_map, m_heroes, m_towns,
                          m_playerResources, day, week);

    m_activeHeroIdx = 0;
    if (!m_heroes.empty()) {
        float hx, hy;
        m_hexRenderer.grid().hexToWorld(m_heroes[0].pos, hx, hy);
        m_camera.setPosition(hx, hy);
    }

    // Re-pin town IDs onto map tiles
    for (auto& t : m_towns)
        if (HexTile* tile = m_map.getTile(t.pos)) tile->townId = t.id;

    m_moveT    = 1.0f;
    m_state    = GameState::WorldMap;
    m_selected = {-999,-999};
    m_reachable.clear();

    printf("Game loaded from %s (day %d week %d)\n", path.c_str(), day, week);
    return true;
}

// ── Editor state ──────────────────────────────────────────────────────────────
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

void Game::updateEditor(float dt)
{
    (void)dt;
    // Camera pan/zoom still works in editor
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

    // Update hovered hex
    float wx, wy;
    m_camera.screenToWorld(static_cast<float>(mouse.x),
                           static_cast<float>(mouse.y), wx, wy);
    HexCoord h = m_hexRenderer.grid().worldToHex(wx, wy);
    m_hovered = m_map.inBounds(h) ? h : HexCoord{-999,-999};

    // Left click — only if ImGui didn't capture it
    if (mouse.leftDown && !ImGui::GetIO().WantCaptureMouse) {
        if (m_map.inBounds(m_hovered))
            m_editor.onHexClicked(m_hovered, m_map, m_towns,
                                  m_resources, m_heroStarts);
    }

    // F3 toggles Combat Simulator window
    if (m_input.keyDown(SDLK_F3))
        m_simWindow.setOpen(!m_simWindow.isOpen());
}

void Game::renderEditor()
{
    // Draw world map as backdrop
    m_hexRenderer.render(m_map, m_camera, m_hovered, {-999,-999});

    // Draw hero starts
    for (auto& s : m_heroStarts) {
        float wx, wy;
        m_hexRenderer.grid().hexToWorld(s, wx, wy);
        (void)wx; (void)wy;  // visual placeholder
    }

    // ImGui editor overlay
    beginImGuiFrame();
    m_editor.renderImGui(m_map, m_towns, m_resources, m_heroStarts);
    m_simWindow.render();
    endImGuiFrame();
}

// ── ImGui integration ─────────────────────────────────────────────────────────
bool Game::initImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui::GetStyle().WindowRounding = 4.0f;

    if (!ImGui_ImplSDL2_InitForOpenGL(m_window, m_glCtx)) {
        fprintf(stderr, "ImGui SDL2 backend init failed\n");
        return false;
    }
    if (!ImGui_ImplOpenGL3_Init("#version 330 core")) {
        fprintf(stderr, "ImGui OpenGL3 backend init failed\n");
        return false;
    }
    m_imguiReady = true;
    printf("ImGui %s ready\n", ImGui::GetVersion());
    return true;
}

void Game::shutdownImGui()
{
    if (!m_imguiReady) return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    m_imguiReady = false;
}

void Game::beginImGuiFrame()
{
    if (!m_imguiReady) return;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void Game::endImGuiFrame()
{
    if (!m_imguiReady) return;
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// ── Shutdown ──────────────────────────────────────────────────────────────────
void Game::shutdown()
{
    m_lua.shutdown();
    shutdownImGui();
    m_editor.shutdown();
    m_hideout.close();
    if (m_glCtx) SDL_GL_DeleteContext(m_glCtx);
    if (m_window) SDL_DestroyWindow(m_window);
    SDL_Quit();
    printf("Shutdown\n");
}
