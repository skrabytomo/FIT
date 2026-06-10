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
static constexpr UnitSimData SIM_UNITS[] = {
    // ── Holy Order ────────────────────────────────────────────────────────────
    {"Penitent",        FactionId::HolyOrder, 1, 8,  2,  2, 1,  3, 4,  0, 0, false, false, UnitTag::Humanoid|UnitTag::Holy,  14, 1},
    {"Torch Bearer",    FactionId::HolyOrder, 2, 15, 4,  3, 2,  4, 5,  0, 0, false, false, UnitTag::Humanoid|UnitTag::Holy,  10, 2},
    {"Plague Doctor",   FactionId::HolyOrder, 3, 25, 5,  5, 3,  6, 6,  4, 8, false, false, UnitTag::Humanoid|UnitTag::Holy,   7, 3},
    {"Penitent Knight", FactionId::HolyOrder, 4, 45, 7,  7, 6, 12, 7,  0, 0, true,  false, UnitTag::Humanoid|UnitTag::Holy,   5, 5},
    {"Seraph",          FactionId::HolyOrder, 5, 70, 10, 9,10, 18, 9,  0, 0, true,  false, UnitTag::Humanoid|UnitTag::Holy,   3, 7},
    {"Winged Hussar",   FactionId::HolyOrder, 6,120,14, 12,18, 30,12,  0, 0, true,  false, UnitTag::Humanoid|UnitTag::Holy,   2, 9},

    // ── Crimson Wardens ───────────────────────────────────────────────────────
    {"Skeleton",        FactionId::CrimsonWardens, 1,  6, 2, 1, 1,  2, 4, 0,  0, false, false, UnitTag::Undead,  15, 1},
    {"Bone Archer",     FactionId::CrimsonWardens, 2, 10, 3, 2, 2,  4, 5, 5, 10, false, false, UnitTag::Undead,  11, 2},
    {"Wight",           FactionId::CrimsonWardens, 3, 22, 5, 4, 3,  7, 6, 0,  0, false, false, UnitTag::Undead,   7, 3},
    {"Vampire",         FactionId::CrimsonWardens, 4, 40, 8, 5, 7, 13, 9, 0,  0, true,  false, UnitTag::Undead,   5, 5},
    {"Lich",            FactionId::CrimsonWardens, 5, 60,11, 6,12, 20, 8, 6, 12, false, false, UnitTag::Undead,   3, 7},
    {"Bone Dragon",     FactionId::CrimsonWardens, 6,140,16,13,20, 35,11, 0,  0, true,  false, UnitTag::Undead,   2, 9},

    // ── Thornkin ──────────────────────────────────────────────────────────────
    {"Sproutling",      FactionId::Thornkin, 1,  7, 1, 2, 1,  2, 3, 0, 0, false, false, UnitTag::Beast,  14, 1},
    {"Briar",           FactionId::Thornkin, 2, 18, 3, 4, 2,  4, 4, 0, 0, false, false, UnitTag::Beast,  10, 2},
    {"Vine Crawler",    FactionId::Thornkin, 3, 30, 6, 5, 4,  8, 5, 0, 0, false, false, UnitTag::Beast,   7, 3},
    {"Grove Guardian",  FactionId::Thornkin, 4, 55, 8, 8, 8, 14, 5, 0, 0, false, false, UnitTag::Beast,   5, 5},
    {"Ancient Oak",     FactionId::Thornkin, 5, 90,11,11,12, 20, 6, 0, 0, false, false, UnitTag::Beast,   3, 7},
    {"World Thorn",     FactionId::Thornkin, 6,160,15,15,22, 35, 7, 0, 0, false, false, UnitTag::Beast,   2, 9},

    // ── Eternal Empire ────────────────────────────────────────────────────────
    {"Conscript",       FactionId::EternalEmpire, 1,  8, 2, 2, 1,  3, 4, 0, 0, false, false, UnitTag::Humanoid|UnitTag::Undead,   13, 1},
    {"Revenant",        FactionId::EternalEmpire, 2, 14, 4, 3, 2,  5, 5, 0, 0, false, false, UnitTag::Undead,                     10, 2},
    {"Shade Archer",    FactionId::EternalEmpire, 3, 20, 5, 3, 4,  7, 6, 5, 8, false, false, UnitTag::Undead,                      7, 3},
    {"Steel Guardian",  FactionId::EternalEmpire, 4, 50, 7, 9, 7, 13, 6, 0, 0, false, false, UnitTag::Construct|UnitTag::Undead,   5, 5},
    {"Phantom Knight",  FactionId::EternalEmpire, 5, 65,10, 8,10, 18, 8, 0, 0, true,  false, UnitTag::Undead,                      3, 7},
    {"Immortal",        FactionId::EternalEmpire, 6,100,13,11,18, 28,10, 0, 0, true,  true,  UnitTag::Undead,                      2, 9},

    // ── Bloodsworn ────────────────────────────────────────────────────────────
    {"Bloodling",       FactionId::Bloodsworn, 1,  9, 3, 1, 2,  4, 5, 0, 0, false, false, UnitTag::Humanoid|UnitTag::BloodBound,  14, 1},
    {"Berserker",       FactionId::Bloodsworn, 2, 16, 5, 2, 3,  7, 7, 0, 0, false, false, UnitTag::Humanoid|UnitTag::BloodBound,  10, 2},
    {"Blood Shaman",    FactionId::Bloodsworn, 3, 22, 6, 3, 5,  9, 6, 4, 6, false, false, UnitTag::Humanoid|UnitTag::BloodBound,   7, 3},
    {"Ravager",         FactionId::Bloodsworn, 4, 45,10, 4, 9, 18, 8, 0, 0, false, false, UnitTag::Humanoid|UnitTag::BloodBound,   5, 5},
    {"Bloodtide Warlord",FactionId::Bloodsworn,5, 70,13, 7,14, 24, 9, 0, 0, false, false, UnitTag::Humanoid|UnitTag::BloodBound,   3, 7},
    {"Crimson Avatar",  FactionId::Bloodsworn, 6,130,18, 8,24, 40,11, 0, 0, false, false, UnitTag::Humanoid|UnitTag::BloodBound,   2, 9},

    // ── Voidkin ───────────────────────────────────────────────────────────────
    {"Void Wisp",       FactionId::Voidkin, 1,  6, 2, 2, 1,  3, 6, 0, 0, true,  false, UnitTag::Void,  13, 1},
    {"Phase Walker",    FactionId::Voidkin, 2, 12, 4, 3, 2,  5, 8, 0, 0, true,  false, UnitTag::Void,  10, 2},
    {"Rift Archer",     FactionId::Voidkin, 3, 20, 6, 4, 4,  8, 7, 5, 8, true,  false, UnitTag::Void,   7, 3},
    {"Void Stalker",    FactionId::Voidkin, 4, 40, 8, 6, 8, 14,10, 0, 0, true,  false, UnitTag::Void,   5, 5},
    {"Entropy Wraith",  FactionId::Voidkin, 5, 60,11, 8,12, 20,12, 0, 0, true,  false, UnitTag::Void,   3, 7},
    {"Void Colossus",   FactionId::Voidkin, 6,110,15,11,20, 32,13, 0, 0, true,  false, UnitTag::Void,   2, 9},

    // ── Iron Assembly ─────────────────────────────────────────────────────────
    {"Automaton",       FactionId::IronAssembly, 1, 10, 2, 3, 1,  3, 3, 4, 10, false, false, UnitTag::Mechanical,  12, 1},
    {"Gun Construct",   FactionId::IronAssembly, 2, 18, 4, 4, 3,  6, 4, 5, 12, false, false, UnitTag::Mechanical,   9, 2},
    {"Steam Walker",    FactionId::IronAssembly, 3, 35, 6, 6, 5,  9, 5, 0,  0, false, false, UnitTag::Mechanical,   6, 3},
    {"Siege Bot",       FactionId::IronAssembly, 4, 60, 9, 9, 9, 16, 4, 6,  8, false, false, UnitTag::Mechanical,   5, 5},
    {"Titan Construct", FactionId::IronAssembly, 5, 90,12,12,14, 22, 5, 0,  0, false, false, UnitTag::Mechanical,   3, 7},
    {"Colossus Prime",  FactionId::IronAssembly, 6,180,16,16,25, 40, 6, 0,  0, false, false, UnitTag::Mechanical,   2, 9},

    // ── Amalgamate ────────────────────────────────────────────────────────────
    {"Flesh Crawler",   FactionId::Amalgamate, 1, 10, 2, 2, 1,  4, 4, 0, 0, false, false, UnitTag::OrganicMech,  13, 1},
    {"Graft Soldier",   FactionId::Amalgamate, 2, 20, 4, 4, 3,  6, 5, 0, 0, false, false, UnitTag::OrganicMech,   9, 2},
    {"Bone Machine",    FactionId::Amalgamate, 3, 38, 7, 5, 5, 10, 6, 0, 0, false, false, UnitTag::OrganicMech,   6, 3},
    {"Fleshwork Knight",FactionId::Amalgamate, 4, 65, 9, 7, 9, 17, 7, 0, 0, true,  false, UnitTag::OrganicMech,   5, 5},
    {"Undying Juggernaut",FactionId::Amalgamate,5,100,12, 9,14, 24, 8, 0, 0, false, false, UnitTag::OrganicMech,   3, 7},
    {"Convergence Spawn",FactionId::Amalgamate,6,170,16,12,24, 38, 9, 0, 0, true,  false, UnitTag::OrganicMech,   2, 9},

    // ── Convergence ───────────────────────────────────────────────────────────
    {"Awakened",        FactionId::Convergence, 1,  9, 3, 3, 2,  4, 5, 0, 0, false, false, UnitTag::Humanoid,  11, 1},
    {"Synthesized",     FactionId::Convergence, 2, 18, 5, 5, 3,  6, 6, 0, 0, false, false, UnitTag::Humanoid,   8, 2},
    {"Harmonized",      FactionId::Convergence, 3, 30, 7, 7, 5, 10, 7, 0, 0, false, false, UnitTag::Humanoid,   6, 3},
    {"Resonant",        FactionId::Convergence, 4, 55,10,10, 9, 16, 8, 0, 0, true,  false, UnitTag::Humanoid,   4, 5},
    {"Transcendent",    FactionId::Convergence, 5, 85,13,13,13, 22,10, 0, 0, true,  false, UnitTag::Humanoid,   3, 7},
    {"Unified Form",    FactionId::Convergence, 6,150,17,17,22, 36,12, 0, 0, true,  false, UnitTag::Humanoid,   2, 9},
};
// clang-format on

static constexpr int SIM_UNIT_COUNT = static_cast<int>(sizeof(SIM_UNITS) / sizeof(SIM_UNITS[0]));
