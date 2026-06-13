#include "DamageCalc.h"
#include <cmath>
#include <algorithm>
#include <random>

static thread_local std::mt19937 s_rng{std::random_device{}()};

void DamageCalc::seedRng(uint32_t seed)
{
    s_rng.seed(seed);
}

// ── Damage roll ────────────────────────────────────────────────────────────────
int DamageCalc::rollDamage(int dmin, int dmax, int count)
{
    if (dmin >= dmax) return dmin * count;
    std::uniform_int_distribution<int> dist(dmin, dmax);
    int total = 0;
    for (int i = 0; i < count; ++i) total += dist(s_rng);
    return total;
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

    // Holy units deal bonus damage to Undead, BloodBound, and Mechanical
    if (hasTag(attackerTags, UnitTag::Holy)) {
        if (hasTag(defenderTags, UnitTag::Undead))      bonus *= 1.07f;
        if (hasTag(defenderTags, UnitTag::BloodBound))  bonus *= 1.04f;
        if (hasTag(defenderTags, UnitTag::Mechanical))  bonus *= 1.08f;
    }

    // Humanoid units deal bonus damage to Holy (numbers vs faith)
    if (hasTag(attackerTags, UnitTag::Humanoid)) {
        if (hasTag(defenderTags, UnitTag::Holy)) bonus *= 1.08f;
    }

    // BloodBound deal bonus damage to Holy (primal blood magic corrupts divine light)
    if (hasTag(attackerTags, UnitTag::BloodBound)) {
        if (hasTag(defenderTags, UnitTag::Holy)) bonus *= 1.09f;
    }

    // Void units deal bonus damage to Holy, Undead, Mechanical, Humanoid, and OrganicMech
    if (hasTag(attackerTags, UnitTag::Void)) {
        if (hasTag(defenderTags, UnitTag::Holy))
            bonus *= 1.08f;
        else if (hasTag(defenderTags, UnitTag::Undead))
            bonus *= 1.08f;
        else if (hasTag(defenderTags, UnitTag::Mechanical))
            bonus *= 1.05f;
        else if (hasTag(defenderTags, UnitTag::Humanoid))
            bonus *= 1.08f;
        else if (hasTag(defenderTags, UnitTag::OrganicMech))
            bonus *= 1.05f;
    }

    // Organic-Mech (Amalgamate) deals bonus damage to Mechanical, Humanoid, Beast, and Void
    if (hasTag(attackerTags, UnitTag::OrganicMech)) {
        if (hasTag(defenderTags, UnitTag::Mechanical))    bonus *= 1.12f;
        else if (hasTag(defenderTags, UnitTag::Humanoid)) bonus *= 1.04f;
        else if (hasTag(defenderTags, UnitTag::Beast))    bonus *= 1.06f;
        else if (hasTag(defenderTags, UnitTag::Void))     bonus *= 1.08f;
    }

    // Undead deal bonus damage to Beast (undead hunger/blight vs living creatures)
    if (hasTag(attackerTags, UnitTag::Undead)) {
        if (hasTag(defenderTags, UnitTag::Beast)) bonus *= 1.06f;
    }

    // BloodBound deal bonus damage to Beast and Undead (primal hunger consumes both)
    if (hasTag(attackerTags, UnitTag::BloodBound)) {
        if (hasTag(defenderTags, UnitTag::Beast))  bonus *= 1.08f;
        if (hasTag(defenderTags, UnitTag::Undead)) bonus *= 1.05f;
    }

    // Humanoid numbers and tactics overcome Undead resilience
    if (hasTag(attackerTags, UnitTag::Humanoid)) {
        if (hasTag(defenderTags, UnitTag::Undead)) bonus *= 1.10f;
    }

    // Beast deals bonus damage to Void and Undead (primal life-force disrupts both)
    if (hasTag(attackerTags, UnitTag::Beast)) {
        if (hasTag(defenderTags, UnitTag::Void))  bonus *= 1.08f;
        if (hasTag(defenderTags, UnitTag::Undead)) bonus *= 1.08f;
    }

    // Void units deal bonus damage to BloodBound (entropic void unravels blood-bonds)
    if (hasTag(attackerTags, UnitTag::Void)) {
        if (hasTag(defenderTags, UnitTag::BloodBound)) bonus *= 1.08f;
    }

    // Holy light sears Void energy — counters VO's widespread advantage
    if (hasTag(attackerTags, UnitTag::Holy)) {
        if (hasTag(defenderTags, UnitTag::Void)) bonus *= 1.13f;
    }

    // Humanoid ingenuity overcomes Mechanical brute-force
    if (hasTag(attackerTags, UnitTag::Humanoid)) {
        if (hasTag(defenderTags, UnitTag::Mechanical)) bonus *= 1.06f;
    }

    // Mechanical and Holy have a mutual rivalry — precision disrupts void, faith resists steel
    if (hasTag(attackerTags, UnitTag::Mechanical)) {
        if (hasTag(defenderTags, UnitTag::Holy)) bonus *= 0.95f;
        if (hasTag(defenderTags, UnitTag::Void)) bonus *= 1.04f;
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

    // Attack vs defense modifier (include per-round bonuses)
    int diff = (attacker.attack + attacker.roundAttackBonus)
             - (defender.defense + defender.roundDefenseBonus);
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
    if (isRetaliation) modifier *= 0.50f;

    int finalDmg = static_cast<int>(baseDmg * modifier);
    finalDmg = std::max(1, finalDmg);

    result.damage = finalDmg;
    result.killed = defender.applyDamage(finalDmg);

    // Morale update — attacker gains morale on kill (not during retaliation; at most one surge/round)
    if (result.killed > 0 && !isRetaliation && !attacker.moraleImmune
        && !attacker.moraleSurgedThisRound) {
        attacker.morale += std::min(10 * result.killed, 60);
        if (attacker.morale >= MORALE_THRESHOLD) {
            attacker.morale = 0;  // full reset — prevents cascade on bonus action
            result.moraleTrigger = true;
            attacker.moraleSurgedThisRound = true;
        }
    }

    // Defender loses morale from taking losses
    if (result.killed > 0 && !defender.moraleImmune) {
        defender.morale = std::max(0, defender.morale - 8);
    }

    // Retaliation — defender hits back if it hasn't acted yet this round
    if (!isRetaliation && defender.alive && defender.canRetaliate && !defender.hasActed) {
        DamageResult ret = attack(defender, attacker, grid, true);
        defender.canRetaliate = false;
        result.retaliated = true;
        (void)ret; // retaliation damage already applied
    }

    return result;
}
