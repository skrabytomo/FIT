#pragma once
#include "CombatUnit.h"
#include "CombatGrid.h"

struct DamageResult
{
    int   damage     = 0;
    int   killed     = 0;
    bool  retaliated = false;
    bool  moraleTrigger = false;  // attacker got bonus action
    bool  luckTrigger   = false;  // lucky hit — double damage
};

class DamageCalc
{
public:
    // Standard melee/ranged attack
    static DamageResult attack(CombatUnit& attacker, CombatUnit& defender,
                                const CombatGrid& grid, bool isRetaliation = false);

    // Tile modifier for a unit standing on a tile
    // Returns damage multiplier (1.0 = normal)
    static float tileAttackMod(const CombatTile* tile);
    static float tileDefenseMod(const CombatTile* tile);

    // Weakness matrix — does attacker tag counter defender tag?
    // Returns bonus damage multiplier (1.0 = no bonus)
    static float weaknessBonus(UnitTag attackerTags, UnitTag defenderTags,
                                bool attackerIsHolyFaction, bool defenderIsUndeadFaction);

    // Morale bonus action threshold
    static constexpr int MORALE_THRESHOLD = 160;

    static void seedRng(uint32_t seed);

private:
    static int rollDamage(int dmin, int dmax, int count);
};
