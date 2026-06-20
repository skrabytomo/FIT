#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include "HexMap.h"
#include "../data/ResourceNode.h"
#include "../town/Town.h"
#include "../world/WorldObject.h"

// ── Generation parameters ──────────────────────────────────────────────────────
struct WorldGenParams
{
    uint32_t seed            = 42;
    MapSize  size            = MapSize::Medium;
    int      playerCount     = 2;     // 1-8, one starting town each
    float    resourceDensity = 1.0f;  // multiplier; 1.0 = default
    bool     balancedStart   = true;  // equidistant player towns
    float    waterRatio      = 0.15f; // target fraction of map that is water
    bool     richNeutralZones = true;  // extra objects in neutral zones
    bool     zoneBasedTerrain = true;  // use zone terrain vs old noise biomes
};

// ── Generation results ─────────────────────────────────────────────────────────
struct WorldGenResult
{
    std::vector<Town>         towns;          // one per player slot
    std::vector<ResourceNode> resources;      // all resource nodes
    std::vector<HexCoord>     startPositions; // hero spawn coords (one per player)
    std::vector<WorldObject>  worldObjects;   // observatories, shrines, dwellings, etc.
};

// ── WorldGen ──────────────────────────────────────────────────────────────────
// Call map.create(params.size) BEFORE calling generate().
class WorldGen
{
public:
    static WorldGenResult generate(HexMap& map, const WorldGenParams& params);

private:
    static void passNoiseTerrain(HexMap& map, const WorldGenParams& p);
    static void passBiomeClusters(HexMap& map, const WorldGenParams& p,
                                  const std::vector<HexCoord>& centers);
    static void passSmoothCoastlines(HexMap& map, int iterations = 2);
    static void passRemoveIsolatedWater(HexMap& map);

    static std::vector<HexCoord> pickSpawnPositions(const HexMap& map,
                                                    int count, uint32_t seed);
    static std::vector<ResourceNode> placeResources(HexMap& map,
                                                    const WorldGenParams& p,
                                                    uint32_t& nextId);
    static void buildTowns(WorldGenResult& result,
                           const HexMap& map,
                           const std::vector<HexCoord>& positions,
                           uint32_t& nextId);
    static void placeWorldObjects(WorldGenResult&, HexMap&, const WorldGenParams&, uint32_t&);

    // ── Zone-based generation ─────────────────────────────────────────────────
    // Returns zoneId per tile coord (-1=water, 0..N-1 player zones, N..N+M-1 neutral)
    static std::unordered_map<HexCoord, int, HexCoordHash> assignZones(
        const HexMap& map,
        const std::vector<HexCoord>& playerSpawns,
        std::vector<HexCoord>& neutralCentersOut,
        uint32_t& rng);

    static void passZoneTerrain(
        HexMap& map,
        const std::unordered_map<HexCoord, int, HexCoordHash>& tileZones,
        const std::vector<Terrain>& zoneTerrain,
        uint32_t& rng);

    static void placePlayerZoneMines(
        WorldGenResult& result, HexMap& map,
        const std::vector<HexCoord>& spawns,
        const std::unordered_map<HexCoord, int, HexCoordHash>& tileZones,
        const WorldGenParams& p, uint32_t& nextId, uint32_t& rng);

    static void placeZoneObjects(
        WorldGenResult& result, HexMap& map,
        const std::vector<HexCoord>& allCenters,
        const std::vector<int>& zonePlayer,   // -1=neutral, 0..N=player idx
        const std::unordered_map<HexCoord, int, HexCoordHash>& tileZones,
        const WorldGenParams& p, uint32_t& nextId, uint32_t& rng);

    static Terrain heightToTerrain(float h, float waterCutoff);
    static bool    isLand(Terrain t);
    static bool    isSuitable(const HexMap& map, HexCoord c, int minDist,
                              const std::vector<HexCoord>& occupied);
    static ResourceType terrainResource(Terrain t, uint32_t rng);

    // LCG for deterministic random inside generation
    static uint32_t lcg(uint32_t& state) {
        state = state * 1664525u + 1013904223u;
        return state;
    }
};
