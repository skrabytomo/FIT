#pragma once
#include <vector>
#include "BuildingDef.h"
#include "UnitDef.h"

// Building IDs — unique across all factions
// 1xx = Holy Order, 2xx = Crimson Wardens, 3xx = Thornkin
// 4xx = Eternal Empire, 5xx = Bloodsworn, 6xx = Voidkin
// 7xx = Iron Assembly, 8xx = Amalgamate, 9xx = Convergence
// 001-009 = shared (fort, market, etc.)

namespace BID {
    // Shared
    constexpr int FORT         = 1;
    constexpr int MARKET       = 2;
    constexpr int WAREHOUSE    = 3;
    constexpr int ROAD         = 4;
    constexpr int MAGE_GUILD    = 5;   // T1: 2 spells
    constexpr int MAGE_GUILD_T2 = 6;  // T2: 4 spells
    constexpr int WAREHOUSE_T2  = 7;  // +4 Iron/wk
    constexpr int WAREHOUSE_T3  = 8;  // +6 Iron/wk
    constexpr int MAGE_GUILD_T3 = 9;  // T3: all 4 spells at 30% discount
    constexpr int MAGE_GUILD_T4 = 10; // T4: all 4 spells at 50% discount (magic factions)
    constexpr int TOWN_HALL     = 11; // +1500 Gold/wk; requires Market
    constexpr int CITY_HALL     = 12; // +3500 Gold/wk; requires Town Hall, unlocks at week 3

    // Holy Order
    constexpr int HO_HALL         = 100; // Town Hall (base income)
    constexpr int HO_T1_BASE      = 101; // Penitent dwelling
    constexpr int HO_T1_A         = 102; // Path A upgrade
    constexpr int HO_T1_B         = 103; // Path B upgrade
    constexpr int HO_T2_BASE      = 104; // Torch Bearer
    constexpr int HO_T2_A         = 105;
    constexpr int HO_T2_B         = 106;
    constexpr int HO_T3_BASE      = 107; // Plague Doctor
    constexpr int HO_T3_A         = 108;
    constexpr int HO_T3_B         = 109;
    constexpr int HO_T4_BASE      = 110; // Penitent Knight
    constexpr int HO_T4_A         = 111;
    constexpr int HO_T4_B         = 112;
    constexpr int HO_T5_BASE      = 113; // Seraph
    constexpr int HO_T5_A         = 114;
    constexpr int HO_T5_B         = 115;
    constexpr int HO_T6_BASE      = 116; // Winged Hussar
    constexpr int HO_T6_A         = 117;
    constexpr int HO_T6_B         = 118;
    constexpr int HO_LIGHT_SHRINE = 119; // +Light Power
    constexpr int HO_RELIQUARY    = 120; // +Desperation meter speed

    // Crimson Wardens (2xx)
    constexpr int CW_HALL         = 200;
    constexpr int CW_T1           = 201; // Skeleton
    constexpr int CW_T2           = 202; // Bone Archer
    constexpr int CW_T3           = 203; // Wight
    constexpr int CW_T4           = 204; // Vampire
    constexpr int CW_T5           = 205; // Lich
    constexpr int CW_T6           = 206; // Bone Dragon
    constexpr int CW_DEATH_ALTAR  = 207; // +Death Power
    constexpr int CW_WARDEN_BRAND = 208; // Warden's Mark support

    // Thornkin (3xx)
    constexpr int TK_GROVE_HEART  = 300;
    constexpr int TK_T1           = 301; // Sproutling
    constexpr int TK_T2           = 302; // Briar
    constexpr int TK_T3           = 303; // Vine Crawler
    constexpr int TK_T4           = 304; // Grove Guardian
    constexpr int TK_T5           = 305; // Ancient Oak
    constexpr int TK_T6           = 306; // World Thorn
    constexpr int TK_ANCIENT_CIRCLE = 307; // +Nature Power
    constexpr int TK_SYMBIOSIS_WEB = 308; // Symbiosis bond support

