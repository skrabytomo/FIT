#include "WorldGen.h"
#include "Noise.h"
#include "HexGrid.h"
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

    printf("WorldGen: %zu towns, %zu resources\n",
           result.towns.size(), result.resources.size());
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
