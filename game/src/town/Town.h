#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "../world/HexMap.h"
#include "../hero/Hero.h"
#include "../data/Resources.h"
#include "BuildingDef.h"
#include "UnitDef.h"

// ── Per-dwelling state ─────────────────────────────────────────────────────────
struct DwellingState
{
    int         buildingId  = 0;
    int         tier        = 0;
    UpgradePath path        = UpgradePath::None;
    int         available   = 0;   // units available to recruit this week
    int         accumulated = 0;   // carried over from previous weeks (uncapped)
};

// ── Town instance ──────────────────────────────────────────────────────────────
class Town
{
public:
    uint32_t    id       = 0;
    std::string name;
    FactionId   faction  = FactionId::None;
    HexCoord    pos      = {0, 0};
    uint32_t    ownerId  = 0;   // hero/player id, 0 = neutral

    // Built buildings (set of building IDs)
    std::vector<int> builtBuildings;

    // Dwelling states per tier
    std::vector<DwellingState> dwellings; // indexed by tier-1

    // Weekly resource income (sum of all economy buildings)
    Resources weeklyIncome;

    // Fort HP (for siege) — 0 = no fort built
    int fortHP    = 0;
    int fortMaxHP = 0;

    // Garrison — units defending the town (up to 7 slots)
    std::vector<UnitStack> garrison;

    // ── Queries ────────────────────────────────────────────────────────────────
    bool hasBuilding(int buildingId) const;
    bool canBuild(int buildingId, const std::vector<BuildingDef>& defs) const;

    // Returns total weekly growth for a tier (base + support bonuses)
    int weeklyGrowth(int tier) const;

    // ── Actions ───────────────────────────────────────────────────────────────
    // Build a building — returns false if prerequisites not met or already built
    bool build(int buildingId, const std::vector<BuildingDef>& defs, Resources& playerRes);

    // Called every week — add growth to available pools
    void onWeekStart(const std::vector<BuildingDef>& defs);

    // Recruit units from a dwelling — returns actual count recruited
    int recruit(int tier, int count, Resources& playerRes,
                const std::vector<UnitDef>& unitDefs);
};
