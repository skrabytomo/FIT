#pragma once
#include <vector>
#include <functional>
#include "CombatPhase.h"
#include "CombatGrid.h"
#include "DamageCalc.h"
#include "../hero/Hero.h"

// ── Action types ───────────────────────────────────────────────────────────────
enum class ActionType { Move, Attack, Wait, Defend, Shoot, UseAbility };

struct CombatAction
{
    ActionType type    = ActionType::Move;
    uint32_t   unitId  = 0;
    HexCoord   target  = {0, 0};   // move target or attack position
    uint32_t   targetUnitId = 0;   // for attacks
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
    using LogCallback = std::function<void(const std::string&)>;

    CombatEngine() = default;

    // Setup a battle — populates grid with units from both sides
    void startBattle(const Hero& playerHero, const std::vector<CombatUnit>& playerUnits,
                     const Hero& enemyHero,  const std::vector<CombatUnit>& enemyUnits,
                     bool isSiege = false);

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

private:
    void buildTurnOrder();
    void advanceTurn();
    void checkVictory();
    void applyTileEffect(CombatUnit& unit);
    void addLog(const std::string& msg);

    // Simple AI — move toward nearest enemy and attack
    void aiActUnit(CombatUnit& unit);

    CombatGrid  m_grid;
    CombatPhase m_phase     = CombatPhase::Setup;
    int         m_round     = 1;
    int         m_turnIndex = 0;

    std::vector<uint32_t>  m_turnOrder;   // unit IDs in speed order
    std::vector<uint32_t>  m_waitQueue;   // units that used Wait

    std::vector<CombatLog> m_log;
    LogCallback            m_logCb;

    Hero m_playerHero;
    Hero m_enemyHero;
};
