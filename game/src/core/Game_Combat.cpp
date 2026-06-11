#include "Game.h"
#include "../hero/SkillRegistry.h"
#include <stdio.h>

// ── Combat update ─────────────────────────────────────────────────────────────
void Game::updateCombat(float dt)
{
    (void)dt;
    const auto& mouse = m_input.mouse();

    if (mouse.leftDown)
        m_combatHUD.onMouseDown(static_cast<float>(mouse.x),
                                static_cast<float>(mouse.y));
    m_combatHUD.onMouseMove(static_cast<float>(mouse.x),
                            static_cast<float>(mouse.y));

    if (m_combat.phase() == CombatPhase::EnemyTurn)
        m_combat.processAITurn();

    if (m_combat.phase() == CombatPhase::Victory)
        exitCombat(true);
    else if (m_combat.phase() == CombatPhase::Defeat)
        exitCombat(false);
}

// ── Combat render ─────────────────────────────────────────────────────────────
void Game::renderCombat()
{
    m_ui.beginFrame();
    m_combatHUD.draw(m_ui, m_combat);
    m_ui.endFrame();
}

// ── State transitions ─────────────────────────────────────────────────────────
void Game::enterCombat(Hero& playerHero,
                       const std::vector<CombatUnit>& playerUnits,
                       const Hero& enemyHero,
                       const std::vector<CombatUnit>& enemyUnits)
{
    m_state = GameState::Combat;
    m_combat.startBattle(playerHero, playerUnits, enemyHero, enemyUnits, false);
    m_combat.setLogCallback([](const std::string& msg) {
        printf("[Combat] %s\n", msg.c_str());
    });
    printf("Entered combat\n");
}

void Game::exitCombat(bool playerWon)
{
    printf("Combat ended — %s\n", playerWon ? "Victory" : "Defeat/Retreat");
    ScriptContext ctx; ctx.heroId = m_heroes.empty() ? 0 : (int)m_heroes[m_activeHeroIdx].id;

    if (playerWon) {
        m_hideout.addXP(50);
        m_triggers.fire(TriggerType::BattleWon, ctx);

        // Award hero XP
        if (!m_heroes.empty()) {
            Hero& hero = m_heroes[m_activeHeroIdx];
            int xp = m_combat.xpEarned();
            printf("Hero earns %d XP\n", xp);
            if (hero.addXp(xp)) {
                printf("Hero leveled up to %d!\n", hero.level);
                const HeroClassDef* cls = m_classRegistry.getClass(hero.classId);
                if (cls) {
                    // Build allSkills vector from static table
                    std::vector<SkillDef> allSkills(SKILL_DEFS, SKILL_DEFS + SKILL_DEF_COUNT);
                    m_levelUpOffers = LevelUpSystem::generateOffers(
                        *cls, hero.skills, hero.level, allSkills, hero.faction);
                }
                if (m_levelUpOffers.empty()) {
                    // Fallback: generic offense offer
                    m_levelUpOffers.push_back({SID::OFFENSE, false, false, "Learn Offense"});
                }
                m_showLevelUpModal = true;
            }
        }
    } else {
        m_triggers.fire(TriggerType::BattleLost, ctx);
    }
    enterWorldMap();
}
