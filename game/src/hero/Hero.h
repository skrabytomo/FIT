#pragma once
#include <vector>
#include <string>
#include "../world/HexMap.h"
#include "FactionId.h"
#include "Skills.h"
#include "Artifacts.h"

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

struct UnitStack
{
    int defId = 0;
    int count = 0;
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
    int xp          = 0;
    int xpToNext    = 100;
    int attack      = 2;
    int defense     = 2;
    int visionRange = 5;

    HeroSkills    skills;
    HeroArtifacts artifacts;

    // Casting stats (grow via class scaling on level-up)
    int lightPower  = 0;
    int bloodPower  = 0;
    int deathPower  = 0;
    int naturePower = 0;
    int forgePower  = 0;
    int fleshPower  = 0;

    // Hero mana pool
    int mana    = 10;
    int maxMana = 10;

    // Hero HP (for targeted abilities)
    int heroHp    = 100;
    int heroMaxHp = 100;

    // Known spell IDs (learned via spellbooks on map or town buildings)
    std::vector<int> knownSpells;

    // Collected artifact IDs not yet equipped
    std::vector<int> artifactInventory;

    // Army — up to 7 unit stacks (indexed by slot)
    std::vector<UnitStack> army;

    static int xpRequired(int lvl) { return 100 * lvl * lvl; }

    // Returns true if the hero leveled up
    bool addXp(int amount) {
        xp += amount;
        bool leveled = false;
        while (xp >= xpToNext) {
            xp -= xpToNext;
            level++;
            xpToNext = xpRequired(level);
            leveled = true;
        }
        return leveled;
    }

    int moveCost(Terrain t) const;
    bool canEnter(Terrain t) const;
};
