#include "WorldGen.h"
#include "Noise.h"
#include "HexGrid.h"
#include "../magic/SpellDef.h"
#include <cmath>
#include <algorithm>
#include <cstdio>

// ── Main entry point ──────────────────────────────────────────────────────────
WorldGenResult WorldGen::generate(HexMap& map, const WorldGenParams& p)
{
    printf("WorldGen: seed=%u size=%d players=%d\n",
           p.seed, static_cast<int>(p.size), p.playerCount);

    // 1. Fill map with noise-driven terrain
    passNoiseTerrain(map, p);

    // 2. Pick player spawn zones, then grow faction home terrain clusters
    auto spawnPos = pickSpawnPositions(map, p.playerCount, p.seed);
    passBiomeClusters(map, p, spawnPos);

    // 3. Smooth choppy coastlines
    passSmoothCoastlines(map, 2);
    passRemoveIsolatedWater(map);

    // 4. Place resources
    uint32_t nextId = 1;
    auto resources = placeResources(map, p, nextId);

    // 5. Build town objects
    WorldGenResult result;
    result.resources     = std::move(resources);
    result.startPositions = spawnPos;
    buildTowns(result, map, spawnPos, nextId);
    placeWorldObjects(result, map, p, nextId);

    printf("WorldGen: %zu towns, %zu resources, %zu world objects\n",
           result.towns.size(), result.resources.size(), result.worldObjects.size());
    return result;
}

// ── Pass 1: Noise terrain ─────────────────────────────────────────────────────
void WorldGen::passNoiseTerrain(HexMap& map, const WorldGenParams& p)
{
    Noise2D noise(p.seed);

    // Scale noise so the full map radius maps to ~2 noise cycles
    float scale = 2.0f / static_cast<float>(map.radius());

    // Elevation bias: makes map edges more likely to be water (island feel)
    float waterCutoff = -0.4f + p.waterRatio * 0.8f;   // higher ratio → more water

    HexGrid grid(40.f);   // size arbitrary — only ratio matters

    map.forEach([&](HexTile& tile) {
        float wx, wy;
        grid.hexToWorld(tile.coord, wx, wy);

        // Base height from fbm noise
        float h = noise.fbm(wx * scale, wy * scale, 5, 2.0f, 0.5f);

        // Radial falloff — edges of map become ocean
        float r = static_cast<float>(HexGrid::distance(tile.coord, {0, 0}));
        float maxR = static_cast<float>(map.radius());
        float edge = 1.0f - (r / maxR);         // 1 at center, 0 at edge
        float bias = 2.5f * (edge - 0.5f);      // positive at center, negative at edge
        h += bias * 0.35f;

        tile.terrain = heightToTerrain(h, waterCutoff);
    });
}

Terrain WorldGen::heightToTerrain(float h, float waterCutoff)
{
    if (h < waterCutoff - 0.05f) return Terrain::Water;
    if (h < waterCutoff + 0.02f) return Terrain::Swamp;
    if (h < waterCutoff + 0.12f) return Terrain::Plains;
    if (h < waterCutoff + 0.22f) return Terrain::Forest;
    if (h < waterCutoff + 0.35f) return Terrain::Highland;
    if (h < waterCutoff + 0.50f) return Terrain::Rocky;
    if (h < waterCutoff + 0.65f) return Terrain::Industrial;
    if (h < waterCutoff + 0.80f) return Terrain::Barren;
    return Terrain::Volcanic;
}

bool WorldGen::isLand(Terrain t)
{
    return t != Terrain::Water;
}

