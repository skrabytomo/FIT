#pragma once
#include "../world/HexMap.h"
#include "../data/Resources.h"

enum class WorldObjectType : uint8_t
{
    SpellScroll,    // teaches a spell to the hero (value = spellId)
    ArtifactChest,  // gives an artifact (value = artifactId)
    XPShrine,       // grants XP to the hero (value = xpAmount)
    ResourceCache,  // gives resources (value = amount, resourceType set below)
};

struct WorldObject
{
    uint32_t        id           = 0;
    WorldObjectType type         = WorldObjectType::XPShrine;
    HexCoord        pos          = {0, 0};
    int             value        = 0;
    ResourceType    resourceType = ResourceType::Gold;
    bool            collected    = false;
};
