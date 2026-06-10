#include "BuildingRegistry.h"
#include <algorithm>

// ── Helper macros for cleaner data entry ──────────────────────────────────────
static Resources gold(int g) { return Resources::gold(g); }
static Resources res(ResourceType t, int v) { return Resources::make(t, v); }

static Resources goldAndRes(int g, ResourceType t, int v) {
    Resources r = Resources::gold(g);
    r.add(t, v);
    return r;
}

void BuildingRegistry::init()
{
    m_buildings.clear();
    m_units.clear();

    // ── SHARED BUILDINGS ──────────────────────────────────────────────────────
    {
        BuildingDef b;
        b.id = BID::FORT; b.name = "Fort";
        b.description = "Adds walls and gate — enables siege defense";
        b.category = BuildingCategory::Fort;
        b.cost = gold(2000);
        m_buildings.push_back(b);
    }
    {
        BuildingDef b;
        b.id = BID::MARKET; b.name = "Market";
        b.description = "Converts resources — +500 Gold weekly";
        b.category = BuildingCategory::Economy;
        b.cost = gold(1000);
        b.weeklyIncome = gold(500);
        m_buildings.push_back(b);
    }
    {
        BuildingDef b;
        b.id = BID::WAREHOUSE; b.name = "Warehouse";
        b.description = "Resource storage — +1 Iron weekly";
        b.category = BuildingCategory::Economy;
        b.cost = gold(500);
        b.weeklyIncome = res(ResourceType::Iron, 1);
        m_buildings.push_back(b);
    }

    // ── HOLY ORDER BUILDINGS ──────────────────────────────────────────────────
    {
        BuildingDef b;
        b.id = BID::HO_HALL; b.name = "Cathedral Hall";
        b.description = "Town Hall — +1000 Gold weekly";
        b.category = BuildingCategory::Economy;
        b.faction = FactionId::HolyOrder;
        b.cost = gold(500);
        b.weeklyIncome = gold(1000);
        m_buildings.push_back(b);
    }

    // T1 — Penitent
    {
        BuildingDef b;
        b.id = BID::HO_T1_BASE; b.name = "Prison Yard";
        b.description = "Produces Penitents each week";
        b.category = BuildingCategory::UnitDwelling;
        b.faction = FactionId::HolyOrder;
        b.tier = 1; b.weeklyGrowth = 14;
        b.cost = gold(300);
        b.upgradeA = BID::HO_T1_A; b.upgradeB = BID::HO_T1_B;
        m_buildings.push_back(b);
    }
    {
        BuildingDef b;
        b.id = BID::HO_T1_A; b.name = "Prison Yard — Fast Death";
        b.description = "Penitents die faster, feed Desperation harder";
        b.category = BuildingCategory::UnitDwelling;
        b.faction = FactionId::HolyOrder;
        b.tier = 1; b.weeklyGrowth = 18; // more units
        b.cost = gold(500);
        b.prerequisites = {BID::HO_T1_BASE};
        m_buildings.push_back(b);
    }
    {
        BuildingDef b;
        b.id = BID::HO_T1_B; b.name = "Prison Yard — Hardened";
        b.description = "Penitents tankier, slower meter feed";
        b.category = BuildingCategory::UnitDwelling;
        b.faction = FactionId::HolyOrder;
        b.tier = 1; b.weeklyGrowth = 12;
        b.cost = gold(500);
        b.prerequisites = {BID::HO_T1_BASE};
        m_buildings.push_back(b);
    }

    // T2 — Torch Bearer
    {
        BuildingDef b;
        b.id = BID::HO_T2_BASE; b.name = "Militia Barracks";
        b.description = "Produces Torch Bearers";
        b.category = BuildingCategory::UnitDwelling;
        b.faction = FactionId::HolyOrder;
        b.tier = 2; b.weeklyGrowth = 10;
        b.cost = goldAndRes(500, ResourceType::Iron, 2);
        b.prerequisites = {BID::HO_T1_BASE};
        b.upgradeA = BID::HO_T2_A; b.upgradeB = BID::HO_T2_B;
        m_buildings.push_back(b);
    }
    {
        BuildingDef b;
        b.id = BID::HO_T2_A; b.name = "Militia Barracks — Arsonist";
        b.description = "Torch Bearers spread fire on death";
        b.category = BuildingCategory::UnitDwelling;
        b.faction = FactionId::HolyOrder;
        b.tier = 2; b.weeklyGrowth = 10;
        b.cost = goldAndRes(700, ResourceType::Iron, 1);
        b.prerequisites = {BID::HO_T2_BASE};
        m_buildings.push_back(b);
    }
    {
        BuildingDef b;
        b.id = BID::HO_T2_B; b.name = "Militia Barracks — Devoted";
        b.description = "Torch Bearers empower nearby units while alive";
        b.category = BuildingCategory::UnitDwelling;
        b.faction = FactionId::HolyOrder;
        b.tier = 2; b.weeklyGrowth = 9;
        b.cost = goldAndRes(700, ResourceType::FaithStones, 1);
        b.prerequisites = {BID::HO_T2_BASE};
        m_buildings.push_back(b);
    }

    // T3 — Plague Doctor
    {
        BuildingDef b;
        b.id = BID::HO_T3_BASE; b.name = "Apothecary";
        b.description = "Produces Plague Doctors";
        b.category = BuildingCategory::UnitDwelling;
        b.faction = FactionId::HolyOrder;
        b.tier = 3; b.weeklyGrowth = 7;
        b.cost = goldAndRes(800, ResourceType::FaithStones, 2);
        b.prerequisites = {BID::HO_T2_BASE};
        b.upgradeA = BID::HO_T3_A; b.upgradeB = BID::HO_T3_B;
        m_buildings.push_back(b);
    }
    {
        BuildingDef b;
        b.id = BID::HO_T3_A; b.name = "Apothecary — Sacrifice";
        b.category = BuildingCategory::UnitDwelling;
        b.faction = FactionId::HolyOrder;
        b.tier = 3; b.weeklyGrowth = 7;
        b.cost = goldAndRes(1000, ResourceType::FaithStones, 1);
        b.prerequisites = {BID::HO_T3_BASE};
        m_buildings.push_back(b);
    }
    {
        BuildingDef b;
        b.id = BID::HO_T3_B; b.name = "Apothecary — Toxic Cloud";
        b.category = BuildingCategory::UnitDwelling;
        b.faction = FactionId::HolyOrder;
        b.tier = 3; b.weeklyGrowth = 6;
        b.cost = goldAndRes(1000, ResourceType::Iron, 2);
        b.prerequisites = {BID::HO_T3_BASE};
        m_buildings.push_back(b);
    }

    // T4 — Penitent Knight
    {
        BuildingDef b;
        b.id = BID::HO_T4_BASE; b.name = "Knight's Penance Hall";
        b.tier = 4; b.weeklyGrowth = 5;
        b.category = BuildingCategory::UnitDwelling;
        b.faction = FactionId::HolyOrder;
        b.cost = goldAndRes(1500, ResourceType::Iron, 4);
        b.prerequisites = {BID::HO_T3_BASE};
        b.upgradeA = BID::HO_T4_A; b.upgradeB = BID::HO_T4_B;
        m_buildings.push_back(b);
    }
    { BuildingDef b; b.id=BID::HO_T4_A; b.name="Knight's Penance — Shield";
      b.tier=4; b.weeklyGrowth=5; b.category=BuildingCategory::UnitDwelling;
      b.faction=FactionId::HolyOrder; b.cost=goldAndRes(2000,ResourceType::Iron,2);
      b.prerequisites={BID::HO_T4_BASE}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::HO_T4_B; b.name="Knight's Penance — Bleed";
      b.tier=4; b.weeklyGrowth=4; b.category=BuildingCategory::UnitDwelling;
      b.faction=FactionId::HolyOrder; b.cost=goldAndRes(2000,ResourceType::BloodEssence,1);
      b.prerequisites={BID::HO_T4_BASE}; m_buildings.push_back(b); }

    // T5 — Seraph
    {
        BuildingDef b;
        b.id = BID::HO_T5_BASE; b.name = "Binding Spire";
        b.tier = 5; b.weeklyGrowth = 3;
        b.category = BuildingCategory::UnitDwelling;
        b.faction = FactionId::HolyOrder;
        b.cost = goldAndRes(3000, ResourceType::FaithStones, 5);
        b.prerequisites = {BID::HO_T4_BASE};
        b.upgradeA = BID::HO_T5_A; b.upgradeB = BID::HO_T5_B;
        m_buildings.push_back(b);
    }
    { BuildingDef b; b.id=BID::HO_T5_A; b.name="Binding Spire — Wide Aura";
      b.tier=5; b.weeklyGrowth=3; b.category=BuildingCategory::UnitDwelling;
      b.faction=FactionId::HolyOrder; b.cost=goldAndRes(4000,ResourceType::FaithStones,3);
      b.prerequisites={BID::HO_T5_BASE}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::HO_T5_B; b.name="Binding Spire — Unchained";
      b.tier=5; b.weeklyGrowth=2; b.category=BuildingCategory::UnitDwelling;
      b.faction=FactionId::HolyOrder; b.cost=goldAndRes(4000,ResourceType::FaithStones,4);
      b.prerequisites={BID::HO_T5_BASE}; m_buildings.push_back(b); }

    // T6 — Winged Hussar
    {
        BuildingDef b;
        b.id = BID::HO_T6_BASE; b.name = "Hussar Sanctum";
        b.tier = 6; b.weeklyGrowth = 1;
        b.category = BuildingCategory::UnitDwelling;
        b.faction = FactionId::HolyOrder;
        b.cost = goldAndRes(6000, ResourceType::FaithStones, 8);
        b.prerequisites = {BID::HO_T5_BASE};
        b.upgradeA = BID::HO_T6_A; b.upgradeB = BID::HO_T6_B;
        m_buildings.push_back(b);
    }
    { BuildingDef b; b.id=BID::HO_T6_A; b.name="Hussar Sanctum — Desperation";
      b.tier=6; b.weeklyGrowth=1; b.category=BuildingCategory::UnitDwelling;
      b.faction=FactionId::HolyOrder; b.cost=goldAndRes(8000,ResourceType::FaithStones,6);
      b.prerequisites={BID::HO_T6_BASE}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::HO_T6_B; b.name="Hussar Sanctum — Both Meters";
      b.tier=6; b.weeklyGrowth=1; b.category=BuildingCategory::UnitDwelling;
      b.faction=FactionId::HolyOrder; b.cost=goldAndRes(8000,ResourceType::FaithStones,8);
      b.prerequisites={BID::HO_T6_BASE}; m_buildings.push_back(b); }

    // Support buildings
    { BuildingDef b; b.id=BID::HO_LIGHT_SHRINE; b.name="Light Shrine";
      b.description="+2 Light Power for all heroes garrisoned here";
      b.category=BuildingCategory::Support; b.faction=FactionId::HolyOrder;
      b.cost=goldAndRes(1500,ResourceType::FaithStones,3);
      b.prerequisites={BID::HO_HALL}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::HO_RELIQUARY; b.name="Reliquary";
      b.description="Desperation meter charges 25% faster in combat";
      b.category=BuildingCategory::Support; b.faction=FactionId::HolyOrder;
      b.cost=goldAndRes(2000,ResourceType::FaithStones,4);
      b.prerequisites={BID::HO_LIGHT_SHRINE}; m_buildings.push_back(b); }

    // ── HOLY ORDER UNITS ──────────────────────────────────────────────────────
    auto addUnit = [&](int id, const char* name, FactionId f, int tier,
                       UpgradePath path, int hp, int atk, int def,
                       int dmin, int dmax, int spd, Resources cost,
                       UnitTag tags, bool flying = false) {
        UnitDef u;
        u.id=id; u.name=name; u.faction=f; u.tier=tier; u.path=path;
        u.hp=hp; u.attack=atk; u.defense=def;
        u.damage_min=dmin; u.damage_max=dmax; u.speed=spd;
        u.cost=cost; u.tags=tags; u.flying=flying;
        m_units.push_back(u);
    };

    using F = FactionId;
    using P = UpgradePath;
    using T = UnitTag;

    // T1 Penitent
    addUnit(1001,"Penitent",       F::HolyOrder,1,P::None,  8,2,1,1,3,6, gold(30),  T::Humanoid);
    addUnit(1002,"Penitent(A)",    F::HolyOrder,1,P::PathA, 7,2,1,1,4,7, gold(30),  T::Humanoid);
    addUnit(1003,"Penitent(B)",    F::HolyOrder,1,P::PathB,12,2,2,1,2,4, gold(35),  T::Humanoid);
    // T2 Torch Bearer
    addUnit(1004,"Torch Bearer",   F::HolyOrder,2,P::None, 15,4,3,2,5,5, goldAndRes(80,ResourceType::Iron,1), T::Humanoid);
    addUnit(1005,"Torch Bearer(A)",F::HolyOrder,2,P::PathA,13,4,3,3,6,5, goldAndRes(90,ResourceType::Iron,1), T::Humanoid);
    addUnit(1006,"Torch Bearer(B)",F::HolyOrder,2,P::PathB,16,5,3,2,4,5, goldAndRes(90,ResourceType::FaithStones,1), T::Humanoid|T::Holy);
    // T3 Plague Doctor
    addUnit(1007,"Plague Doctor",  F::HolyOrder,3,P::None, 20,5,5,3,6,4, goldAndRes(150,ResourceType::FaithStones,1), T::Humanoid);
    addUnit(1008,"Plague Doctor(A)",F::HolyOrder,3,P::PathA,18,4,4,2,5,4, goldAndRes(170,ResourceType::FaithStones,1), T::Humanoid);
    addUnit(1009,"Plague Doctor(B)",F::HolyOrder,3,P::PathB,22,5,5,4,7,4, goldAndRes(160,ResourceType::Iron,1), T::Humanoid);
    // T4 Penitent Knight
    addUnit(1010,"Penitent Knight",F::HolyOrder,4,P::None, 40,8,7,5,10,5, goldAndRes(300,ResourceType::Iron,2), T::Humanoid);
    addUnit(1011,"Penitent Knight(A)",F::HolyOrder,4,P::PathA,45,8,8,5,10,5, goldAndRes(350,ResourceType::Iron,2), T::Humanoid|T::Holy);
    addUnit(1012,"Penitent Knight(B)",F::HolyOrder,4,P::PathB,38,9,6,6,11,5, goldAndRes(330,ResourceType::BloodEssence,1), T::Humanoid|T::BloodBound);
    // T5 Seraph
    addUnit(1013,"Seraph",         F::HolyOrder,5,P::None, 65,12,10,8,15,7, goldAndRes(700,ResourceType::FaithStones,3), T::Humanoid|T::Holy|T::Flying, true);
    addUnit(1014,"Seraph(A)",      F::HolyOrder,5,P::PathA,65,12,10,8,15,7, goldAndRes(800,ResourceType::FaithStones,3), T::Humanoid|T::Holy|T::Flying, true);
    addUnit(1015,"Seraph(B)",      F::HolyOrder,5,P::PathB,60,14,9,10,17,9, goldAndRes(750,ResourceType::FaithStones,4), T::Humanoid|T::Holy|T::Flying, true);
    // T6 Winged Hussar
    addUnit(1016,"Winged Hussar",  F::HolyOrder,6,P::None, 90,15,12,12,20,8, goldAndRes(1500,ResourceType::FaithStones,5), T::Humanoid|T::Holy|T::Flying, true);
    addUnit(1017,"Winged Hussar(A)",F::HolyOrder,6,P::PathA,90,15,12,12,20,8, goldAndRes(1800,ResourceType::FaithStones,6), T::Humanoid|T::Holy|T::Flying, true);
    addUnit(1018,"Winged Hussar(B)",F::HolyOrder,6,P::PathB,90,15,12,12,20,8, goldAndRes(1800,ResourceType::FaithStones,8), T::Humanoid|T::Holy|T::Flying, true);
}

const BuildingDef* BuildingRegistry::getBuildingDef(int id) const {
    for (auto& b : m_buildings) if (b.id == id) return &b;
    return nullptr;
}

const UnitDef* BuildingRegistry::getUnitDef(int id) const {
    for (auto& u : m_units) if (u.id == id) return &u;
    return nullptr;
}

std::vector<const BuildingDef*> BuildingRegistry::getBuildingsForFaction(FactionId f) const {
    std::vector<const BuildingDef*> result;
    for (auto& b : m_buildings)
        if (b.faction == f || b.faction == FactionId::None)
            result.push_back(&b);
    return result;
}