// ── Pass 2: Biome clusters around spawn points ────────────────────────────────
void WorldGen::passBiomeClusters(HexMap& map, const WorldGenParams& p,
                                  const std::vector<HexCoord>& centers)
{
    // Faction home terrains in order (matches FactionId enum)
    static const Terrain kHomeTerrain[] = {
        Terrain::Sacred,          // Holy Order
        Terrain::Highland,        // Crimson Wardens
        Terrain::Forest,          // Thornkin
        Terrain::Toxic,           // Eternal Empire
        Terrain::Corrupted,       // Bloodsworn
        Terrain::CorruptedForest, // Voidkin
        Terrain::Industrial,      // Iron Assembly
        Terrain::Wasteland,       // Amalgamate
        Terrain::Plains,          // Convergence (neutral)
    };

    uint32_t rng = p.seed ^ 0xDEADBEEF;
    int clusterRadius = map.radius() / 5 + 1;

    for (int i = 0; i < static_cast<int>(centers.size()); ++i) {
        Terrain home = kHomeTerrain[i % 9];
        auto cells = HexGrid::range(centers[i], clusterRadius);
        Noise2D cNoise(rng + static_cast<uint32_t>(i) * 13);

        for (auto& c : cells) {
            HexTile* tile = map.getTile(c);
            if (!tile || !isLand(tile->terrain)) continue;

            float dist = static_cast<float>(HexGrid::distance(c, centers[i]));
            float frac = 1.0f - dist / static_cast<float>(clusterRadius);
            // Probabilistic: high chance near center, tapers off
            float n = cNoise.sample(c.q * 0.3f, c.r * 0.3f) * 0.5f + 0.5f;
            if (n < frac * 0.8f)
                tile->terrain = home;
        }
        lcg(rng);
    }
}

// ── Pass 3: Smooth coastlines ─────────────────────────────────────────────────
void WorldGen::passSmoothCoastlines(HexMap& map, int iterations)
{
    for (int iter = 0; iter < iterations; ++iter) {
        std::vector<std::pair<HexCoord, Terrain>> changes;

        for (auto c : map.coords()) {
            HexTile* tile = map.getTile(c);
            if (!tile) continue;

            auto neighbors = HexGrid::neighbors(c);
            int waterCount = 0, landCount = 0;
            Terrain commonLand = Terrain::Plains;

            for (auto n : neighbors) {
                const HexTile* nt = map.getTile(n);
                if (!nt) continue;
                if (nt->terrain == Terrain::Water) ++waterCount;
                else { ++landCount; commonLand = nt->terrain; }
            }

            // Isolated water tile surrounded by land → convert to land
            if (tile->terrain == Terrain::Water && waterCount <= 1 && landCount >= 4)
                changes.push_back({c, commonLand});
            // Isolated land tile surrounded by water → convert to water
            else if (isLand(tile->terrain) && landCount <= 1 && waterCount >= 4)
                changes.push_back({c, Terrain::Water});
        }

        for (auto& [coord, t] : changes)
            if (HexTile* tile = map.getTile(coord)) tile->terrain = t;
    }
}

// ── Pass 4: Remove isolated water specks ─────────────────────────────────────
void WorldGen::passRemoveIsolatedWater(HexMap& map)
{
    // Single water hexes surrounded by all-land get converted to swamp
    for (auto c : map.coords()) {
        HexTile* tile = map.getTile(c);
        if (!tile || tile->terrain != Terrain::Water) continue;

        auto neighbors = HexGrid::neighbors(c);
        bool allLand = true;
        for (auto n : neighbors) {
            const HexTile* nt = map.getTile(n);
            if (!nt || nt->terrain == Terrain::Water) { allLand = false; break; }
        }
        if (allLand) tile->terrain = Terrain::Swamp;
    }
}

// ── Spawn position selection ──────────────────────────────────────────────────
std::vector<HexCoord> WorldGen::pickSpawnPositions(const HexMap& map,
                                                    int count, uint32_t seed)
{
    std::vector<HexCoord> candidates, result;
    int radius = map.radius();

    // Collect land tiles in the inner 60% of the map radius
    for (auto c : map.coords()) {
        const HexTile* tile = map.getTile(c);
        if (!tile || !isLand(tile->terrain)) continue;
        int d = HexGrid::distance(c, {0, 0});
        if (d < radius / 4 || d > radius * 3 / 4) continue;
        candidates.push_back(c);
    }

    if (candidates.empty()) {
        // Fallback: use axial positions around the ring
        for (int i = 0; i < count; ++i) {
            float angle = 2.0f * 3.14159f * i / count;
            int q = static_cast<int>(std::round(radius * 0.6f * std::cos(angle)));
            int r = static_cast<int>(std::round(radius * 0.6f * std::sin(angle)));
            result.push_back({q, r});
        }
        return result;
    }

    uint32_t rng = seed ^ 0xBEEF1234;

    // For balanced start, pick evenly spaced candidates around a ring
    if (count <= 1) {
        // Just pick a random land tile near center
        uint32_t idx = lcg(rng) % static_cast<uint32_t>(candidates.size());
        result.push_back(candidates[idx]);
        return result;
    }

    // Place first randomly, then pick furthest from all existing
    {
        uint32_t idx = lcg(rng) % static_cast<uint32_t>(candidates.size());
        result.push_back(candidates[idx]);
    }

    while (static_cast<int>(result.size()) < count) {
        HexCoord best = candidates[0];
        int bestDist = 0;

        for (auto& c : candidates) {
            int minD = INT32_MAX;
            for (auto& r : result) minD = std::min(minD, HexGrid::distance(c, r));
            if (minD > bestDist) { bestDist = minD; best = c; }
        }
        result.push_back(best);
    }

    return result;
}

