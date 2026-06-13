#include "CombatUnit.h"
#include <algorithm>

int CombatUnit::applyDamage(int dmg)
{
    if (dmg <= 0 || !alive) return 0;

    int killed = 0;
    int remaining = dmg;

    // Damage top unit first
    hp -= remaining;

    while (hp <= 0 && count > 0) {
        count--;
        killed++;
        if (count > 0)
            hp += maxHp; // next unit in stack
        else
            hp = 0;
    }

    if (count <= 0) {
        if (hasSecondLife && !secondLifeUsed) {
            // Eternal Empire second-life: revive at 1 unit, half HP
            count          = 1;
            hp             = std::max(1, maxHp / 2);
            secondLifeUsed = true;
        } else {
            count = 0;
            hp    = 0;
            alive = false;
        }
    }

    return killed;
}

void CombatUnit::newRound()
{
    hasMoved          = false;
    hasActed          = false;
    waitUsed          = false;
    canRetaliate      = true;
    roundAttackBonus      = 0;
    roundDefenseBonus     = 0;
    moraleSurgedThisRound = false;

    // Tick defend buff down; remove it when it expires
    if (defendRoundsLeft > 0) {
        --defendRoundsLeft;
        if (defendRoundsLeft == 0) {
            defense -= defendDefenseBonus;
            defendDefenseBonus = 0;
        }
    }
}
