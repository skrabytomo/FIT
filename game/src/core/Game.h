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
#include "../hero/Artifacts.h"
#include "../world/WorldObject.h"
#include "../audio/AudioManager.h"

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
    void updateMainMenu(float dt);
    void renderMainMenu();
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
    void renderWorldOverlay();      // ImGui DrawList markers for all map entities
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

    // ── Combat spell panel ─────────────────────────────────────────────────────
    void renderSpellPanel();

    // ── Combat board (hex grid with units) ────────────────────────────────────
    void renderCombatBoard();

    // ── Artifact equip panel (F7) ──────────────────────────────────────────────
    void renderArtifactPanel();

    // ── Hero inspect panel (F8) ───────────────────────────────────────────────
    void renderHeroInspect();

    // ── Mage guild overlay in town (ImGui) ────────────────────────────────────
    void renderMageGuild();
    void renderCapturePopup();
    void renderTownLostPopup();
    void renderTavern();

    // ── Unit exchange overlay ─────────────────────────────────────────────────
    void renderUnitExchange();

    // ── World object interaction popups ───────────────────────────────────────
    void renderDwellingPopup();
    void renderStatShrinePopup();
    void renderQuestPopup();

    // ── Victory / defeat modals ────────────────────────────────────────────────
    void renderVictoryModal();
    void renderDefeatModal();

    // ── Lua scripting API (called from Lua, thin wrappers) ────────────────────
    void bindLuaAPI();
    int  luaGetDay()       const { return m_turns.day(); }
    int  luaGetWeek()      const { return m_turns.week(); }
    int  luaGetGold()      const { return m_playerResources.get(ResourceType::Gold); }
    int  luaGetHeroLevel() const { return m_heroes.empty() ? 1 : m_heroes[m_activeHeroIdx].level; }
    void luaAddGold(int n)       { m_playerResources.add(ResourceType::Gold, n); }
    void luaAddXP(int n);
    void luaAddSpell(int spellId);

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
    std::vector<Hero> m_enemyHeroes;
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
    bool         m_showSpellPanel  = false;
    uint32_t     m_spellTargetId   = 0;    // unit ID pre-selected for spell

    // Combat board rendering/click transform
    float        m_combatBoardScale = 1.0f;
    float        m_combatBoardOffX  = 0.0f;
    float        m_combatBoardOffY  = 0.0f;

    // ── UI ────────────────────────────────────────────────────────────────────
    WorldMapHUD  m_worldHUD;
    CombatHUD    m_combatHUD;
    TownScreen   m_townScreen;

    // ── Icon texture atlas (256x96, 8x3 cells of 32x32) ──────────────────────
    Texture           m_iconTex;

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

    // ── World objects (scrolls, chests, shrines) ──────────────────────────────
    std::vector<WorldObject> m_worldObjects;
    uint32_t                 m_nextObjId = 1;

    // ── Artifact registry ──────────────────────────────────────────────────────
    ArtifactRegistry m_artifactRegistry;

    // ── Artifact / Hero inspect overlay flags ─────────────────────────────────
    bool m_showArtifactPanel = false;
    bool m_showHeroInspect   = false;

    // ── Combat tracking ───────────────────────────────────────────────────────
    uint32_t m_lastCombatEnemyId = 0;

    // ── Persistent meta layer ──────────────────────────────────────────────────
    HideoutDB    m_hideout;
    HideoutScreen m_hideoutScreen;
    bool          m_showHideoutScreen = false;

    // ── Level-up flow ──────────────────────────────────────────────────────────
    HeroClassRegistry          m_classRegistry;
    std::vector<LevelUpOffer>  m_levelUpOffers;
    bool                       m_showLevelUpModal = false;

    // ── Victory / defeat ──────────────────────────────────────────────────────
    bool m_showVictory = false;
    bool m_showDefeat  = false;

    // ── Town capture notification ─────────────────────────────────────────────
    bool        m_showCapturePopup  = false;
    std::string m_capturedTownName;

    // ── Pending town capture after garrison combat ────────────────────────────
    uint32_t    m_pendingTownCaptureId = 0;

    // ── Town-lost notification (enemy captured player town) ───────────────────
    bool        m_showTownLostPopup = false;
    std::string m_lostTownName;

    // ── Unit exchange between player heroes ────────────────────────────────────
    bool        m_showUnitExchange  = false;
    int         m_exchangeHeroIdx   = -1;   // index of the OTHER hero
    int         m_exchangeSelSlotA  = -1;   // selected slot in hero A's army
    int         m_exchangeSelSlotB  = -1;   // selected slot in hero B's army

    // ── World object interactions ─────────────────────────────────────────────
    uint32_t m_pendingObjId        = 0;
    bool     m_showDwellingPopup   = false;
    bool     m_showStatShrinePopup = false;
    bool     m_showQuestPopup      = false;
    uint32_t m_lastBanditCampId    = 0;

    // ── Audio ─────────────────────────────────────────────────────────────────
    AudioManager m_audio;
};
