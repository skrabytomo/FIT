#pragma once
#include <string>
#include <vector>
#include <utility>
#include "../world/HexMap.h"
#include "../hero/Hero.h"
#include "../town/Town.h"
#include "../data/Resources.h"

// ── Tile save data (only fields that change at runtime) ────────────────────────
struct TileSave
{
    int  q, r;
    int  terrain;      // Terrain enum as int
    bool explored;
    bool visible;
    uint32_t heroId;
    uint32_t townId;
    uint32_t resourceId;
};

// ── Hero save data ─────────────────────────────────────────────────────────────
struct HeroSave
{
    uint32_t    id;
    std::string name;
    int         faction;   // FactionId as int
    int         classId;
    int         posQ, posR;
    int         movePool;
    int         maxMove;
    int         level;
    int         attack;
    int         defense;
    int         visionRange;
    // Extended stats
    int         xp;
    int         xpToNext;
    int         hp;
    int         maxHp;
    int         mana;
    int         maxMana;
    // Casting stats
    int         lightPower;
    int         bloodPower;
    int         deathPower;
    int         naturePower;
    int         forgePower;
    int         fleshPower;
};

// ── Dwelling save ──────────────────────────────────────────────────────────────
struct DwellingSave
{
    int  buildingId;
    int  tier;
    int  path;         // UpgradePath as int
    int  available;
    int  accumulated;
};

// ── Town save data ─────────────────────────────────────────────────────────────
struct TownSave
{
    uint32_t    id;
    std::string name;
    int         faction;   // FactionId as int
    int         posQ, posR;
    uint32_t    ownerId;
    std::vector<int>         builtBuildings;
    std::vector<DwellingSave> dwellings;
    int         fortHP;
    int         fortMaxHP;
    // Weekly income stored
    std::array<int, RESOURCE_COUNT> weeklyIncomeAmounts;
};

// ── Campaign save state ────────────────────────────────────────────────────────
struct CampaignSaveState
{
    bool     active      = false;
    int      missionIdx  = 0;
    int      orderScore  = 0;
    int      lightScore  = 0;
    std::vector<std::pair<uint32_t, int>> decisions; // {decisionId, choiceIdx}
};

// ── Full game save ─────────────────────────────────────────────────────────────
struct GameSaveData
{
    int version = 1;

    // Turn state
    int day  = 1;
    int week = 1;

    // Map
    int mapRadius   = 0;
    int mapSizeEnum = 0;   // MapSize as int

    // Player resources
    std::array<int, RESOURCE_COUNT> resourceAmounts = {};

    // Entities
    std::vector<HeroSave>  heroes;
    std::vector<TownSave>  towns;

    // Tile fog/entity state
    std::vector<TileSave>  tiles;

    // Campaign state (optional — only populated when a campaign is active)
    CampaignSaveState campaign;
};

// ── Save / Load API ────────────────────────────────────────────────────────────
namespace SaveLoad
{
    bool saveGame(const std::string& path, const GameSaveData& data);
    bool loadGame(const std::string& path, GameSaveData& out);

    // Convenience: pack/unpack live game objects
    GameSaveData packState(const HexMap& map,
                           const std::vector<Hero>& heroes,
                           const std::vector<Town>& towns,
                           const Resources& playerRes,
                           int day, int week,
                           MapSize mapSize);

    void unpackState(const GameSaveData& save,
                     HexMap& map,
                     std::vector<Hero>& heroes,
                     std::vector<Town>& towns,
                     Resources& playerRes,
                     int& day, int& week);
}
