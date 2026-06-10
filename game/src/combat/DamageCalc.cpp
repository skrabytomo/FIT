#include "DamageCalc.h"
#include <cstdlib>
#include <cmath>
#include <algorithm>

// ── Damage roll ────────────────────────────────────────────────────────────────
int DamageCalc::rollDamage(int dmin, int dmax, int count)
{
    if (dmin >= dmax) return dmin * count;
    int perUnit = dmin + rand() % (dmax - dmin + 1);
    return perUnit * count;
}

// ── Tile modifiers ─────────────────────────────────────────────────────────────
float DamageCalc::tileAttackMod(const CombatTile* tile)
{
    if (!tile) return 1.0f;
    switch (tile->type) {
        case CombatTileType::Attack:  return 1.25f;
        case CombatTileType::Defense: return 0.85f;
        default: return 1.0f;
    }
}

float DamageCalc::tileDefenseMod(const CombatTile* tile)
{
    if (!tile) return 1.0f;
    switch (tile->type) {
        case CombatTileType::Defense: return 0.80f;  // damage reduced on defender
        case CombatTileType::Attack:  return 1.10f;  // defender takes more
        default: return 1.0f;
    }
}

// ── Weakness bonus ─────────────────────────────────────────────────────────────
float DamageCalc::weaknessBonus(UnitTag attackerTags, UnitTag defenderTags,
                                 bool attackerIsHoly, bool defenderIsUndead)
{
    float bonus = 1.0f;

    // Holy units deal bonus damage to Undead and BloodBound
    if (hasTag(attackerTags, UnitTag::Holy)) {
        if (hasTag(defenderTags, UnitTag::Undead))     bonus *= 1.30f;
        if (hasTag(defenderTags, UnitTag::BloodBound)) bonus *= 1.20f;
    }

    // Holy faction bonus vs Undead faction (Light Power bonus — represented here)
    if (attackerIsHoly && defenderIsUndead) bonus *= 1.15f;

    // Constructs resist BloodBound abilities but are weak to Void
    if (hasTag(attackerTags, UnitTag::Void)) {
        if (hasTag(defenderTags, UnitTag::Humanoid))  bonus *= 1.20f;
        if (hasTag(defenderTags, UnitTag::Holy))       bonus *= 1.40f;
    }

    // Mechanical units take bonus from Organic-Mech (Amalgamate)
    if (hasTag(attackerTags, UnitTag::OrganicMech)) {
        if (hasTag(defenderTags, UnitTag::Mechanical)) bonus *= 1.25f;
    }

    return bonus;
}

// ── Main attack formula ────────────────────────────────────────────────────────
// HoMM3-style: damage = (attacker.attack - defender.defense) modifier * roll
// attack > defense: +5% per point (max +300%)
// defense > attack: -2.5% per point (min 30% damage)
DamageResult DamageCalc::attack(CombatUnit& attacker, CombatUnit& defender,
                                  const CombatGrid& grid, bool isRetaliation)
{
    DamageResult result;

    // Base damage roll
    int baseDmg = rollDamage(attacker.damageMin, attacker.damageMax, attacker.count);

    // Attack vs defense modifier
    int diff = attacker.attack - defender.defense;
    float modifier = 1.0f;
    if (diff > 0)
        modifier = 1.0f + std::min(diff * 0.05f, 3.0f);
    else if (diff < 0)
        modifier = std::max(1.0f + diff * 0.025f, 0.30f);

    // Tile modifiers
    const CombatTile* atkTile = grid.getTile(attacker.pos);
    const CombatTile* defTile = grid.getTile(defender.pos);
    modifier *= tileAttackMod(atkTile);
    modifier *= tileDefenseMod(defTile);

    // Weakness matrix
    bool attackerHoly   = hasTag(attacker.tags, UnitTag::Holy);
    bool defenderUndead = hasTag(defender.tags, UnitTag::Undead);
    modifier *= weaknessBonus(attacker.tags, defender.tags, attackerHoly, defenderUndead);

    // Retaliation is weaker
    if (isRetaliation) modifier *= 0.85f;

    int finalDmg = static_cast<int>(baseDmg * modifier);
    finalDmg = std::max(1, finalDmg);

    result.damage = finalDmg;
    result.killed = defender.applyDamage(finalDmg);

    // Morale update — attacker gains morale on kill
    if (result.killed > 0 && !attacker.moraleImmune) {
        attacker.morale += 10 * result.killed;
        if (attacker.morale >= MORALE_THRESHOLD) {
            attacker.morale -= MORALE_THRESHOLD;
            result.moraleTrigger = true; // bonus action granted
        }
    }

    // Defender loses morale from taking losses
    if (result.killed > 0 && !defender.moraleImmune) {
        defender.morale = std::max(0, defender.morale - 8);
    }

    // Retaliation — defender hits back if alive and hasn't retaliated yet
    if (!isRetaliation && defender.alive && defender.canRetaliate && !defender.hasActed) {
        DamageResult ret = attack(defender, attacker, grid, true);
        defender.canRetaliate = false;
        result.retaliated = true;
        (void)ret; // retaliation damage already applied
    }

    return result;
}
