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
        b.description = "Converts resources — +500 Gold weekly, +1 unit growth";
        b.category = BuildingCategory::Economy;
        b.cost = gold(1000);
        b.weeklyIncome = gold(500);
        b.growthBonus = 1;
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
    {
        BuildingDef b;
        b.id = BID::MAGE_GUILD; b.name = "Mage Guild";
        b.description = "Teaches 2 faction spells (Tier 1). Upgrade for 4 spells (Tier 2).";
        b.category = BuildingCategory::MageGuild;
        b.cost = gold(2000);
        m_buildings.push_back(b);
    }
    {
        BuildingDef b;
        b.id = BID::MAGE_GUILD_T2; b.name = "Mage Guild (Tier 2)";
        b.description = "Unlocks 2 additional faction spells for purchase (spells 3 & 4).";
        b.category = BuildingCategory::MageGuild;
        b.cost = gold(3000);
        b.prerequisites = {BID::MAGE_GUILD};
        m_buildings.push_back(b);
    }

    // ── HOLY ORDER BUILDINGS ──────────────────────────────────────────────────
    {
        BuildingDef b;
        b.id = BID::HO_HALL; b.name = "Cathedral Hall";
        b.description = "Town Hall — +1000 Gold weekly, +2 unit growth";
        b.category = BuildingCategory::Economy;
        b.faction = FactionId::HolyOrder;
        b.cost = gold(500);
        b.weeklyIncome = gold(1000);
        b.growthBonus = 2;
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
      b.description="Holy units start battle with +20 Desperation pre-charged";
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

    // ── CRIMSON WARDENS ───────────────────────────────────────────────────────
    { BuildingDef b; b.id=BID::CW_HALL; b.name="Catacombs Throne";
      b.description="Town Hall — +1000 Gold weekly, +2 unit growth"; b.category=BuildingCategory::Economy;
      b.faction=F::CrimsonWardens; b.cost=gold(500); b.weeklyIncome=gold(1000); b.growthBonus=2;
      m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::CW_T1; b.name="Ossuary"; b.tier=1; b.weeklyGrowth=15;
      b.description="Produces Skeletons"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::CrimsonWardens; b.cost=gold(300); m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::CW_T2; b.name="Archer Crypt"; b.tier=2; b.weeklyGrowth=11;
      b.description="Produces Bone Archers"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::CrimsonWardens; b.cost=goldAndRes(600,ResourceType::FaithStones,1);
      b.prerequisites={BID::CW_T1}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::CW_T3; b.name="Shade Hollow"; b.tier=3; b.weeklyGrowth=7;
      b.description="Produces Wights"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::CrimsonWardens; b.cost=goldAndRes(1000,ResourceType::FaithStones,2);
      b.prerequisites={BID::CW_T2}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::CW_T4; b.name="Blood Roost"; b.tier=4; b.weeklyGrowth=5;
      b.description="Produces Vampires"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::CrimsonWardens; b.cost=goldAndRes(1800,ResourceType::FaithStones,3);
      b.prerequisites={BID::CW_T3}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::CW_T5; b.name="Lich Spire"; b.tier=5; b.weeklyGrowth=3;
      b.description="Produces Liches"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::CrimsonWardens; b.cost=goldAndRes(3000,ResourceType::FaithStones,4);
      b.prerequisites={BID::CW_T4}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::CW_T6; b.name="Dragon Crypts"; b.tier=6; b.weeklyGrowth=2;
      b.description="Produces Bone Dragons"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::CrimsonWardens; b.cost=goldAndRes(5500,ResourceType::FaithStones,6);
      b.prerequisites={BID::CW_T5}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::CW_DEATH_ALTAR; b.name="Death Altar";
      b.description="+3 Death Power for all heroes garrisoned here";
      b.category=BuildingCategory::Support; b.faction=F::CrimsonWardens;
      b.cost=goldAndRes(1500,ResourceType::FaithStones,3);
      b.prerequisites={BID::CW_HALL}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::CW_WARDEN_BRAND; b.name="Warden's Brand Chamber";
      b.description="Warden's Mark affects one additional target";
      b.category=BuildingCategory::Support; b.faction=F::CrimsonWardens;
      b.cost=goldAndRes(2500,ResourceType::FaithStones,4);
      b.prerequisites={BID::CW_DEATH_ALTAR}; m_buildings.push_back(b); }

    addUnit(2001,"Skeleton",    F::CrimsonWardens,1,P::None,  6,2,1,1, 2,4, gold(40),        T::Undead);
    addUnit(2002,"Bone Archer", F::CrimsonWardens,2,P::None, 10,3,2,2, 4,5, gold(90),        T::Undead);
    addUnit(2003,"Wight",       F::CrimsonWardens,3,P::None, 22,5,4,3, 7,6, goldAndRes(180,ResourceType::FaithStones,1), T::Undead);
    m_units.back().regenerates = true;   // Wight regenerates full HP at start of turn
    addUnit(2004,"Vampire",     F::CrimsonWardens,4,P::None, 40,8,5,7,13,9, goldAndRes(380,ResourceType::FaithStones,2), T::Undead|T::Flying, true);
    m_units.back().vampiric = true;      // Vampire drains HP equal to damage dealt
    addUnit(2005,"Lich",        F::CrimsonWardens,5,P::None, 60,11,6,12,20,8,goldAndRes(720,ResourceType::FaithStones,3), T::Undead);
    addUnit(2006,"Bone Dragon", F::CrimsonWardens,6,P::None,140,16,13,20,35,11,goldAndRes(1500,ResourceType::FaithStones,5), T::Undead|T::Flying, true);

    // ── THORNKIN ─────────────────────────────────────────────────────────────
    { BuildingDef b; b.id=BID::TK_GROVE_HEART; b.name="Grove Heart";
      b.description="Town Hall — +1000 Gold weekly, +2 unit growth"; b.category=BuildingCategory::Economy;
      b.faction=F::Thornkin; b.cost=gold(500); b.weeklyIncome=gold(1000); b.growthBonus=2;
      m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::TK_T1; b.name="Sprout Hollow"; b.tier=1; b.weeklyGrowth=14;
      b.description="Produces Sproutlings"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::Thornkin; b.cost=gold(300); m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::TK_T2; b.name="Briar Thicket"; b.tier=2; b.weeklyGrowth=10;
      b.description="Produces Briars"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::Thornkin; b.cost=goldAndRes(600,ResourceType::VerdantSap,1);
      b.prerequisites={BID::TK_T1}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::TK_T3; b.name="Vine Den"; b.tier=3; b.weeklyGrowth=7;
      b.description="Produces Vine Crawlers"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::Thornkin; b.cost=goldAndRes(1000,ResourceType::VerdantSap,2);
      b.prerequisites={BID::TK_T2}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::TK_T4; b.name="Guardian Grove"; b.tier=4; b.weeklyGrowth=5;
      b.description="Produces Grove Guardians"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::Thornkin; b.cost=goldAndRes(1800,ResourceType::VerdantSap,3);
      b.prerequisites={BID::TK_T3}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::TK_T5; b.name="Elder Circle"; b.tier=5; b.weeklyGrowth=3;
      b.description="Produces Ancient Oaks"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::Thornkin; b.cost=goldAndRes(3000,ResourceType::VerdantSap,4);
      b.prerequisites={BID::TK_T4}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::TK_T6; b.name="World Tree Root"; b.tier=6; b.weeklyGrowth=2;
      b.description="Produces World Thorns"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::Thornkin; b.cost=goldAndRes(5500,ResourceType::VerdantSap,6);
      b.prerequisites={BID::TK_T5}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::TK_ANCIENT_CIRCLE; b.name="Ancient Circle";
      b.description="+3 Nature Power; terrain converts to Forest around town";
      b.category=BuildingCategory::Support; b.faction=F::Thornkin;
      b.cost=goldAndRes(1500,ResourceType::VerdantSap,3);
      b.prerequisites={BID::TK_GROVE_HEART}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::TK_SYMBIOSIS_WEB; b.name="Symbiosis Web";
      b.description="Bond pairs share stat bonuses from Symbiosis skill";
      b.category=BuildingCategory::Support; b.faction=F::Thornkin;
      b.cost=goldAndRes(2500,ResourceType::VerdantSap,4);
      b.prerequisites={BID::TK_ANCIENT_CIRCLE}; m_buildings.push_back(b); }

    addUnit(3001,"Sproutling",    F::Thornkin,1,P::None,  7,1,2,1, 2,3, gold(45),        T::Beast);
    addUnit(3002,"Briar",         F::Thornkin,2,P::None, 18,3,4,2, 4,4, goldAndRes(100,ResourceType::VerdantSap,1), T::Beast);
    addUnit(3003,"Vine Crawler",  F::Thornkin,3,P::None, 30,6,5,4, 8,5, goldAndRes(200,ResourceType::VerdantSap,1), T::Beast);
    m_units.back().regenerates = true;   // Vine Crawler regenerates each turn
    addUnit(3004,"Grove Guardian",F::Thornkin,4,P::None, 55,8,8,8,14,5, goldAndRes(400,ResourceType::VerdantSap,2), T::Beast);
    addUnit(3005,"Ancient Oak",   F::Thornkin,5,P::None, 90,11,11,12,20,6,goldAndRes(750,ResourceType::VerdantSap,3), T::Beast);
    addUnit(3006,"World Thorn",   F::Thornkin,6,P::None,160,15,15,22,35,7,goldAndRes(1500,ResourceType::VerdantSap,5), T::Beast);

    // ── ETERNAL EMPIRE ────────────────────────────────────────────────────────
    { BuildingDef b; b.id=BID::EE_THRONE; b.name="Imperial Throne";
      b.description="Town Hall — +1000 Gold weekly, +2 unit growth"; b.category=BuildingCategory::Economy;
      b.faction=F::EternalEmpire; b.cost=gold(500); b.weeklyIncome=gold(1000); b.growthBonus=2;
      m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::EE_T1; b.name="Conscript Pen"; b.tier=1; b.weeklyGrowth=13;
      b.description="Produces Conscripts"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::EternalEmpire; b.cost=gold(300); m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::EE_T2; b.name="Revenant Barracks"; b.tier=2; b.weeklyGrowth=10;
      b.description="Produces Revenants"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::EternalEmpire; b.cost=goldAndRes(600,ResourceType::BloodEssence,1);
      b.prerequisites={BID::EE_T1}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::EE_T3; b.name="Shade Gallery"; b.tier=3; b.weeklyGrowth=7;
      b.description="Produces Shade Archers"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::EternalEmpire; b.cost=goldAndRes(1000,ResourceType::Mercury,1);
      b.prerequisites={BID::EE_T2}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::EE_T4; b.name="Steel Foundry"; b.tier=4; b.weeklyGrowth=5;
      b.description="Produces Steel Guardians"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::EternalEmpire; b.cost=goldAndRes(1800,ResourceType::Mercury,2);
      b.prerequisites={BID::EE_T3}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::EE_T5; b.name="Phantom Keep"; b.tier=5; b.weeklyGrowth=3;
      b.description="Produces Phantom Knights"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::EternalEmpire; b.cost=goldAndRes(3000,ResourceType::Mercury,3);
      b.prerequisites={BID::EE_T4}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::EE_T6; b.name="Immortal Vault"; b.tier=6; b.weeklyGrowth=2;
      b.description="Produces Immortals (Second Life)"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::EternalEmpire; b.cost=goldAndRes(5500,ResourceType::Mercury,5);
      b.prerequisites={BID::EE_T5}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::EE_NECROPOLIS; b.name="Necropolis Gate";
      b.description="+3 Death Power; fallen enemies have 15% chance to rise as Conscripts";
      b.category=BuildingCategory::Support; b.faction=F::EternalEmpire;
      b.cost=goldAndRes(1500,ResourceType::BloodEssence,2);
      b.prerequisites={BID::EE_THRONE}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::EE_MONUMENT; b.name="Monument of Eternity";
      b.description="Undead and Holy units gain Second Life — revive once at half HP on death";
      b.category=BuildingCategory::Support; b.faction=F::EternalEmpire;
      b.cost=goldAndRes(2500,ResourceType::Mercury,4);
      b.prerequisites={BID::EE_NECROPOLIS}; m_buildings.push_back(b); }

    addUnit(4001,"Conscript",      F::EternalEmpire,1,P::None,  8,2,2,1, 3,4, gold(45),        T::Humanoid|T::Undead);
    addUnit(4002,"Revenant",       F::EternalEmpire,2,P::None, 14,4,3,2, 5,5, goldAndRes(95,ResourceType::BloodEssence,1), T::Undead);
    addUnit(4003,"Shade Archer",   F::EternalEmpire,3,P::None, 20,5,3,4, 7,6, goldAndRes(190,ResourceType::Mercury,1), T::Undead);
    addUnit(4004,"Steel Guardian", F::EternalEmpire,4,P::None, 50,7,9,7,13,6, goldAndRes(380,ResourceType::Mercury,2), T::Construct|T::Undead);
    addUnit(4005,"Phantom Knight", F::EternalEmpire,5,P::None, 65,10,8,10,18,8,goldAndRes(720,ResourceType::Mercury,3), T::Undead|T::Flying, true);
    addUnit(4006,"Immortal",       F::EternalEmpire,6,P::None,100,13,11,18,28,10,goldAndRes(1400,ResourceType::Mercury,5), T::Undead|T::Flying, true);

    // ── BLOODSWORN ────────────────────────────────────────────────────────────
    { BuildingDef b; b.id=BID::BS_WAR_HALL; b.name="War Hall";
      b.description="Town Hall — +1000 Gold weekly, +2 unit growth"; b.category=BuildingCategory::Economy;
      b.faction=F::Bloodsworn; b.cost=gold(500); b.weeklyIncome=gold(1000); b.growthBonus=2;
      m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::BS_T1; b.name="Bloodling Pen"; b.tier=1; b.weeklyGrowth=14;
      b.description="Produces Bloodlings"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::Bloodsworn; b.cost=gold(300); m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::BS_T2; b.name="Berserker Pits"; b.tier=2; b.weeklyGrowth=10;
      b.description="Produces Berserkers"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::Bloodsworn; b.cost=goldAndRes(600,ResourceType::BloodEssence,1);
      b.prerequisites={BID::BS_T1}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::BS_T3; b.name="Shaman Hut"; b.tier=3; b.weeklyGrowth=7;
      b.description="Produces Blood Shamans"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::Bloodsworn; b.cost=goldAndRes(1000,ResourceType::BloodEssence,2);
      b.prerequisites={BID::BS_T2}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::BS_T4; b.name="Ravager Corral"; b.tier=4; b.weeklyGrowth=5;
      b.description="Produces Ravagers"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::Bloodsworn; b.cost=goldAndRes(1800,ResourceType::BloodEssence,3);
      b.prerequisites={BID::BS_T3}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::BS_T5; b.name="Warlord Pavilion"; b.tier=5; b.weeklyGrowth=3;
      b.description="Produces Bloodtide Warlords"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::Bloodsworn; b.cost=goldAndRes(3000,ResourceType::BloodEssence,4);
      b.prerequisites={BID::BS_T4}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::BS_T6; b.name="Avatar Shrine"; b.tier=6; b.weeklyGrowth=2;
      b.description="Produces Crimson Avatars"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::Bloodsworn; b.cost=goldAndRes(5500,ResourceType::BloodEssence,6);
      b.prerequisites={BID::BS_T5}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::BS_BLOOD_ALTAR; b.name="Blood Altar";
      b.description="+3 Blood Power; sacrifice 1 Bloodling to instantly fill 25% of Blood Pool";
      b.category=BuildingCategory::Support; b.faction=F::Bloodsworn;
      b.cost=goldAndRes(1500,ResourceType::BloodEssence,3);
      b.prerequisites={BID::BS_WAR_HALL}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::BS_WAR_SHRINE; b.name="War Shrine";
      b.description="BloodBound units gain +15 Morale at battle start";
      b.category=BuildingCategory::Support; b.faction=F::Bloodsworn;
      b.cost=goldAndRes(2500,ResourceType::BloodEssence,4);
      b.prerequisites={BID::BS_BLOOD_ALTAR}; m_buildings.push_back(b); }

    addUnit(5001,"Bloodling",          F::Bloodsworn,1,P::None,  9,3,1,2, 4,5, gold(50),        T::Humanoid|T::BloodBound);
    addUnit(5002,"Berserker",          F::Bloodsworn,2,P::None, 16,5,2,3, 7,7, goldAndRes(110,ResourceType::BloodEssence,1), T::Humanoid|T::BloodBound);
    addUnit(5003,"Blood Shaman",       F::Bloodsworn,3,P::None, 22,6,3,5, 9,6, goldAndRes(200,ResourceType::BloodEssence,1), T::Humanoid|T::BloodBound);
    addUnit(5004,"Ravager",            F::Bloodsworn,4,P::None, 45,10,4,9,18,8, goldAndRes(400,ResourceType::BloodEssence,2), T::Humanoid|T::BloodBound);
    addUnit(5005,"Bloodtide Warlord",  F::Bloodsworn,5,P::None, 70,13,7,14,24,9,goldAndRes(750,ResourceType::BloodEssence,3), T::Humanoid|T::BloodBound);
    addUnit(5006,"Crimson Avatar",     F::Bloodsworn,6,P::None,130,18,8,24,40,11,goldAndRes(1500,ResourceType::BloodEssence,5), T::Humanoid|T::BloodBound);

    // ── VOIDKIN ───────────────────────────────────────────────────────────────
    { BuildingDef b; b.id=BID::VK_NEXUS; b.name="Void Nexus";
      b.description="Town Hall — +1000 Gold weekly, +2 unit growth"; b.category=BuildingCategory::Economy;
      b.faction=F::Voidkin; b.cost=gold(500); b.weeklyIncome=gold(1000); b.growthBonus=2;
      m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::VK_T1; b.name="Wisp Hollow"; b.tier=1; b.weeklyGrowth=13;
      b.description="Produces Void Wisps"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::Voidkin; b.cost=gold(300); m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::VK_T2; b.name="Phase Den"; b.tier=2; b.weeklyGrowth=10;
      b.description="Produces Phase Walkers"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::Voidkin; b.cost=goldAndRes(600,ResourceType::VerdantSap,1);
      b.prerequisites={BID::VK_T1}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::VK_T3; b.name="Rift Arch"; b.tier=3; b.weeklyGrowth=7;
      b.description="Produces Rift Archers"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::Voidkin; b.cost=goldAndRes(1000,ResourceType::VerdantSap,2);
      b.prerequisites={BID::VK_T2}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::VK_T4; b.name="Stalker Gate"; b.tier=4; b.weeklyGrowth=5;
      b.description="Produces Void Stalkers"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::Voidkin; b.cost=goldAndRes(1800,ResourceType::VerdantSap,3);
      b.prerequisites={BID::VK_T3}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::VK_T5; b.name="Wraith Spire"; b.tier=5; b.weeklyGrowth=3;
      b.description="Produces Entropy Wraiths"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::Voidkin; b.cost=goldAndRes(3000,ResourceType::VerdantSap,4);
      b.prerequisites={BID::VK_T4}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::VK_T6; b.name="Colossus Rift"; b.tier=6; b.weeklyGrowth=2;
      b.description="Produces Void Colossi"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::Voidkin; b.cost=goldAndRes(5500,ResourceType::VerdantSap,6);
      b.prerequisites={BID::VK_T5}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::VK_RIFT_GATE; b.name="Rift Gate";
      b.description="+3 Nature Power (void resonance); phase units ignore terrain penalties";
      b.category=BuildingCategory::Support; b.faction=F::Voidkin;
      b.cost=goldAndRes(1500,ResourceType::VerdantSap,3);
      b.prerequisites={BID::VK_NEXUS}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::VK_VOID_LENS; b.name="Void Lens";
      b.description="Void units gain +1 Attack at battle start";
      b.category=BuildingCategory::Support; b.faction=F::Voidkin;
      b.cost=goldAndRes(2500,ResourceType::VerdantSap,4);
      b.prerequisites={BID::VK_RIFT_GATE}; m_buildings.push_back(b); }

    addUnit(6001,"Void Wisp",      F::Voidkin,1,P::None,  6,2,2,1, 3,6, gold(45),        T::Void|T::Flying, true);
    addUnit(6002,"Phase Walker",   F::Voidkin,2,P::None, 12,4,3,2, 5,8, goldAndRes(100,ResourceType::VerdantSap,1), T::Void|T::Flying, true);
    addUnit(6003,"Rift Archer",    F::Voidkin,3,P::None, 20,6,4,4, 8,7, goldAndRes(190,ResourceType::VerdantSap,1), T::Void|T::Flying, true);
    addUnit(6004,"Void Stalker",   F::Voidkin,4,P::None, 40,8,6,8,14,10,goldAndRes(380,ResourceType::VerdantSap,2), T::Void|T::Flying, true);
    addUnit(6005,"Entropy Wraith", F::Voidkin,5,P::None, 60,11,8,12,20,12,goldAndRes(720,ResourceType::VerdantSap,3), T::Void|T::Flying, true);
    addUnit(6006,"Void Colossus",  F::Voidkin,6,P::None,110,15,11,20,32,13,goldAndRes(1400,ResourceType::VerdantSap,5), T::Void|T::Flying, true);

    // ── IRON ASSEMBLY ─────────────────────────────────────────────────────────
    { BuildingDef b; b.id=BID::IA_FORGE_HALL; b.name="Forge Hall";
      b.description="Town Hall — +1000 Gold weekly, +2 unit growth"; b.category=BuildingCategory::Economy;
      b.faction=F::IronAssembly; b.cost=gold(500); b.weeklyIncome=gold(1000); b.growthBonus=2;
      m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::IA_T1; b.name="Automaton Works"; b.tier=1; b.weeklyGrowth=12;
      b.description="Produces Automatons"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::IronAssembly; b.cost=goldAndRes(350,ResourceType::Iron,2); m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::IA_T2; b.name="Gun Construct Bay"; b.tier=2; b.weeklyGrowth=9;
      b.description="Produces Gun Constructs"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::IronAssembly; b.cost=goldAndRes(700,ResourceType::Iron,3);
      b.prerequisites={BID::IA_T1}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::IA_T3; b.name="Steam Walker Depot"; b.tier=3; b.weeklyGrowth=6;
      b.description="Produces Steam Walkers"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::IronAssembly; b.cost=goldAndRes(1100,ResourceType::Iron,4);
      b.prerequisites={BID::IA_T2}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::IA_T4; b.name="Siege Bot Foundry"; b.tier=4; b.weeklyGrowth=5;
      b.description="Produces Siege Bots (available week 3)"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::IronAssembly; b.cost=goldAndRes(2000,ResourceType::Iron,6);
      b.minWeek=3; b.prerequisites={BID::IA_T3}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::IA_T5; b.name="Titan Assembly"; b.tier=5; b.weeklyGrowth=3;
      b.description="Produces Titan Constructs (available week 5)"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::IronAssembly; b.cost=goldAndRes(3500,ResourceType::Iron,8);
      b.minWeek=5; b.prerequisites={BID::IA_T4}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::IA_T6; b.name="Colossus Prime Dock"; b.tier=6; b.weeklyGrowth=2;
      b.description="Produces Colossus Primes (available week 7)"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::IronAssembly; b.cost=goldAndRes(6000,ResourceType::Iron,12);
      b.minWeek=7; b.prerequisites={BID::IA_T5}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::IA_BLUEPRINT_VAULT; b.name="Blueprint Vault";
      b.description="+3 Forge Power; constructs unlock 1 week earlier";
      b.category=BuildingCategory::Support; b.faction=F::IronAssembly;
      b.cost=goldAndRes(1500,ResourceType::Iron,4);
      b.prerequisites={BID::IA_FORGE_HALL}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::IA_OVERCLOCK; b.name="Overclock Chamber";
      b.description="All mechanical units gain +1 speed in combat";
      b.category=BuildingCategory::Support; b.faction=F::IronAssembly;
      b.cost=goldAndRes(2500,ResourceType::Iron,6);
      b.minWeek=3; b.prerequisites={BID::IA_BLUEPRINT_VAULT}; m_buildings.push_back(b); }

    addUnit(7001,"Automaton",       F::IronAssembly,1,P::None, 10,2,3,1, 3,3, goldAndRes(55,ResourceType::Iron,1),  T::Mechanical);
    addUnit(7002,"Gun Construct",   F::IronAssembly,2,P::None, 18,4,4,3, 6,4, goldAndRes(110,ResourceType::Iron,2), T::Mechanical);
    addUnit(7003,"Steam Walker",    F::IronAssembly,3,P::None, 35,6,6,5, 9,5, goldAndRes(210,ResourceType::Iron,3), T::Mechanical);
    addUnit(7004,"Siege Bot",       F::IronAssembly,4,P::None, 60,9,9,9,16,4, goldAndRes(400,ResourceType::Iron,5), T::Mechanical);
    addUnit(7005,"Titan Construct", F::IronAssembly,5,P::None, 90,12,12,14,22,5,goldAndRes(780,ResourceType::Iron,7), T::Mechanical);
    addUnit(7006,"Colossus Prime",  F::IronAssembly,6,P::None,180,16,16,25,40,6,goldAndRes(1600,ResourceType::Iron,10), T::Mechanical);

    // ── AMALGAMATE ────────────────────────────────────────────────────────────
    { BuildingDef b; b.id=BID::AM_GRAFTING_HALL; b.name="Grafting Hall";
      b.description="Town Hall — +1000 Gold weekly, +2 unit growth"; b.category=BuildingCategory::Economy;
      b.faction=F::Amalgamate; b.cost=gold(500); b.weeklyIncome=gold(1000); b.growthBonus=2;
      m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::AM_T1; b.name="Flesh Crawler Vat"; b.tier=1; b.weeklyGrowth=13;
      b.description="Produces Flesh Crawlers"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::Amalgamate; b.cost=gold(300); m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::AM_T2; b.name="Graft Soldier Bay"; b.tier=2; b.weeklyGrowth=9;
      b.description="Produces Graft Soldiers"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::Amalgamate; b.cost=goldAndRes(650,ResourceType::Iron,1);
      b.prerequisites={BID::AM_T1}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::AM_T3; b.name="Bone Machine Works"; b.tier=3; b.weeklyGrowth=6;
      b.description="Produces Bone Machines"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::Amalgamate; b.cost=goldAndRes(1100,ResourceType::Iron,2);
      b.prerequisites={BID::AM_T2}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::AM_T4; b.name="Fleshwork Forge"; b.tier=4; b.weeklyGrowth=5;
      b.description="Produces Fleshwork Knights"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::Amalgamate; b.cost=goldAndRes(1900,ResourceType::BloodEssence,2);
      b.prerequisites={BID::AM_T3}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::AM_T5; b.name="Juggernaut Pit"; b.tier=5; b.weeklyGrowth=3;
      b.description="Produces Undying Juggernauts"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::Amalgamate; b.cost=goldAndRes(3200,ResourceType::BloodEssence,3);
      b.prerequisites={BID::AM_T4}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::AM_T6; b.name="Spawn Chamber"; b.tier=6; b.weeklyGrowth=2;
      b.description="Produces Convergence Spawns"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::Amalgamate; b.cost=goldAndRes(5800,ResourceType::BloodEssence,5);
      b.prerequisites={BID::AM_T5}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::AM_FLESH_VAULT; b.name="Flesh Vault";
      b.description="+3 Flesh Power; fallen units leave behind organic material for crafting";
      b.category=BuildingCategory::Support; b.faction=F::Amalgamate;
      b.cost=goldAndRes(1500,ResourceType::BloodEssence,2);
      b.prerequisites={BID::AM_GRAFTING_HALL}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::AM_MERGE_CHAMBER; b.name="Merge Chamber";
      b.description="Adaptation traits acquired 20% faster";
      b.category=BuildingCategory::Support; b.faction=F::Amalgamate;
      b.cost=goldAndRes(2500,ResourceType::Iron,3);
      b.prerequisites={BID::AM_FLESH_VAULT}; m_buildings.push_back(b); }

    addUnit(8001,"Flesh Crawler",     F::Amalgamate,1,P::None, 10,2,2,1, 4,4, gold(50),        T::OrganicMech);
    addUnit(8002,"Graft Soldier",     F::Amalgamate,2,P::None, 20,4,4,3, 6,5, goldAndRes(105,ResourceType::Iron,1), T::OrganicMech);
    addUnit(8003,"Bone Machine",      F::Amalgamate,3,P::None, 38,7,5,5,10,6, goldAndRes(210,ResourceType::Iron,2), T::OrganicMech);
    addUnit(8004,"Fleshwork Knight",  F::Amalgamate,4,P::None, 65,9,7,9,17,7, goldAndRes(420,ResourceType::BloodEssence,2), T::OrganicMech|T::Flying, true);
    addUnit(8005,"Undying Juggernaut",F::Amalgamate,5,P::None,100,12,9,14,24,8,goldAndRes(780,ResourceType::BloodEssence,3), T::OrganicMech);
    addUnit(8006,"Convergence Spawn", F::Amalgamate,6,P::None,170,16,12,24,38,9,goldAndRes(1500,ResourceType::BloodEssence,5), T::OrganicMech|T::Flying, true);

    // ── CONVERGENCE ───────────────────────────────────────────────────────────
    { BuildingDef b; b.id=BID::CV_SYNTHESIS_HUB; b.name="Synthesis Hub";
      b.description="Town Hall — +1000 Gold weekly, +2 unit growth"; b.category=BuildingCategory::Economy;
      b.faction=F::Convergence; b.cost=gold(500); b.weeklyIncome=gold(1000); b.growthBonus=2;
      m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::CV_T1; b.name="Awakening Chamber"; b.tier=1; b.weeklyGrowth=11;
      b.description="Produces Awakened"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::Convergence; b.cost=gold(350); m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::CV_T2; b.name="Synthesis Lab"; b.tier=2; b.weeklyGrowth=8;
      b.description="Produces Synthesized"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::Convergence; b.cost=gold(700);
      b.prerequisites={BID::CV_T1}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::CV_T3; b.name="Harmony Hall"; b.tier=3; b.weeklyGrowth=6;
      b.description="Produces Harmonized"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::Convergence; b.cost=gold(1100);
      b.prerequisites={BID::CV_T2}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::CV_T4; b.name="Resonance Spire"; b.tier=4; b.weeklyGrowth=4;
      b.description="Produces Resonants"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::Convergence; b.cost=gold(2000);
      b.prerequisites={BID::CV_T3}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::CV_T5; b.name="Transcendence Gate"; b.tier=5; b.weeklyGrowth=3;
      b.description="Produces Transcendents"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::Convergence; b.cost=gold(3200);
      b.prerequisites={BID::CV_T4}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::CV_T6; b.name="Unity Forge"; b.tier=6; b.weeklyGrowth=2;
      b.description="Produces Unified Forms"; b.category=BuildingCategory::UnitDwelling;
      b.faction=F::Convergence; b.cost=gold(5500);
      b.prerequisites={BID::CV_T5}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::CV_RESONANCE_WELL; b.name="Resonance Well";
      b.description="Convergence units gain +1 Attack and Defense at battle start (synergizes with Mirroring)";
      b.category=BuildingCategory::Support; b.faction=F::Convergence;
      b.cost=gold(2000); b.prerequisites={BID::CV_SYNTHESIS_HUB}; m_buildings.push_back(b); }
    { BuildingDef b; b.id=BID::CV_MIRROR_CHAMBER; b.name="Mirror Chamber";
      b.description="Convergence units gain an additional +1 Attack and Defense; hero gains +1 Attack and Defense";
      b.category=BuildingCategory::Support; b.faction=F::Convergence;
      b.cost=gold(3500); b.prerequisites={BID::CV_RESONANCE_WELL}; m_buildings.push_back(b); }

    addUnit(9001,"Awakened",      F::Convergence,1,P::None,  9,3,3,2, 4,5, gold(60),   T::Humanoid);
    addUnit(9002,"Synthesized",   F::Convergence,2,P::None, 18,5,5,3, 6,6, gold(120),  T::Humanoid);
    addUnit(9003,"Harmonized",    F::Convergence,3,P::None, 30,7,7,5,10,7, gold(220),  T::Humanoid);
    addUnit(9004,"Resonant",      F::Convergence,4,P::None, 55,10,10,9,16,8, gold(430), T::Humanoid|T::Flying, true);
    addUnit(9005,"Transcendent",  F::Convergence,5,P::None, 85,13,13,13,22,10,gold(800), T::Humanoid|T::Flying, true);
    addUnit(9006,"Unified Form",  F::Convergence,6,P::None,150,17,17,22,36,12,gold(1600), T::Humanoid|T::Flying, true);
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