    // Eternal Empire (4xx)
    constexpr int EE_THRONE       = 400;
    constexpr int EE_T1           = 401; // Conscript
    constexpr int EE_T2           = 402; // Revenant
    constexpr int EE_T3           = 403; // Shade Archer
    constexpr int EE_T4           = 404; // Steel Guardian
    constexpr int EE_T5           = 405; // Phantom Knight
    constexpr int EE_T6           = 406; // Immortal
    constexpr int EE_NECROPOLIS   = 407; // +Death Power
    constexpr int EE_MONUMENT     = 408; // Eternal Command support

    // Bloodsworn (5xx)
    constexpr int BS_WAR_HALL     = 500;
    constexpr int BS_T1           = 501; // Bloodling
    constexpr int BS_T2           = 502; // Berserker
    constexpr int BS_T3           = 503; // Blood Shaman
    constexpr int BS_T4           = 504; // Ravager
    constexpr int BS_T5           = 505; // Bloodtide Warlord
    constexpr int BS_T6           = 506; // Crimson Avatar
    constexpr int BS_BLOOD_ALTAR  = 507; // +Blood Power
    constexpr int BS_WAR_SHRINE   = 508; // Blood Pool faster

    // Voidkin (6xx)
    constexpr int VK_NEXUS        = 600;
    constexpr int VK_T1           = 601; // Void Wisp
    constexpr int VK_T2           = 602; // Phase Walker
    constexpr int VK_T3           = 603; // Rift Archer
    constexpr int VK_T4           = 604; // Void Stalker
    constexpr int VK_T5           = 605; // Entropy Wraith
    constexpr int VK_T6           = 606; // Void Colossus
    constexpr int VK_RIFT_GATE    = 607; // +Nature Power (void resonance)
    constexpr int VK_VOID_LENS    = 608; // Possession duration

    // Iron Assembly (7xx)
    constexpr int IA_FORGE_HALL   = 700;
    constexpr int IA_T1           = 701; // Automaton
    constexpr int IA_T2           = 702; // Gun Construct
    constexpr int IA_T3           = 703; // Steam Walker
    constexpr int IA_T4           = 704; // Siege Bot
    constexpr int IA_T5           = 705; // Titan Construct
    constexpr int IA_T6           = 706; // Colossus Prime
    constexpr int IA_BLUEPRINT_VAULT = 707; // +Forge Power
    constexpr int IA_OVERCLOCK    = 708; // Construct speed support

    // Amalgamate (8xx)
    constexpr int AM_GRAFTING_HALL = 800;
    constexpr int AM_T1           = 801; // Flesh Crawler
    constexpr int AM_T2           = 802; // Graft Soldier
    constexpr int AM_T3           = 803; // Bone Machine
    constexpr int AM_T4           = 804; // Fleshwork Knight
    constexpr int AM_T5           = 805; // Undying Juggernaut
    constexpr int AM_T6           = 806; // Convergence Spawn
    constexpr int AM_FLESH_VAULT  = 807; // +Flesh Power
    constexpr int AM_MERGE_CHAMBER = 808; // Adaptation support

    // Convergence (9xx)
    constexpr int CV_SYNTHESIS_HUB = 900;
    constexpr int CV_T1           = 901; // Awakened
    constexpr int CV_T2           = 902; // Synthesized
    constexpr int CV_T3           = 903; // Harmonized
    constexpr int CV_T4           = 904; // Resonant
    constexpr int CV_T5           = 905; // Transcendent
    constexpr int CV_T6           = 906; // Unified Form
    constexpr int CV_RESONANCE_WELL = 907; // Mirroring support
    constexpr int CV_MIRROR_CHAMBER = 908; // Mirror duration
}

class BuildingRegistry
{
public:
    void init(); // populate all definitions

    const BuildingDef* getBuildingDef(int id) const;
    const UnitDef*     getUnitDef(int id)     const;

    // Get all buildings for a faction
    std::vector<const BuildingDef*> getBuildingsForFaction(FactionId f) const;

    const std::vector<BuildingDef>& buildings() const { return m_buildings; }
    const std::vector<UnitDef>&     units()     const { return m_units; }

private:
    std::vector<BuildingDef> m_buildings;
    std::vector<UnitDef>     m_units;
};
