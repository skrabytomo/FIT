#pragma once
#include "../world/HexMap.h"
#include "../data/Resources.h"
#include "../hero/FactionId.h"

enum class WorldObjectType : uint8_t
{
    SpellScroll,    // teaches a spell to the hero (value = spellId)
    ArtifactChest,  // gives an artifact (value = artifactId)
    XPShrine,       // grants XP to the hero (value = xpAmount)
    ResourceCache,  // gives resources (value = amount, resourceType set below)
    Observatory,    // reveals FoW in radius; value=radius(tiles), resets weekly
    StatShrine,     // spend 1000g for hero +stat; value=which stat (0=atk,1=def,2=spd,3=lightPower); questState tracks uses (max 3 per visit)
    BanditCamp,     // fight bandits, get reward; value=difficulty(1-3)
    UnitDwelling,   // weekly recruitable pool; value=tier(1-6), faction field = faction
    QuestGiver,     // gives a quest; linkedId = QuestTarget obj id
    QuestTarget,    // destination; linkedId = QuestGiver obj id
    // Terrain-specific objects
    ForestShrine,   // Forest: +75 XP (value=75)
    HighlandRuin,   // Highland/Rocky: reveals radius (value=4), permanent
    HolyFountain,   // Sacred: restores hero mana; resets weekly
    Oasis,          // Barren/Wasteland: restores hero movePool; resets weekly
    Campfire,       // Plains: +150 gold (value=150)
    LavaCrystal,    // Volcanic: gives Mercury (value=3, resourceType=Mercury)
    SwampAltar,     // Swamp: teaches a spell (value=spellId)
};

struct WorldObject
{
    uint32_t        id           = 0;
    WorldObjectType type         = WorldObjectType::XPShrine;
    HexCoord        pos          = {0, 0};
    int             value        = 0;
    ResourceType    resourceType = ResourceType::Gold;
    bool            collected    = false;
    // New fields for extended types
    uint8_t         faction      = 0;   // UnitDwelling: cast to FactionId
    int             available    = 0;   // UnitDwelling: units ready to recruit this week
    uint32_t        linkedId     = 0;   // QuestGiver<->QuestTarget cross-reference
    int             questState   = 0;   // QuestGiver: 0=idle,1=active,2=complete; StatShrine: uses remaining
};
