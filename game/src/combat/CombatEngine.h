#pragma once
#include <vector>
#include <functional>
#include "CombatPhase.h"
#include "CombatGrid.h"
#include "DamageCalc.h"
#include "../hero/Hero.h"
#include "../magic/SpellDef.h"

// ── AI difficulty / personality ────────────────────────────────────────────────
enum class AIDifficulty : uint8_t
{
    Passive,    // random movement, no targeting priority — baseline / easy
    Standard,   // nearest enemy, attack if adjacent — current default
    Tactical,   // focus weakest stack, protect own ranged, kite
};

// ── Action types ───────────────────────────────────────────────────────────────
enum class ActionType { Move, Attack, Wait, Defend, Shoot, UseAbility };

struct CombatAction
{
    ActionType type         = ActionType::Move;
    uint32_t   unitId       = 0;
    HexCoord   target       = {0, 0};   // move target or attack position
    uint32_t   targetUnitId = 0;        // for attacks
    int        spellId      = 0;        // for UseAbility (cast spell)
};

// ── Combat log entry ───────────────────────────────────────────────────────────
struct CombatLog
{
    std::string message;
};

// ── CombatEngine ──────────────────────────────────────────────────────────────
class CombatEngine
{
public:
    using LogCallback    = std::function<void(const std::string&)>;
    using DamageCallback = std::function<void(uint32_t /*targetId*/, int /*damage*/, HexCoord /*pos*/)>;

    CombatEngine() = default;

    // Setup a battle — populates grid with units from both sides
    void startBattle(const Hero& playerHero, const std::vector<CombatUnit>& playerUnits,
                     const Hero& enemyHero,  const std::vector<CombatUnit>& enemyUnits,
                     bool isSiege = false);

    // Apply equipped-artifact bonuses to stored hero copies and their units
    // (call once after startBattle, before first turn)
    void applyArtifactBonuses(const ArtifactBonus& playerBonus,
                              const ArtifactBonus& enemyBonus);

    // Process one player action — returns true if action was valid
    bool submitAction(const CombatAction& action);

    // Advance AI turn — processes all enemy units
    void processAITurn();

    // Called when current unit clicks Wait
    void wait();

    // End current unit's turn without acting
    void skipUnit();

    // State queries
    CombatPhase    phase()          const { return m_phase; }
    CombatGrid&    grid()                 { return m_grid; }
    const CombatGrid& grid()        const { return m_grid; }
    CombatUnit*    activeUnit();
    int            round()          const { return m_round; }
    bool           isPlayerTurn()   const { return m_phase == CombatPhase::PlayerTurn; }

    // Turn order queue (sorted by speed, rebuilt each round)
    const std::vector<uint32_t>& turnOrder() const { return m_turnOrder; }
    int turnIndex() const { return m_turnIndex; }

    // Log
    const std::vector<CombatLog>& log() const { return m_log; }
    void setLogCallback(LogCallback cb) { m_logCb = cb; }
    void setDamageCallback(DamageCallback cb) { m_dmgCb = cb; }

    // XP earned this battle (enemy unit-count × 5, awarded on victory)
    int xpEarned() const { return m_enemyStartCount * 5; }
    int enemyStartCount() const { return m_enemyStartCount; }

    // Hero state accessors (for HUD display)
    const Hero& playerHero() const { return m_playerHero; }
    const Hero& enemyHero()  const { return m_enemyHero; }

    // Headless batch simulation — both sides use AI, returns final phase
    void setSilent(bool s) { m_silent = s; }
    CombatPhase runHeadless(int maxRounds = 60);

    // Seed the per-battle turn-order RNG (call alongside DamageCalc::seedRng)
    static void seedTurnRng(uint32_t seed);

    // AI difficulty — affects both processAITurn() and the player side in runHeadless()
    void setPlayerAI(AIDifficulty d) { m_playerAI = d; }
    void setEnemyAI(AIDifficulty d)  { m_enemyAI  = d; }
    AIDifficulty playerAI() const { return m_playerAI; }
    AIDifficulty enemyAI()  const { return m_enemyAI; }

private:
    void buildTurnOrder();
    void advanceTurn();
    void checkVictory();
    void applyTileEffect(CombatUnit& unit);
    void addLog(const std::string& msg);
    void applySymbiosisRound(); // Thornkin bond bonus — called at round start
    void processRoundStartEffects(); // DoT tick, mana regen — called at round start

    // AI dispatch — delegates to difficulty-specific implementation
    void aiActUnit(CombatUnit& unit);
    void aiActPassive(CombatUnit& unit);   // random target, no priority
    void aiActStandard(CombatUnit& unit);  // nearest enemy (current behaviour)
    void aiActTactical(CombatUnit& unit);  // weakest stack first, protect ranged
    void tryEnemyHeroSpell();              // enemy hero casts one spell per round

    CombatGrid  m_grid;
    CombatPhase m_phase     = CombatPhase::Setup;
    int         m_round     = 1;
    int         m_turnIndex = 0;

    std::vector<uint32_t>  m_turnOrder;   // unit IDs in speed order
    std::vector<uint32_t>  m_waitQueue;   // units that used Wait
    int                    m_enemyStartCount = 0; // total enemy units at battle start
    bool                   m_enemyHeroSpellUsed = false; // one cast per round

    std::vector<CombatLog> m_log;
    LogCallback            m_logCb;
    DamageCallback         m_dmgCb;
    bool                   m_silent  = false;
    AIDifficulty           m_playerAI = AIDifficulty::Standard;
    AIDifficulty           m_enemyAI  = AIDifficulty::Standard;

    Hero m_playerHero;
    Hero m_enemyHero;
};
