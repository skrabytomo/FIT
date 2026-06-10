#pragma once
#include "Skills.h"

// Static table of all skill definitions.
// Index with findSkillDef(SID::xxx).

// clang-format off
static const SkillDef SKILL_DEFS[] = {
    // id                    name               description                                    faction              effectType                          values          statName        prereq
    {SID::OFFENSE,      "Offense",      "Units deal +1/2/3 attack",                    FactionId::None,  SkillEffectType::UnitStatBonus,   {1,2,3}, "attack",      0},
    {SID::DEFENSE_SKILL,"Defense",      "Units gain +1/2/3 defense",                   FactionId::None,  SkillEffectType::UnitStatBonus,   {1,2,3}, "defense",     0},
    {SID::ARCHERY,      "Archery",      "Ranged units deal +1/2/2 attack",             FactionId::None,  SkillEffectType::UnitStatBonus,   {1,2,2}, "attack",      0},
    {SID::LEADERSHIP,   "Leadership",   "Units start with +10/20/30 morale",           FactionId::None,  SkillEffectType::UnitStatBonus,   {10,20,30},"morale",    0},
    {SID::TACTICS,      "Tactics",      "Hero attack/defense bonus +1/2/3",            FactionId::None,  SkillEffectType::StatBonus,       {1,2,3}, "attack",      0},
    {SID::LOGISTICS,    "Logistics",    "+3/5/7 movement pool",                        FactionId::None,  SkillEffectType::MovementBonus,   {3,5,7}, "",            0},
    {SID::SCOUTING,     "Scouting",     "+1/2/3 vision range",                         FactionId::None,  SkillEffectType::VisionBonus,     {1,2,3}, "",            0},
    {SID::FIRST_AID,    "First Aid",    "Restore 10/20/30% unit HP after battle",      FactionId::None,  SkillEffectType::StatBonus,       {10,20,30},"heal",      0},

    {SID::LIGHT_MAGIC,  "Light Magic",  "+2/4/6 Light Power",                          FactionId::None,  SkillEffectType::MagicSchoolBonus,{2,4,6}, "lightPower",  0},
    {SID::BLOOD_MAGIC,  "Blood Magic",  "+2/4/6 Blood Power",                          FactionId::None,  SkillEffectType::MagicSchoolBonus,{2,4,6}, "bloodPower",  0},
    {SID::DEATH_MAGIC,  "Death Magic",  "+2/4/6 Death Power",                          FactionId::None,  SkillEffectType::MagicSchoolBonus,{2,4,6}, "deathPower",  0},
    {SID::NATURE_MAGIC, "Nature Magic", "+2/4/6 Nature Power",                         FactionId::None,  SkillEffectType::MagicSchoolBonus,{2,4,6}, "naturePower", 0},
    {SID::FORGE_MAGIC,  "Forge Magic",  "+2/4/6 Forge Power",                          FactionId::None,  SkillEffectType::MagicSchoolBonus,{2,4,6}, "forgePower",  0},
    {SID::FLESH_MAGIC,  "Flesh Magic",  "+2/4/6 Flesh Power",                          FactionId::None,  SkillEffectType::MagicSchoolBonus,{2,4,6}, "fleshPower",  0},

    {SID::DESPERATION,  "Desperation",  "Desperation meter charges 10/20/30% faster",  FactionId::HolyOrder,      SkillEffectType::SpecialMechanic, {10,20,30},"",  0},
    {SID::INSPIRATION,  "Inspiration",  "Morale aura radius +1/2/3",                   FactionId::HolyOrder,      SkillEffectType::SpecialMechanic, {1,2,3},  "",   0},
    {SID::BLOOD_POOL,   "Blood Pool",   "Blood Pool fills 25/50/100% faster",           FactionId::Bloodsworn,     SkillEffectType::SpecialMechanic, {25,50,100},"", 0},
    {SID::POSSESSION,   "Possession",   "Possession lasts +50/100/150% longer",         FactionId::Voidkin,        SkillEffectType::SpecialMechanic, {50,100,150},"",0},
    {SID::BLUEPRINT,    "Blueprint",    "Constructs unlocked 1/2/3 weeks earlier",      FactionId::IronAssembly,   SkillEffectType::SpecialMechanic, {1,2,3},  "",   0},
    {SID::ADAPTATION,   "Adaptation",   "Amalgamate traits gained 10/20/30% faster",    FactionId::Amalgamate,     SkillEffectType::SpecialMechanic, {10,20,30},"",  0},
    {SID::ETERNAL_CMD,  "Eternal Command","Reraised units 10/20/30% stronger",          FactionId::EternalEmpire,  SkillEffectType::SpecialMechanic, {10,20,30},"",  0},
    {SID::SYMBIOSIS,    "Symbiosis",    "Bond pairs gain +1/2/3 to all stats",          FactionId::Thornkin,       SkillEffectType::SpecialMechanic, {1,2,3},  "",   0},
    {SID::WARDEN_MARK,  "Warden's Mark","Mark hits 1/2/3 additional targets",           FactionId::CrimsonWardens, SkillEffectType::SpecialMechanic, {1,2,3},  "",   0},
    {SID::MIRRORING,    "Mirroring",    "Mirror lasts +50/100/150% longer",             FactionId::Convergence,    SkillEffectType::SpecialMechanic, {50,100,150},"",0},
};
// clang-format on

static constexpr int SKILL_DEF_COUNT = static_cast<int>(sizeof(SKILL_DEFS) / sizeof(SKILL_DEFS[0]));

inline const SkillDef* findSkillDef(int id)
{
    for (int i = 0; i < SKILL_DEF_COUNT; ++i)
        if (SKILL_DEFS[i].id == id) return &SKILL_DEFS[i];
    return nullptr;
}
