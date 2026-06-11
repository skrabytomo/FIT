#pragma once
#include <SDL2/SDL.h>
#include <string>
#include <vector>

#include "GameState.h"
#include "InputState.h"
#include "TurnManager.h"
#include "../renderer/SpriteBatch.h"
#include "../renderer/Camera2D.h"
#include "../world/HexMap.h"
#include "../world/HexMapRenderer.h"
#include "../world/FogOfWar.h"
#include "../hero/Hero.h"
#include "../ai/Pathfinder.h"
#include "../town/Town.h"
#include "../town/BuildingRegistry.h"
#include "../combat/CombatEngine.h"
#include "../data/Resources.h"
#include "../data/ResourceNode.h"
#include "../data/SaveLoad.h"
#include "../data/MapFormat.h"
#include "../ui/UIRenderer.h"
#include "../ui/WorldMapHUD.h"
#include "../ui/CombatHUD.h"
#include "../ui/TownScreen.h"
#include "../meta/HideoutDB.h"
#include "../scripting/LuaEngine.h"
#include "../scripting/TriggerSystem.h"
#include "../editor/MapEditor.h"
#include "../editor/SimulatorWindow.h"
#include "../campaign/CampaignManager.h"
#include "../ui/CampaignHUD.h"
#include "../ui/HideoutScreen.h"
#include "../hero/LevelUpSystem.h"
#include "../hero/HeroClass.h"

class Game
{
public:
    Game() = default;
    ~Game() = default;

    bool init(const std::string& title, int width, int height);
    void run();
    void shutdown();

private:
    // ── Core loop ──────────────────────────────────────────────────────────────
    void processEvents();
    void update(float dt);
    void render();

    // ── State dispatch ─────────────────────────────────────────────────────────
    void updateWorldMap(float dt);
    void renderWorldMap();
    void updateCombat(float dt);
    void renderCombat();
    void updateTown(float dt);
    void renderTown();
    void updateEditor(float dt);
    void renderEditor();
    void updateCampaign(float dt);
    void renderCampaign();
    void enterCampaign();
    void exitCampaign();

    // ── State transitions ─────────────────────────────────────────────────────
    void enterWorldMap();
    void enterCombat(Hero& playerHero,
                     const std::vector<CombatUnit>& playerUnits,
                     const Hero& enemyHero,
                     const std::vector<CombatUnit>& enemyUnits);
    void enterTown(Town* town);
    void enterEditor();
    void exitCombat(bool playerWon);
    void exitTown();
    void exitEditor();

    // ── World map helpers ──────────────────────────────────────────────────────
    void updateHeroMovement(float dt);
    void drawHero(const Hero& hero);
    void onTileClicked(HexCoord h);
    void checkTileEvents();

    // ── ImGui integration ──────────────────────────────────────────────────────
    bool initImGui();
    void shutdownImGui();
    void beginImGuiFrame();
    void endImGuiFrame();

    // ── Save / Load ────────────────────────────────────────────────────────────
    void saveGame(const std::string& path);
    bool loadGame(const std::string& path);

    // ── Level-up modal ─────────────────────────────────────────────────────────
    void renderLevelUpModal();

    // ── Hideout screen ─────────────────────────────────────────────────────────
    void renderHideoutScreen();

    // ── SDL / GL ───────────────────────────────────────────────────────────────
    SDL_Window*   m_window  = nullptr;
    SDL_GLContext m_glCtx   = nullptr;
    bool          m_running = false;
    int           m_width   = 0;
    int           m_height  = 0;

    // ── State machine ──────────────────────────────────────────────────────────
    GameState m_state = GameState::WorldMap;

    // ── Core systems ──────────────────────────────────────────────────────────
    InputState     m_input;
    SpriteBatch    m_batch;
    Camera2D       m_camera;
    UIRenderer     m_ui;

    // ── World map ─────────────────────────────────────────────────────────────
    HexMap         m_map;
    HexMapRenderer m_hexRenderer;
    MapSize        m_mapSize = MapSize::Small;

    // ── Heroes ────────────────────────────────────────────────────────────────
    std::vector<Hero> m_heroes;
    int               m_activeHeroIdx = 0;

    HexCoord       m_hovered  {-999, -999};
    HexCoord       m_selected {-999, -999};

    float m_moveT    = 1.0f;
    float m_moveSrcX = 0.0f, m_moveSrcY = 0.0f;
    float m_moveDstX = 0.0f, m_moveDstY = 0.0f;

    std::vector<HexCoord> m_reachable;

    // ── Towns & resources ─────────────────────────────────────────────────────
    std::vector<Town>         m_towns;
    std::vector<ResourceNode> m_resources;
    std::vector<HexCoord>     m_heroStarts;
    BuildingRegistry          m_registry;

    // ── Economy / turn ────────────────────────────────────────────────────────
    Resources    m_playerResources;
    TurnManager  m_turns;

    // ── Combat ────────────────────────────────────────────────────────────────
    CombatEngine m_combat;

    // ── UI ────────────────────────────────────────────────────────────────────
    WorldMapHUD  m_worldHUD;
    CombatHUD    m_combatHUD;
    TownScreen   m_townScreen;

    // ── Editor ────────────────────────────────────────────────────────────────
    MapEditor         m_editor;
    SimulatorWindow   m_simWindow;
    bool              m_imguiReady = false;

    // ── Scripting ─────────────────────────────────────────────────────────────
    LuaEngine     m_lua;
    TriggerSystem m_triggers;

    // ── Campaign ───────────────────────────────────────────────────────────────
    CampaignManager m_campaign;
    CampaignHUD     m_campaignHUD;

    // ── Persistent meta layer ──────────────────────────────────────────────────
    HideoutDB    m_hideout;
    HideoutScreen m_hideoutScreen;
    bool          m_showHideoutScreen = false;

    // ── Level-up flow ──────────────────────────────────────────────────────────
    HeroClassRegistry          m_classRegistry;
    std::vector<LevelUpOffer>  m_levelUpOffers;
    bool                       m_showLevelUpModal = false;
};
