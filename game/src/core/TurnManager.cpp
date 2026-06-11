#include "TurnManager.h"
#include <stdio.h>

bool TurnManager::endTurn(std::vector<Town>& towns,
                           std::vector<Hero>& heroes,
                           Resources& playerResources,
                           const BuildingRegistry& registry)
{
    // Restore hero movement
    for (auto& hero : heroes) {
        hero.movePool = hero.maxMove;
        hero.path.clear();
        hero.pathStep = 0;
    }

    m_day++;
    bool newWeek = false;

    if (m_day > 7) {
        m_day = 1;
        m_week++;
        newWeek = true;
        onNewWeek(towns, playerResources, registry);
    }

    printf("Day %d Week %d | Gold: %d\n",
        m_day, m_week,
        playerResources.get(ResourceType::Gold));

    return newWeek;
}

void TurnManager::onNewWeek(std::vector<Town>& towns,
                             Resources& playerResources,
                             const BuildingRegistry& registry)
{
    printf("=== WEEK %d BEGINS ===\n", m_week);

    for (auto& town : towns) {
        // Add weekly resource income — only player-owned towns
        if (town.ownerId == 1)
            playerResources.addAll(town.weeklyIncome);

        // Add unit growth to dwellings
        town.onWeekStart(registry.buildings());
    }
}

Resources TurnManager::calculateWeeklyIncome(const std::vector<Town>& towns,
                                               uint32_t ownerId) const
{
    Resources total;
    for (auto& town : towns)
        if (town.ownerId == ownerId)
            total.addAll(town.weeklyIncome);
    return total;
}
