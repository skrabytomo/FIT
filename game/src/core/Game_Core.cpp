#include "Game.h"
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include <stdio.h>
#include <cmath>
extern "C" {
#include <lua.h>
}

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

        m_lua.registerGameFunc("getDay", [](lua_State* L) -> int {
            lua_pushinteger(L, 1); return 1;
        });

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
    if (m_input.keyDown(SDLK_F4)) {
        if (m_state == GameState::Campaign) exitCampaign();
        else enterCampaign();
    }

    switch (m_state) {
        case GameState::WorldMap: updateWorldMap(dt);  break;
        case GameState::Combat:   updateCombat(dt);    break;
        case GameState::Town:     updateTown(dt);      break;
        case GameState::Editor:   updateEditor(dt);    break;
        case GameState::Campaign: updateCampaign(dt);  break;
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
        case GameState::WorldMap: renderWorldMap();  break;
        case GameState::Combat:   renderCombat();    break;
        case GameState::Town:     renderTown();      break;
        case GameState::Editor:   renderEditor();    break;
        case GameState::Campaign: renderCampaign();  break;
        default: break;
    }

    SDL_GL_SwapWindow(m_window);
}

// ── Save / Load ───────────────────────────────────────────────────────────────
void Game::saveGame(const std::string& path)
{
    SDL_RWops* f = SDL_RWFromFile("saves/.keep", "w");
    if (f) SDL_RWclose(f);

    GameSaveData data = SaveLoad::packState(
        m_map, m_heroes, m_towns,
        m_playerResources,
        m_turns.day(), m_turns.week(),
        m_mapSize);

    data.campaign = m_campaign.toSaveState();

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

    for (auto& t : m_towns)
        if (HexTile* tile = m_map.getTile(t.pos)) tile->townId = t.id;

    m_moveT    = 1.0f;
    m_state    = GameState::WorldMap;
    m_selected = {-999,-999};
    m_reachable.clear();

    if (data.campaign.active) {
        m_campaign.init();
        m_campaign.fromSaveState(data.campaign);
        m_state = GameState::Campaign;
    }

    printf("Game loaded from %s (day %d week %d)\n", path.c_str(), day, week);
    return true;
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