// ── Resource placement ────────────────────────────────────────────────────────
std::vector<ResourceNode> WorldGen::placeResources(HexMap& map,
                                                    const WorldGenParams& p,
                                                    uint32_t& nextId)
{
    std::vector<ResourceNode> nodes;
    uint32_t rng = p.seed ^ 0xCAFE0000;
    int radius = map.radius();

    // Target count scales with map area and density
    int baseCount = static_cast<int>(radius * radius * 0.08f * p.resourceDensity);
    baseCount = std::max(baseCount, 6);

    auto allCoords = map.coords();
    // Shuffle candidate list deterministically
    for (size_t i = allCoords.size() - 1; i > 0; --i) {
        uint32_t j = lcg(rng) % static_cast<uint32_t>(i + 1);
        std::swap(allCoords[i], allCoords[j]);
    }

    int placed = 0;
    for (auto& c : allCoords) {
        if (placed >= baseCount) break;

        HexTile* tile = map.getTile(c);
        if (!tile || !isLand(tile->terrain)) continue;
        if (tile->resourceId != 0) continue;

        // Minimum spacing between resources
        bool tooClose = false;
        for (auto& n : nodes) {
            if (HexGrid::distance(c, n.pos) < 3) { tooClose = true; break; }
        }
        if (tooClose) continue;

        ResourceNode node;
        node.id     = nextId++;
        node.pos    = c;
        node.type   = terrainResource(tile->terrain, lcg(rng));
        node.amount = 2 + static_cast<int>(lcg(rng) % 4);  // 2–5 per week

        tile->resourceId = node.id;
        nodes.push_back(node);
        ++placed;
    }

    return nodes;
}

ResourceType WorldGen::terrainResource(Terrain t, uint32_t rng)
{
    switch (t) {
        case Terrain::Sacred:
        case Terrain::Plains:       return ResourceType::FaithStones;
        case Terrain::Corrupted:
        case Terrain::Toxic:        return ResourceType::BloodEssence;
        case Terrain::Forest:
        case Terrain::CorruptedForest: return ResourceType::VerdantSap;
        case Terrain::Industrial:
        case Terrain::Rocky:        return ResourceType::Iron;
        case Terrain::Swamp:        return ResourceType::Mercury;
        case Terrain::Highland:     return (rng & 1) ? ResourceType::Iron
                                                     : ResourceType::Gold;
        default:                    return ResourceType::Gold;
    }
}

// ── Town building ─────────────────────────────────────────────────────────────
void WorldGen::buildTowns(WorldGenResult& result,
                           const HexMap& map,
                           const std::vector<HexCoord>& positions,
                           uint32_t& nextId)
{
    // Faction IDs match order of spawn positions (players pick factions separately)
    static const FactionId kFactions[] = {
        FactionId::HolyOrder,   FactionId::CrimsonWardens,
        FactionId::Thornkin,    FactionId::EternalEmpire,
        FactionId::Bloodsworn,  FactionId::Voidkin,
        FactionId::IronAssembly,FactionId::Amalgamate,
        FactionId::Convergence,
    };

    static const char* kTownNames[] = {
        "Sanctuary",   "Ironhold",   "Thornwald",  "Greyspire",
        "Crimsongate", "Voidhaven",  "Forge City", "The Meld",
        "The Nexus",
    };

    for (int i = 0; i < static_cast<int>(positions.size()); ++i) {
        Town t;
        t.id      = nextId++;
        t.name    = kTownNames[i % 8];
        t.faction = kFactions[i % 8];
        t.pos     = positions[i];
        t.ownerId = static_cast<uint32_t>(i + 1);  // player ID 1-based
        result.towns.push_back(t);
    }
}

