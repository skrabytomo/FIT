#pragma once
#include "../combat/CombatUnit.h"
#include "../hero/Hero.h"

// Balance-tunable stat table for all 9 factions × 6 tiers.
// Edit these values to tune faction balance — the simulator will re-run automatically.
// Stats are per single unit (stack count comes from ArmyBuilder weekly growth).
//
// unlockWeek: first week the dwelling produces units (building order assumption).
// weeklyGrowth: units added to pool each week once unlocked.

struct UnitSimData
{
    const char* name;
    FactionId   faction;
    int         tier;
    int         hp;
    int         attack;
    int         defense;
    int         damageMin;
    int         damageMax;
    int         speed;
    int         range;       // 0 = melee
    int         shots;       // 0 = melee
    bool        flying;
    bool        hasSecondLife; // Eternal Empire T6
    UnitTag     tags;
    int         weeklyGrowth;
    int         unlockWeek;
};

// clang-format off
// Power budget target: T1–T3 total HP ≈ 1200–1400 at week 4.
// DPS ≈ HP / 10 per unit (rough guide). T4 unlocks week 5, T5 week 7, T6 week 9.
static constexpr UnitSimData SIM_UNITS[] = {
    // ── Holy Order ────────────────────────────────────────────────────────────
    {"Penitent",        FactionId::HolyOrder, 1, 10, 2, 3, 1,  3, 4,  0, 0, false, false, UnitTag::Humanoid|UnitTag::Holy,  14, 1},
    {"Torch Bearer",    FactionId::HolyOrder, 2, 18, 4, 4, 2,  5, 5,  0, 0, false, false, UnitTag::Humanoid|UnitTag::Holy,  10, 2},
    {"Plague Doctor",   FactionId::HolyOrder, 3, 28, 5, 5, 4,  7, 6,  5, 8, false, false, UnitTag::Humanoid|UnitTag::Holy,   7, 3},
    {"Penitent Knight", FactionId::HolyOrder, 4, 50, 8, 8, 7, 13, 7,  0, 0, true,  false, UnitTag::Humanoid|UnitTag::Holy,   5, 5},
    {"Seraph",          FactionId::HolyOrder, 5, 75,11,10,11, 19, 9,  0, 0, true,  false, UnitTag::Humanoid|UnitTag::Holy,   3, 7},
    {"Winged Hussar",   FactionId::HolyOrder, 6,130,15,13,20, 32,12,  0, 0, true,  false, UnitTag::Humanoid|UnitTag::Holy,   2, 9},

    // ── Crimson Wardens ───────────────────────────────────────────────────────
    {"Skeleton",        FactionId::CrimsonWardens, 1,  8, 2, 2, 1,  3, 4, 0,  0, false, false, UnitTag::Undead,  15, 1},
    {"Bone Archer",     FactionId::CrimsonWardens, 2, 14, 4, 3, 3,  6, 5, 5, 12, false, false, UnitTag::Undead,  11, 2},
    {"Wight",           FactionId::CrimsonWardens, 3, 26, 6, 5, 4,  8, 6, 0,  0, false, false, UnitTag::Undead,   7, 3},
    {"Vampire",         FactionId::CrimsonWardens, 4, 45, 9, 6, 8, 14,10, 0,  0, true,  false, UnitTag::Undead,   5, 5},
    {"Lich",            FactionId::CrimsonWardens, 5, 65,12, 7,13, 21, 8, 6,  8, false, false, UnitTag::Undead,   3, 7},
    {"Bone Dragon",     FactionId::CrimsonWardens, 6,155,17,14,22, 38,11, 0,  0, true,  false, UnitTag::Undead,   2, 9},

    // ── Thornkin ──────────────────────────────────────────────────────────────
    {"Sproutling",      FactionId::Thornkin, 1,  9, 2, 3, 1,  3, 3, 0, 0, false, false, UnitTag::Beast,  14, 1},
    {"Briar",           FactionId::Thornkin, 2, 20, 4, 5, 3,  6, 4, 0, 0, false, false, UnitTag::Beast,  10, 2},
    {"Vine Crawler",    FactionId::Thornkin, 3, 34, 7, 6, 5,  9, 5, 0, 0, false, false, UnitTag::Beast,   7, 3},
    {"Grove Guardian",  FactionId::Thornkin, 4, 60, 9, 9, 9, 15, 5, 0, 0, false, false, UnitTag::Beast,   5, 5},
    {"Ancient Oak",     FactionId::Thornkin, 5,100,12,12,13, 22, 6, 0, 0, false, false, UnitTag::Beast,   3, 7},
    {"World Thorn",     FactionId::Thornkin, 6,175,16,16,24, 38, 7, 0, 0, false, false, UnitTag::Beast,   2, 9},

    // ── Eternal Empire ────────────────────────────────────────────────────────
    {"Conscript",       FactionId::EternalEmpire, 1, 10, 2, 3, 1,  3, 4, 0, 0, false, false, UnitTag::Humanoid|UnitTag::Undead,   13, 1},
    {"Revenant",        FactionId::EternalEmpire, 2, 16, 4, 4, 3,  6, 5, 0, 0, false, false, UnitTag::Undead,                     10, 2},
    {"Shade Archer",    FactionId::EternalEmpire, 3, 24, 5, 4, 4,  8, 6, 5, 9, false, false, UnitTag::Undead,                      7, 3},
    {"Steel Guardian",  FactionId::EternalEmpire, 4, 55, 8,10, 8, 14, 6, 0, 0, false, false, UnitTag::Construct|UnitTag::Undead,   5, 5},
    {"Phantom Knight",  FactionId::EternalEmpire, 5, 70,11, 9,11, 20, 8, 0, 0, true,  false, UnitTag::Undead,                      3, 7},
    {"Immortal",        FactionId::EternalEmpire, 6,110,14,12,20, 30,10, 0, 0, true,  true,  UnitTag::Undead,                      2, 9},

    // ── Bloodsworn ────────────────────────────────────────────────────────────
    {"Bloodling",       FactionId::Bloodsworn, 1, 10, 3, 2, 2,  4, 5, 0, 0, false, false, UnitTag::Humanoid|UnitTag::BloodBound,  14, 1},
    {"Berserker",       FactionId::Bloodsworn, 2, 18, 5, 3, 3,  7, 7, 0, 0, false, false, UnitTag::Humanoid|UnitTag::BloodBound,  10, 2},
    {"Blood Shaman",    FactionId::Bloodsworn, 3, 25, 6, 4, 4,  8, 6, 4, 7, false, false, UnitTag::Humanoid|UnitTag::BloodBound,   7, 3},
    {"Ravager",         FactionId::Bloodsworn, 4, 48,10, 5,10, 18, 8, 0, 0, false, false, UnitTag::Humanoid|UnitTag::BloodBound,   5, 5},
    {"Bloodtide Warlord",FactionId::Bloodsworn,5, 72,13, 8,14, 25, 9, 0, 0, false, false, UnitTag::Humanoid|UnitTag::BloodBound,   3, 7},
    {"Crimson Avatar",  FactionId::Bloodsworn, 6,140,18, 9,25, 42,11, 0, 0, false, false, UnitTag::Humanoid|UnitTag::BloodBound,   2, 9},

    // ── Voidkin ───────────────────────────────────────────────────────────────
    {"Void Wisp",       FactionId::Voidkin, 1,  8, 2, 2, 1,  3, 6, 0, 0, true,  false, UnitTag::Void,  13, 1},
    {"Phase Walker",    FactionId::Voidkin, 2, 14, 4, 3, 2,  5, 8, 0, 0, true,  false, UnitTag::Void,  10, 2},
    {"Rift Archer",     FactionId::Voidkin, 3, 24, 6, 5, 4,  8, 7, 5, 9, true,  false, UnitTag::Void,   7, 3},
    {"Void Stalker",    FactionId::Voidkin, 4, 44, 9, 7, 9, 15,10, 0, 0, true,  false, UnitTag::Void,   5, 5},
    {"Entropy Wraith",  FactionId::Voidkin, 5, 65,12, 9,13, 22,12, 0, 0, true,  false, UnitTag::Void,   3, 7},
    {"Void Colossus",   FactionId::Voidkin, 6,120,16,12,22, 34,13, 0, 0, true,  false, UnitTag::Void,   2, 9},

    // ── Iron Assembly ─────────────────────────────────────────────────────────
    {"Automaton",       FactionId::IronAssembly, 1, 12, 2, 4, 1,  3, 3, 4,  6, false, false, UnitTag::Mechanical,  12, 1},
    {"Gun Construct",   FactionId::IronAssembly, 2, 20, 4, 5, 3,  6, 4, 5,  8, false, false, UnitTag::Mechanical,   9, 2},
    {"Steam Walker",    FactionId::IronAssembly, 3, 38, 6, 7, 5, 10, 5, 0,  0, false, false, UnitTag::Mechanical,   6, 3},
    {"Siege Bot",       FactionId::IronAssembly, 4, 65, 9,10, 9, 17, 4, 5,  8, false, false, UnitTag::Mechanical,   5, 5},
    {"Titan Construct", FactionId::IronAssembly, 5, 95,12,13,15, 24, 5, 0,  0, false, false, UnitTag::Mechanical,   3, 7},
    {"Colossus Prime",  FactionId::IronAssembly, 6,190,16,17,26, 42, 6, 0,  0, false, false, UnitTag::Mechanical,   2, 9},

    // ── Amalgamate ────────────────────────────────────────────────────────────
    {"Flesh Crawler",   FactionId::Amalgamate, 1, 11, 2, 3, 1,  4, 4, 0, 0, false, false, UnitTag::OrganicMech,  12, 1},
    {"Graft Soldier",   FactionId::Amalgamate, 2, 21, 4, 4, 3,  6, 5, 0, 0, false, false, UnitTag::OrganicMech,   9, 2},
    {"Bone Machine",    FactionId::Amalgamate, 3, 36, 7, 5, 5, 10, 6, 0, 0, false, false, UnitTag::OrganicMech,   6, 3},
    {"Fleshwork Knight",FactionId::Amalgamate, 4, 62, 9, 7, 9, 16, 7, 0, 0, true,  false, UnitTag::OrganicMech,   4, 5},
    {"Undying Juggernaut",FactionId::Amalgamate,5, 95,12, 9,14, 23, 8, 0, 0, false, false, UnitTag::OrganicMech,   3, 7},
    {"Convergence Spawn",FactionId::Amalgamate,6,165,16,12,23, 37, 9, 0, 0, true,  false, UnitTag::OrganicMech,   2, 9},

    // ── Convergence ───────────────────────────────────────────────────────────
    {"Awakened",        FactionId::Convergence, 1, 10, 3, 3, 2,  4, 5, 0, 0, false, false, UnitTag::Humanoid,  11, 1},
    {"Synthesized",     FactionId::Convergence, 2, 19, 5, 5, 3,  6, 6, 0, 0, false, false, UnitTag::Humanoid,   8, 2},
    {"Harmonized",      FactionId::Convergence, 3, 32, 7, 7, 5, 10, 7, 0, 0, false, false, UnitTag::Humanoid,   6, 3},
    {"Resonant",        FactionId::Convergence, 4, 58,10,10, 9, 16, 8, 0, 0, true,  false, UnitTag::Humanoid,   4, 5},
    {"Transcendent",    FactionId::Convergence, 5, 88,13,13,14, 23,10, 0, 0, true,  false, UnitTag::Humanoid,   3, 7},
    {"Unified Form",    FactionId::Convergence, 6,155,17,17,23, 37,12, 0, 0, true,  false, UnitTag::Humanoid,   2, 9},
};
// clang-format on

static constexpr int SIM_UNIT_COUNT = static_cast<int>(sizeof(SIM_UNITS) / sizeof(SIM_UNITS[0]));
