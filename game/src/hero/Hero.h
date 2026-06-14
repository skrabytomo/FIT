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

    // Garrison: hero digs in at current tile, blocks enemy passage, +2 def in combat
    bool isGarrisoned = false;

    // Specialty progression: tracked stats for specialty effects
    int battlesWon = 0;         // total combat victories (Veteran specialty)
    int specialtyAtk = 0;       // accumulated specialty attack bonus

    // Transient per-battle specialty flags (set in enterCombat, not persisted)
    bool feastSpecialty      = false;  // Blood Prince — drain own units, heal hero per round
    bool witherSpecialty     = false;  // Fell Druid — enemies -1 ATK per round
    bool ironDiscipline      = false;  // Warlord Mechanic — own units immune to morale loss
    bool exsanguinate        = false;  // Crimson Mage — one Blood spell costs no mana
    bool exsanguinateUsed    = false;  // tracks if free cast was used this battle
    bool heresyDetection     = false;  // Inquisitor — negate first enemy spell cast
    bool heresyDetectionUsed = false;  // tracks if negation was used this battle
    bool lightningRodSpecialty = false; // Stormbark — first enemy spell reflected back
    bool lightningRodUsed      = false;
    bool harmonySpecialty      = false; // Warsinger — adjacent same-side pairs +1 ATK/DEF
    bool elixirSpecialty       = false; // Blood Sage — once per battle fully heal one unit
    bool elixirUsed            = false;
    bool coordinatedStrikeSpecialty = false; // Warden Captain — marked target +2 ATK for all player attacks
    bool bloodPenanceSpecialty      = false; // Flagellant Marshal — units +2 ATK, hero -5 HP/round

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
