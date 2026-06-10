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
