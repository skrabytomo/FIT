#include "Game.h"
#include "../hero/SkillRegistry.h"
#include "../magic/SpellRegistry.h"
#include <imgui.h>
#include <stdio.h>
#include <sstream>
#include <algorithm>

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

    beginImGuiFrame();
    if (m_showSpellPanel) renderSpellPanel();
    endImGuiFrame();
}

// ── Spell panel (ImGui) ───────────────────────────────────────────────────────
void Game::renderSpellPanel()
{
    if (m_heroes.empty()) return;
    const Hero& hero = m_heroes[m_activeHeroIdx];
    if (hero.knownSpells.empty()) {
        ImGui::Begin("Spells", &m_showSpellPanel, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::TextDisabled("No spells known.");
        ImGui::TextDisabled("Find spellbooks on the world map or build a mage tower.");
        ImGui::End();
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Spells", &m_showSpellPanel)) { ImGui::End(); return; }

    ImGui::Text("Mana: %d / %d", hero.mana, hero.maxMana);
    ImGui::Separator();

    // Target selector — pick from living enemy units
    if (ImGui::BeginCombo("Target", m_spellTargetId == 0 ? "— pick —" : [&]() -> const char* {
        auto* u = m_combat.grid().getUnit(m_spellTargetId);
        return u ? u->name.c_str() : "—";
    }()))
    {
        for (auto& u : m_combat.grid().units()) {
            if (!u.alive) continue;
            bool sel = (u.id == m_spellTargetId);
            std::string lbl = u.name + (u.isPlayer ? " [ally]" : " [enemy]");
            if (ImGui::Selectable(lbl.c_str(), sel))
                m_spellTargetId = u.id;
        }
        ImGui::EndCombo();
    }
    ImGui::Separator();

    CombatUnit* active = m_combat.activeUnit();
    bool isPlayerTurn  = active && active->isPlayer;

    for (int sid : hero.knownSpells) {
        const SpellDef* spell = findSpell(sid);
        if (!spell) continue;

        bool canAfford = hero.mana >= spell->manaCost;
        if (!isPlayerTurn || !canAfford) ImGui::BeginDisabled();

        char btnLabel[128];
        std::snprintf(btnLabel, sizeof(btnLabel), "%s  (%d mana)", spell->name, spell->manaCost);
        if (ImGui::Button(btnLabel, ImVec2(-1, 0))) {
            CombatAction act;
            act.type         = ActionType::UseAbility;
            act.spellId      = sid;
            act.targetUnitId = m_spellTargetId;
            m_combat.submitAction(act);
            m_showSpellPanel = false;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", spell->desc);

        if (!isPlayerTurn || !canAfford) ImGui::EndDisabled();
    }
    ImGui::End();
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

    // Sync surviving units back to their hero armies
    if (!m_heroes.empty()) {
        Hero& hero = m_heroes[m_activeHeroIdx];
        hero.army.clear();
        for (const auto& cu : m_combat.grid().units()) {
            if (!cu.alive || cu.isPlayer == false || cu.count <= 0) continue;
            bool merged = false;
            for (auto& s : hero.army)
                if (s.defId == cu.defId) { s.count += cu.count; merged = true; break; }
            if (!merged) hero.army.push_back({cu.defId, cu.count});
        }
        printf("Hero survivors: %zu stacks\n", hero.army.size());
    }

    if (playerWon) {
        // Capture town if this was a garrison fight
        if (m_pendingTownCapture) {
            m_pendingTownCapture->ownerId = 1;
            m_pendingTownCapture->garrison.clear();
            m_capturedTownName = m_pendingTownCapture->name;
            m_showCapturePopup = true;
            printf("Captured town after garrison fight: %s\n", m_capturedTownName.c_str());
            m_pendingTownCapture = nullptr;
        }

        // Remove defeated enemy hero from the world
        if (m_lastCombatEnemyId != 0) {
            m_enemyHeroes.erase(
                std::remove_if(m_enemyHeroes.begin(), m_enemyHeroes.end(),
                    [&](const Hero& e){ return e.id == m_lastCombatEnemyId; }),
                m_enemyHeroes.end());
            // Clear their map tile
            m_map.forEach([&](HexTile& t){
                if (t.heroId == m_lastCombatEnemyId) t.heroId = 0;
            });
            m_lastCombatEnemyId = 0;
        }

        m_hideout.addXP(50);
        m_triggers.fire(TriggerType::BattleWon, ctx);
        if (m_enemyHeroes.empty()) m_showVictory = true;

        // Award hero XP
        if (!m_heroes.empty()) {
            Hero& hero = m_heroes[m_activeHeroIdx];
            int xp = m_combat.xpEarned();
            printf("Hero earns %d XP\n", xp);
            if (hero.addXp(xp)) {
                printf("Hero leveled up to %d!\n", hero.level);
                const HeroClassDef* cls = m_classRegistry.getClass(hero.classId);
                if (cls) {
                    std::vector<SkillDef> allSkills(SKILL_DEFS, SKILL_DEFS + SKILL_DEF_COUNT);
                    m_levelUpOffers = LevelUpSystem::generateOffers(
                        *cls, hero.skills, hero.level, allSkills, hero.faction);
                }
                if (m_levelUpOffers.empty())
                    m_levelUpOffers.push_back({SID::OFFENSE, false, false, "Learn Offense"});
                m_showLevelUpModal = true;
            }
        }
    } else {
        // Sync surviving enemy army too (they won, they keep what's left)
        for (auto& eh : m_enemyHeroes) {
            if (eh.id != m_lastCombatEnemyId) continue;
            eh.army.clear();
            for (const auto& cu : m_combat.grid().units()) {
                if (!cu.alive || cu.isPlayer || cu.count <= 0) continue;
                bool merged = false;
                for (auto& s : eh.army)
                    if (s.defId == cu.defId) { s.count += cu.count; merged = true; break; }
                if (!merged) eh.army.push_back({cu.defId, cu.count});
            }
            break;
        }
        m_pendingTownCapture = nullptr;
        m_triggers.fire(TriggerType::BattleLost, ctx);
        m_showDefeat = true;
    }
    enterWorldMap();
}