bool WorldGen::isSuitable(const HexMap& map, HexCoord c, int minDist,
                           const std::vector<HexCoord>& occupied)
{
    const HexTile* tile = map.getTile(c);
    if (!tile || !isLand(tile->terrain)) return false;
    for (auto& o : occupied)
        if (HexGrid::distance(c, o) < minDist) return false;
    return true;
}

// ── World object placement ────────────────────────────────────────────────────
void WorldGen::placeWorldObjects(WorldGenResult& result, HexMap& map,
                                  const WorldGenParams& p, uint32_t& nextId)
{
    uint32_t rng = p.seed ^ 0xDEADB00B;
    int radius = map.radius();

    // Build a list of occupied positions (towns + resources)
    std::vector<HexCoord> occupied;
    for (const auto& t : result.towns)    occupied.push_back(t.pos);
    for (const auto& r : result.resources) occupied.push_back(r.pos);

    // Helper: place one world object at a random suitable location
    auto tryPlace = [&](WorldObject& obj, int minDist) -> bool {
        auto allCoords = map.coords();
        // Shuffle
        for (size_t i = allCoords.size() - 1; i > 0; --i) {
            uint32_t j = lcg(rng) % static_cast<uint32_t>(i + 1);
            std::swap(allCoords[i], allCoords[j]);
        }
        for (auto& c : allCoords) {
            if (!isSuitable(map, c, minDist, occupied)) continue;
            // Extra: check no other worldObject here
            bool used = false;
            for (const auto& wo : result.worldObjects)
                if (wo.pos == c) { used = true; break; }
            if (used) continue;
            obj.pos = c;
            occupied.push_back(c);
            return true;
        }
        return false;
    };

    // 2 Observatories (value=5 radius, far from towns)
    for (int i = 0; i < 2; ++i) {
        WorldObject obj;
        obj.id    = nextId++;
        obj.type  = WorldObjectType::Observatory;
        obj.value = 5;
        if (tryPlace(obj, radius / 3)) result.worldObjects.push_back(obj);
    }

    // 4 StatShrines (value = which stat 0-3)
    for (int i = 0; i < 4; ++i) {
        WorldObject obj;
        obj.id    = nextId++;
        obj.type  = WorldObjectType::StatShrine;
        obj.value = static_cast<int>(lcg(rng) % 4);
        obj.questState = 3; // 3 uses max
        if (tryPlace(obj, 4)) result.worldObjects.push_back(obj);
    }

    // 3 BanditCamps (value = difficulty 1-3)
    for (int i = 0; i < 3; ++i) {
        WorldObject obj;
        obj.id    = nextId++;
        obj.type  = WorldObjectType::BanditCamp;
        obj.value = 1 + static_cast<int>(lcg(rng) % 3);
        if (tryPlace(obj, 4)) result.worldObjects.push_back(obj);
    }

    // UnitDwellings: for factions 0-8, tier 1-3
    for (int faction = 0; faction < 9; ++faction) {
        for (int tier = 1; tier <= 3; ++tier) {
            WorldObject obj;
            obj.id        = nextId++;
            obj.type      = WorldObjectType::UnitDwelling;
            obj.value     = tier;
            obj.faction   = static_cast<uint8_t>(faction);
            obj.available = 4 + tier * 2;
            if (tryPlace(obj, 3)) result.worldObjects.push_back(obj);
        }
    }

    // 2 QuestGiver pairs (QuestGiver + QuestTarget 8+ tiles apart)
    for (int q = 0; q < 2; ++q) {
        WorldObject giver;
        giver.id   = nextId++;
        giver.type = WorldObjectType::QuestGiver;
        giver.questState = 0;

        if (!tryPlace(giver, 5)) continue;

        // Find a target 8+ tiles away from giver
        WorldObject target;
        target.id   = nextId++;
        target.type = WorldObjectType::QuestTarget;

        auto allCoords = map.coords();
        for (size_t i = allCoords.size() - 1; i > 0; --i) {
            uint32_t j = lcg(rng) % static_cast<uint32_t>(i + 1);
            std::swap(allCoords[i], allCoords[j]);
        }
        bool found = false;
        for (auto& c : allCoords) {
            if (HexGrid::distance(c, giver.pos) < 8) continue;
            if (!isSuitable(map, c, 4, occupied)) continue;
            bool used = false;
            for (const auto& wo : result.worldObjects)
                if (wo.pos == c) { used = true; break; }
            if (used) continue;
            target.pos = c;
            occupied.push_back(c);
            found = true;
            break;
        }
        if (!found) { nextId -= 2; continue; }  // rollback IDs if we can't place

        // Link them
        giver.linkedId  = target.id;
        target.linkedId = giver.id;

        result.worldObjects.push_back(giver);
        result.worldObjects.push_back(target);
    }

    // Terrain-specific ambient objects
    struct TerrainObjSpec {
        Terrain         terrain;
        WorldObjectType type;
        int             value;
        ResourceType    rtype;
        int             count;   // how many to place
    };
    static const TerrainObjSpec kTerrainSpecs[] = {
        { Terrain::Forest,          WorldObjectType::ForestShrine,  75,                    ResourceType::Gold,         3 },
        { Terrain::Highland,        WorldObjectType::HighlandRuin,   4,                    ResourceType::Gold,         2 },
        { Terrain::Rocky,           WorldObjectType::HighlandRuin,   3,                    ResourceType::Gold,         2 },
        { Terrain::Sacred,          WorldObjectType::HolyFountain,   0,                    ResourceType::Gold,         2 },
        { Terrain::Barren,          WorldObjectType::Oasis,          0,                    ResourceType::Gold,         2 },
        { Terrain::Wasteland,       WorldObjectType::Oasis,          0,                    ResourceType::Gold,         1 },
        { Terrain::Plains,          WorldObjectType::Campfire,      150,                   ResourceType::Gold,         4 },
        { Terrain::Volcanic,        WorldObjectType::LavaCrystal,    3,                    ResourceType::Mercury,      2 },
        { Terrain::Swamp,           WorldObjectType::SwampAltar,    SPL::CURSE,            ResourceType::Gold,         2 },
        // Extended: formerly missing terrains
        { Terrain::Corrupted,       WorldObjectType::SpellScroll,   SPL::DEATH_COIL,      ResourceType::Gold,         2 },
        { Terrain::CorruptedForest, WorldObjectType::SpellScroll,   SPL::VENOMOUS_CLOUD,  ResourceType::Gold,         2 },
        { Terrain::Industrial,      WorldObjectType::ResourceCache,  250,                   ResourceType::Iron,         2 },
        { Terrain::Toxic,           WorldObjectType::ResourceCache,    5,                   ResourceType::BloodEssence, 2 },
    };

    auto tryPlaceOnTerrain = [&](WorldObject& obj, Terrain terrain, int minDist) -> bool {
        auto allCoords = map.coords();
        for (size_t i = allCoords.size() - 1; i > 0; --i) {
            uint32_t j = lcg(rng) % static_cast<uint32_t>(i + 1);
            std::swap(allCoords[i], allCoords[j]);
        }
        for (auto& c : allCoords) {
            const HexTile* t = map.getTile(c);
            if (!t || t->terrain != terrain) continue;
            if (!isSuitable(map, c, minDist, occupied)) continue;
            bool used = false;
            for (const auto& wo : result.worldObjects)
                if (wo.pos == c) { used = true; break; }
            if (used) continue;
            obj.pos = c;
            occupied.push_back(c);
            return true;
        }
        return false;
    };

    for (const auto& spec : kTerrainSpecs) {
        for (int n = 0; n < spec.count; ++n) {
            WorldObject obj;
            obj.id           = nextId++;
            obj.type         = spec.type;
            obj.value        = spec.value;
            obj.resourceType = spec.rtype;
            if (tryPlaceOnTerrain(obj, spec.terrain, 3))
                result.worldObjects.push_back(obj);
            else
                --nextId; // rollback unused id
        }
    }
}
