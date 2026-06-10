#pragma once
#include <vector>
#include <string>
#include "../world/HexMap.h"

// Movement cost per terrain (indexed by Terrain enum)
static constexpr int BASE_MOVE_COST[] = {
    2,  // Plains
    3,  // Forest
    3,  // Highland
    3,  // Corrupted
    4,  // Toxic
    2,  // Sacred
    2,  // Industrial
    3,  // Rocky
    5,  // Swamp
    99, // Water
    4,  // Volcanic
    4,  // Barren
    4,  // Wasteland
    3,  // CorruptedForest
    3,  // FleshZone
};

enum class FactionId : uint8_t
{
    HolyOrder = 0,
    CrimsonWardens,
    Thornkin,
    EternalEmpire,
    Bloodsworn,
    Voidkin,
    IronAssembly,
    Amalgamate,
    Convergence,
    None
};

struct Hero
{
    uint32_t    id       = 0;
    std::string name;
    FactionId   faction  = FactionId::None;
    int         classId  = 0;    // HeroClassDef id

    HexCoord    pos      = {0, 0};
    int         movePool = 0;
    int         maxMove  = 20;

    std::vector<HexCoord> path;
    int pathStep = 0;

    int level       = 1;
    int attack      = 2;
    int defense     = 2;
    int visionRange = 5;

    int moveCost(Terrain t) const;
    bool canEnter(Terrain t) const;
};
