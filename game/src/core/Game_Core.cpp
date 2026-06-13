#include "Game.h"
#include "../magic/SpellRegistry.h"
#include "../hero/SkillRegistry.h"
#include "../hero/LevelUpSystem.h"
#include "../world/WorldGen.h"
#include "../world/HexGrid.h"
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include <nlohmann/json.hpp>
#include <stdio.h>
#include <fstream>
#include <algorithm>
#include <cmath>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

static constexpr const char* HIDEOUT_PATH = "saves/hideout.db";

static std::string slotPath(int slot)
{
    return "saves/save" + std::to_string(slot) + ".json";
}

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
    m_iconTex.load("assets/icons.png", true, false); // flipV=false: ImGui uses top-left UV origin

    // Faction sprite atlases (optional — falls back to circles if missing)
    for (int i = 0; i < NUM_FACTIONS; ++i) {
        char path[64];
        std::snprintf(path, sizeof(path), "assets/sprites/faction_%d.png", i);
        m_spriteAtlas[i].load(path, false, false); // pixel-art=false, flipV=false
    }

    // SDL cursors
    m_cursorArrow = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    m_cursorFight = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);

    // Building registry
    m_registry.init();

    // Hero class registry
    m_classRegistry.init();

    // Artifact registry
    m_artifactRegistry.init();

    startNewGame();
    m_state = GameState::MainMenu;

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
    m_combatHUD.onSpells    = [this]() { m_showSpellPanel = !m_showSpellPanel; };

    // Open hideout DB (non-fatal if it fails)
    m_hideout.open(HIDEOUT_PATH);

    // Scripting
    if (m_lua.init()) {
        m_triggers.setEngine(&m_lua);
        bindLuaAPI();
        m_lua.execFile("scripts/autoload.lua");
    }

    // ImGui (non-fatal if fails)
    if (initImGui())
        m_editor.init(width, height);

    // Audio
    if (m_audio.init()) {
        m_audio.loadWav("click",          "assets/sounds/click.wav");
        m_audio.loadWav("pickup",         "assets/sounds/pickup.wav");
        m_audio.loadWav("levelup",        "assets/sounds/levelup.wav");
        m_audio.loadWav("hit",            "assets/sounds/hit.wav");
        m_audio.loadWav("spell",          "assets/sounds/spell.wav");
        m_audio.loadWav("buy",            "assets/sounds/buy.wav");
        m_audio.loadWav("worldmap_music", "assets/sounds/worldmap_music.wav");
        m_audio.loadWav("combat_music",   "assets/sounds/combat_music.wav");
        m_audio.loadWav("town_music",     "assets/sounds/town_music.wav");
        for (int fi = 0; fi < 9; ++fi) {
            char key[32], path[64];
            std::snprintf(key,  sizeof(key),  "faction_music_%d", fi);
            std::snprintf(path, sizeof(path), "assets/sounds/faction_music_%d.wav", fi);
            m_audio.loadWav(key, path);
        }
        m_audio.playMusic("worldmap_music");
    }

    loadSettings();   // apply persisted volume / fullscreen settings

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
            else if (m_state == GameState::WorldMap || m_state == GameState::Campaign)
                m_showPauseMenu = !m_showPauseMenu;
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
    m_audio.update();
    if (m_input.keyDown(SDLK_F5)) saveGame(slotPath(m_activeSlot));
    if (m_input.keyDown(SDLK_F9)) loadGame(slotPath(m_activeSlot));
    if (m_input.keyDown(SDLK_F2)) {
        if (m_state == GameState::Editor) exitEditor();
        else enterEditor();
    }
    if (m_input.keyDown(SDLK_F4)) {
        if (m_state == GameState::Campaign) exitCampaign();
        else enterCampaign();
    }

    switch (m_state) {
        case GameState::MainMenu: updateMainMenu(dt);  break;
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
    glClearColor(0.04f, 0.03f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    switch (m_state) {
        case GameState::MainMenu: renderMainMenu();  break;
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
        m_map, m_heroes, m_enemyHeroes,
        m_towns, m_worldObjects, m_resources, m_nextObjId,
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
    SaveLoad::unpackState(data, m_map, m_heroes, m_enemyHeroes,
                          m_towns, m_worldObjects, m_resources, m_nextObjId,
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

// ── New game (reset + world gen) ──────────────────────────────────────────────
void Game::startNewGame()
{
    // Clear all runtime state
    m_heroes.clear();
    m_enemyHeroes.clear();
    m_towns.clear();
    m_resources.clear();
    m_worldObjects.clear();
    m_heroMapAnimators.clear();
    m_pickupEffects.clear();
    m_reachable.clear();
    m_activeHeroIdx   = 0;
    m_moveT           = 1.0f;
    m_selected        = {-999, -999};
    m_hovered         = {-999, -999};
    m_nextObjId       = 1;
    m_turns           = TurnManager{};
    m_playerResources = Resources{};
    m_showVictory     = false;
    m_showDefeat      = false;
    m_showCapturePopup = false;
    m_showTownLostPopup = false;

    // Generate world procedurally using selected settings
    static constexpr MapSize kMapSizes[] = {
        MapSize::Small, MapSize::Medium, MapSize::Large, MapSize::XLarge
    };
    m_mapSize = kMapSizes[std::clamp(m_newGameMapSize, 0, 3)];
    m_map.create(m_mapSize);

    WorldGenParams wgp;
    wgp.seed        = static_cast<uint32_t>(SDL_GetTicks()) ^ 0x5A5A5A5Au;
    wgp.size        = m_mapSize;
    wgp.playerCount = 4;
    wgp.waterRatio  = 0.18f;
    auto wgResult   = WorldGen::generate(m_map, wgp);

    m_resources = std::move(wgResult.resources);
    m_nextObjId = static_cast<uint32_t>(m_resources.size()) + 1;

    m_playerResources.set(ResourceType::Gold, 5000);
    m_playerResources.set(ResourceType::Iron, 20);

    static constexpr FactionId kFactions[] = {
        FactionId::HolyOrder, FactionId::CrimsonWardens, FactionId::Thornkin,
        FactionId::EternalEmpire, FactionId::Bloodsworn, FactionId::Voidkin,
        FactionId::IronAssembly, FactionId::Amalgamate, FactionId::Convergence
    };
    static constexpr int kFactionStartSpell[] = {
        SPL::BLESS, SPL::BLOOD_FRENZY, SPL::ENTANGLE,
        SPL::CURSE, SPL::BLOOD_FRENZY, SPL::ENTANGLE,
        SPL::REINFORCE, SPL::MEND_FLESH, SPL::BLESS
    };
    static const char* kFactionHeroNames[] = {
        "Alara", "Dren", "Korvas", "Mira", "Seld",
        "Thayne", "Vex", "Lyra", "Cael"
    };
    int fi = std::clamp(m_newGameFaction, 0, 8);

    Hero hero;
    hero.id       = 1;
    hero.name     = kFactionHeroNames[fi];
    hero.faction  = kFactions[fi];
    hero.pos      = wgResult.startPositions.empty() ? HexCoord{0,0}
                                                    : wgResult.startPositions[0];
    hero.movePool = hero.maxMove;
    hero.lightPower = (fi == 0 || fi == 8) ? 3 : 0;
    hero.knownSpells = { kFactionStartSpell[fi] };
    m_heroes.push_back(hero);
    if (HexTile* ht = m_map.getTile(hero.pos)) ht->heroId = hero.id;

    auto giveStartingArmy = [&](Hero& h) {
        for (int tier : {1, 2}) {
            for (const auto& ud : m_registry.units()) {
                if (ud.faction == h.faction && ud.tier == tier
                    && ud.path == UpgradePath::None) {
                    h.army.push_back({ud.id, (tier == 1) ? 20 : 8});
                    break;
                }
            }
        }
    };
    giveStartingArmy(m_heroes[0]);

    static const char* kEnemyNames[] = {
        "Dark Warlord", "Blood Raider", "Thornkin Shaman", "Void Stalker"
    };
    for (int i = 1; i < static_cast<int>(wgResult.startPositions.size()) && i <= 3; ++i) {
        FactionId ef = (i < static_cast<int>(wgResult.towns.size()))
                       ? wgResult.towns[i].faction : FactionId::EternalEmpire;
        Hero eHero;
        eHero.id       = 99u + static_cast<uint32_t>(i);
        eHero.name     = kEnemyNames[i - 1];
        eHero.faction  = ef;
        eHero.pos      = wgResult.startPositions[i];
        eHero.movePool = eHero.maxMove;
        giveStartingArmy(eHero);
        m_enemyHeroes.push_back(eHero);
        if (HexTile* ht = m_map.getTile(eHero.pos)) ht->heroId = eHero.id;
    }

    for (int i = 0; i < static_cast<int>(wgResult.towns.size()); ++i) {
        Town& wt = wgResult.towns[i];
        if (i == 0) {
            wt.ownerId = 1;
            wt.builtBuildings.push_back(BID::MAGE_GUILD);
        } else {
            wt.ownerId = 0;
            for (const auto& ud : m_registry.units()) {
                if (ud.faction == wt.faction && ud.tier == 1
                    && ud.path == UpgradePath::None) {
                    wt.garrison.push_back({ud.id, 15});
                    break;
                }
            }
        }
        if (HexTile* ht = m_map.getTile(wt.pos)) ht->townId = wt.id;
        m_towns.push_back(wt);
    }

    m_worldObjects.clear();
    {
        uint32_t rng = wgp.seed ^ 0xF00DBABE;
        auto lcg = [&]() { return (rng = rng * 1664525u + 1013904223u); };

        auto allCoords = m_map.coords();
        for (size_t ci = allCoords.size() - 1; ci > 0; --ci)
            std::swap(allCoords[ci], allCoords[lcg() % (ci + 1)]);

        auto pickTile = [&]() -> HexCoord {
            for (auto& c : allCoords) {
                const HexTile* t = m_map.getTile(c);
                if (!t || t->terrain == Terrain::Water) continue;
                if (t->heroId || t->townId || t->resourceId) continue;
                bool used = false;
                for (auto& o : m_worldObjects) if (o.pos == c) { used = true; break; }
                if (!used) return c;
            }
            return {0, 0};
        };

        static const int kScrollSpells[] = {
            SPL::SMITE, SPL::REGROWTH, SPL::CURSE, SPL::BLESS, SPL::CALL_LIGHTNING
        };
        for (int s = 0; s < 4; ++s) {
            HexCoord p = pickTile();
            m_worldObjects.push_back({m_nextObjId++, WorldObjectType::SpellScroll, p,
                kScrollSpells[lcg() % 5], ResourceType::Gold, false});
        }
        for (int a = 0; a < 3; ++a) {
            HexCoord p = pickTile();
            m_worldObjects.push_back({m_nextObjId++, WorldObjectType::ArtifactChest, p,
                1 + static_cast<int>(lcg() % 8), ResourceType::Gold, false});
        }
        for (int x = 0; x < 3; ++x) {
            HexCoord p = pickTile();
            m_worldObjects.push_back({m_nextObjId++, WorldObjectType::XPShrine, p,
                50 + static_cast<int>(lcg() % 80), ResourceType::Gold, false});
        }
        for (int rc = 0; rc < 3; ++rc) {
            HexCoord p = pickTile();
            bool isGold = (lcg() & 1);
            ResourceType rtype = isGold ? ResourceType::Gold : ResourceType::Iron;
            int rval = isGold ? 300 + static_cast<int>(lcg() % 400)
                              : 5   + static_cast<int>(lcg() % 10);
            m_worldObjects.push_back({m_nextObjId++, WorldObjectType::ResourceCache, p, rval, rtype, false});
        }
    }

    for (auto& wo : wgResult.worldObjects) m_worldObjects.push_back(wo);
    for (const auto& wo : m_worldObjects)
        if (wo.id >= m_nextObjId) m_nextObjId = wo.id + 1;

    FogOfWar::hideAll(m_map);
    FogOfWar::updateVision(m_map, m_heroes[0]);

    float hx, hy;
    m_hexRenderer.grid().hexToWorld(m_heroes[0].pos, hx, hy);
    m_camera.setPosition(hx, hy);
}

// ── Settings persistence ──────────────────────────────────────────────────────
void Game::saveSettings()
{
    nlohmann::json j;
    j["sfxVol"]    = m_settingsSfxVol;
    j["musVol"]    = m_settingsMasVol;
    j["fullscreen"] = m_settingsFullscreen;
    std::ofstream f("settings.json");
    if (f) f << j.dump(2);
}

void Game::loadSettings()
{
    std::ifstream f("settings.json");
    if (!f) return;
    try {
        nlohmann::json j = nlohmann::json::parse(f);
        m_settingsSfxVol     = j.value("sfxVol",     0.7f);
        m_settingsMasVol     = j.value("musVol",      0.35f);
        m_settingsFullscreen = j.value("fullscreen",  false);
        m_audio.setSfxVolume(m_settingsSfxVol);
        m_audio.setMusicVolume(m_settingsMasVol);
        if (m_settingsFullscreen)
            SDL_SetWindowFullscreen(m_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    } catch (...) {}
}

// ── ImGui integration ─────────────────────────────────────────────────────────
bool Game::initImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // ── Medieval dark-stone / gold / crimson theme ────────────────────────────────────────
    ImGuiStyle& sty = ImGui::GetStyle();
    sty.WindowRounding    = 2.0f;
    sty.ChildRounding     = 2.0f;
    sty.PopupRounding     = 2.0f;
    sty.FrameRounding     = 2.0f;
    sty.ScrollbarRounding = 2.0f;
    sty.GrabRounding      = 2.0f;
    sty.TabRounding       = 2.0f;
    sty.WindowBorderSize  = 1.0f;
    sty.FrameBorderSize   = 0.0f;
    sty.FramePadding      = {6.0f, 4.0f};
    sty.ItemSpacing       = {8.0f, 6.0f};
    sty.WindowPadding     = {10.0f, 10.0f};
    sty.ScrollbarSize     = 12.0f;

    ImVec4* C = sty.Colors;
    C[ImGuiCol_Text]                  = {0.92f, 0.86f, 0.70f, 1.00f}; // parchment
    C[ImGuiCol_TextDisabled]          = {0.52f, 0.46f, 0.34f, 1.00f};
    C[ImGuiCol_WindowBg]              = {0.10f, 0.08f, 0.06f, 0.96f}; // dark stone
    C[ImGuiCol_ChildBg]               = {0.08f, 0.06f, 0.05f, 0.90f};
    C[ImGuiCol_PopupBg]               = {0.10f, 0.08f, 0.06f, 0.97f};
    C[ImGuiCol_Border]                = {0.42f, 0.32f, 0.10f, 0.72f}; // dark gold
    C[ImGuiCol_BorderShadow]          = {0.00f, 0.00f, 0.00f, 0.50f};
    C[ImGuiCol_FrameBg]               = {0.18f, 0.14f, 0.10f, 0.90f}; // dark iron
    C[ImGuiCol_FrameBgHovered]        = {0.28f, 0.22f, 0.12f, 0.90f};
    C[ImGuiCol_FrameBgActive]         = {0.38f, 0.28f, 0.12f, 0.90f};
    C[ImGuiCol_TitleBg]               = {0.20f, 0.08f, 0.06f, 1.00f}; // deep burgundy
    C[ImGuiCol_TitleBgActive]         = {0.38f, 0.14f, 0.08f, 1.00f};
    C[ImGuiCol_TitleBgCollapsed]      = {0.12f, 0.05f, 0.04f, 1.00f};
    C[ImGuiCol_MenuBarBg]             = {0.14f, 0.11f, 0.08f, 1.00f};
    C[ImGuiCol_ScrollbarBg]           = {0.06f, 0.05f, 0.04f, 0.90f};
    C[ImGuiCol_ScrollbarGrab]         = {0.38f, 0.28f, 0.10f, 0.80f};
    C[ImGuiCol_ScrollbarGrabHovered]  = {0.52f, 0.40f, 0.14f, 0.90f};
    C[ImGuiCol_ScrollbarGrabActive]   = {0.65f, 0.50f, 0.18f, 1.00f};
    C[ImGuiCol_CheckMark]             = {0.85f, 0.65f, 0.20f, 1.00f}; // gold
    C[ImGuiCol_SliderGrab]            = {0.58f, 0.44f, 0.14f, 0.90f};
    C[ImGuiCol_SliderGrabActive]      = {0.78f, 0.58f, 0.18f, 1.00f};
    C[ImGuiCol_Button]                = {0.22f, 0.18f, 0.12f, 0.90f}; // dark iron
    C[ImGuiCol_ButtonHovered]         = {0.48f, 0.35f, 0.12f, 1.00f}; // burnished bronze
    C[ImGuiCol_ButtonActive]          = {0.68f, 0.50f, 0.16f, 1.00f}; // bright gold
    C[ImGuiCol_Header]                = {0.38f, 0.13f, 0.08f, 0.85f}; // deep crimson
    C[ImGuiCol_HeaderHovered]         = {0.52f, 0.19f, 0.10f, 0.90f};
    C[ImGuiCol_HeaderActive]          = {0.64f, 0.24f, 0.12f, 1.00f};
    C[ImGuiCol_Separator]             = {0.38f, 0.28f, 0.08f, 0.80f};
    C[ImGuiCol_SeparatorHovered]      = {0.55f, 0.40f, 0.12f, 0.90f};
    C[ImGuiCol_SeparatorActive]       = {0.70f, 0.52f, 0.15f, 1.00f};
    C[ImGuiCol_ResizeGrip]            = {0.35f, 0.25f, 0.08f, 0.40f};
    C[ImGuiCol_ResizeGripHovered]     = {0.52f, 0.38f, 0.12f, 0.70f};
    C[ImGuiCol_ResizeGripActive]      = {0.70f, 0.52f, 0.16f, 0.90f};
    C[ImGuiCol_Tab]                   = {0.18f, 0.14f, 0.10f, 0.85f};
    C[ImGuiCol_TabHovered]            = {0.48f, 0.35f, 0.12f, 0.90f};
    C[ImGuiCol_TabActive]             = {0.36f, 0.25f, 0.10f, 1.00f};
    C[ImGuiCol_TabUnfocused]          = {0.14f, 0.10f, 0.07f, 0.85f};
    C[ImGuiCol_TabUnfocusedActive]    = {0.28f, 0.20f, 0.10f, 1.00f};
    C[ImGuiCol_PlotLines]             = {0.80f, 0.60f, 0.20f, 1.00f};
    C[ImGuiCol_PlotLinesHovered]      = {1.00f, 0.80f, 0.30f, 1.00f};
    C[ImGuiCol_PlotHistogram]         = {0.65f, 0.48f, 0.15f, 1.00f};
    C[ImGuiCol_PlotHistogramHovered]  = {0.85f, 0.65f, 0.20f, 1.00f};
    C[ImGuiCol_TableHeaderBg]         = {0.22f, 0.10f, 0.06f, 1.00f};
    C[ImGuiCol_TableBorderStrong]     = {0.38f, 0.28f, 0.10f, 1.00f};
    C[ImGuiCol_TableBorderLight]      = {0.28f, 0.20f, 0.08f, 0.70f};
    C[ImGuiCol_TableRowBg]            = {0.00f, 0.00f, 0.00f, 0.00f};
    C[ImGuiCol_TableRowBgAlt]         = {0.12f, 0.10f, 0.07f, 0.30f};
    C[ImGuiCol_TextSelectedBg]        = {0.55f, 0.40f, 0.10f, 0.40f};
    C[ImGuiCol_DragDropTarget]        = {0.85f, 0.65f, 0.20f, 0.90f};
    C[ImGuiCol_NavHighlight]          = {0.80f, 0.60f, 0.20f, 0.90f};
    C[ImGuiCol_NavWindowingHighlight] = {0.85f, 0.65f, 0.20f, 0.70f};
    C[ImGuiCol_NavWindowingDimBg]     = {0.00f, 0.00f, 0.00f, 0.50f};
    C[ImGuiCol_ModalWindowDimBg]      = {0.00f, 0.00f, 0.00f, 0.72f};

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

// ── Lua scripting API ─────────────────────────────────────────────────────────
void Game::luaAddSpell(int spellId)
{
    if (m_heroes.empty()) return;
    Hero& hero = m_heroes[m_activeHeroIdx];
    for (int s : hero.knownSpells) if (s == spellId) return;
    hero.knownSpells.push_back(spellId);
}

void Game::luaAddXP(int amount)
{
    if (m_heroes.empty()) return;
    Hero& hero = m_heroes[m_activeHeroIdx];
    if (hero.addXp(amount)) {
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

void Game::bindLuaAPI()
{
    lua_State* L = m_lua.state();
    if (!L) return;

    // Store this in registry so non-capturing callbacks can reach Game state
    lua_pushlightuserdata(L, static_cast<void*>(this));
    lua_setfield(L, LUA_REGISTRYINDEX, "Game");

    m_lua.registerGameFunc("getDay", [](lua_State* L) -> int {
        lua_getfield(L, LUA_REGISTRYINDEX, "Game");
        auto* g = static_cast<Game*>(lua_touserdata(L, -1)); lua_pop(L, 1);
        lua_pushinteger(L, g ? g->luaGetDay() : 0);
        return 1;
    });

    m_lua.registerGameFunc("getWeek", [](lua_State* L) -> int {
        lua_getfield(L, LUA_REGISTRYINDEX, "Game");
        auto* g = static_cast<Game*>(lua_touserdata(L, -1)); lua_pop(L, 1);
        lua_pushinteger(L, g ? g->luaGetWeek() : 0);
        return 1;
    });

    m_lua.registerGameFunc("getGold", [](lua_State* L) -> int {
        lua_getfield(L, LUA_REGISTRYINDEX, "Game");
        auto* g = static_cast<Game*>(lua_touserdata(L, -1)); lua_pop(L, 1);
        lua_pushinteger(L, g ? g->luaGetGold() : 0);
        return 1;
    });

    m_lua.registerGameFunc("getHeroLevel", [](lua_State* L) -> int {
        lua_getfield(L, LUA_REGISTRYINDEX, "Game");
        auto* g = static_cast<Game*>(lua_touserdata(L, -1)); lua_pop(L, 1);
        lua_pushinteger(L, g ? g->luaGetHeroLevel() : 1);
        return 1;
    });

    m_lua.registerGameFunc("addGold", [](lua_State* L) -> int {
        int n = static_cast<int>(luaL_checkinteger(L, 1));
        lua_getfield(L, LUA_REGISTRYINDEX, "Game");
        auto* g = static_cast<Game*>(lua_touserdata(L, -1)); lua_pop(L, 1);
        if (g) g->luaAddGold(n);
        return 0;
    });

    m_lua.registerGameFunc("addSpell", [](lua_State* L) -> int {
        int id = static_cast<int>(luaL_checkinteger(L, 1));
        lua_getfield(L, LUA_REGISTRYINDEX, "Game");
        auto* g = static_cast<Game*>(lua_touserdata(L, -1)); lua_pop(L, 1);
        if (g) g->luaAddSpell(id);
        return 0;
    });

    m_lua.registerGameFunc("addXP", [](lua_State* L) -> int {
        int n = static_cast<int>(luaL_checkinteger(L, 1));
        lua_getfield(L, LUA_REGISTRYINDEX, "Game");
        auto* g = static_cast<Game*>(lua_touserdata(L, -1)); lua_pop(L, 1);
        if (g) g->luaAddXP(n);
        return 0;
    });
}

// ── Shutdown ──────────────────────────────────────────────────────────────────
void Game::shutdown()
{
    SDL_FreeCursor(m_cursorArrow);
    SDL_FreeCursor(m_cursorFight);
    m_audio.shutdown();
    m_lua.shutdown();
    shutdownImGui();
    m_editor.shutdown();
    m_hideout.close();
    if (m_glCtx) SDL_GL_DeleteContext(m_glCtx);
    if (m_window) SDL_DestroyWindow(m_window);
    SDL_Quit();
    printf("Shutdown\n");
}
