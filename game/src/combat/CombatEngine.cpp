#include "CombatEngine.h"
#include <algorithm>
#include <stdio.h>
#include <sstream>

void CombatEngine::addLog(const std::string& msg)
{
    if (m_silent) return;
    m_log.push_back({msg});
    if (m_logCb) m_logCb(msg);
    printf("[Combat] %s\n", msg.c_str());
}

// ── Start battle ───────────────────────────────────────────────────────────────
void CombatEngine::startBattle(
    const Hero& playerHero, const std::vector<CombatUnit>& playerUnits,
    const Hero& enemyHero,  const std::vector<CombatUnit>& enemyUnits,
    bool isSiege)
{
    m_playerHero = playerHero;
    m_enemyHero  = enemyHero;
    m_round      = 1;
    m_turnIndex  = 0;
    m_log.clear();
    m_waitQueue.clear();

    m_grid.init(48.0f);
    if (!isSiege) m_grid.placeRandomSpecialTiles(4);

    // Place player units on left side (columns 0-1)
    int playerRow = 1;
    for (auto& u : playerUnits) {
        CombatUnit copy = u;
        copy.isPlayer = true;
        copy.shotsLeft = u.shots;
        uint32_t id = m_grid.addUnit(copy);
        CombatUnit* placed = m_grid.getUnit(id);
        if (placed) {
            // Find a free spawn hex on left side
            for (int row = 0; row < CombatGrid::ROWS; ++row) {
                int q = 0 + (placed->stackSlot % 2);
                int r = row - (q - (q & 1)) / 2;
                HexCoord h{q, r};
                if (m_grid.inBounds(h) && !m_grid.getTile(h)->occupied) {
                    m_grid.placeUnit(*placed, h);
                    break;
                }
            }
        }
    }

    // Place enemy units on right side (columns 9-10)
    for (auto& u : enemyUnits) {
        CombatUnit copy = u;
        copy.isPlayer = false;
        copy.shotsLeft = u.shots;
        uint32_t id = m_grid.addUnit(copy);
        CombatUnit* placed = m_grid.getUnit(id);
        if (placed) {
            for (int row = 0; row < CombatGrid::ROWS; ++row) {
                int q = CombatGrid::COLS - 1 - (placed->stackSlot % 2);
                int r = row - (q - (q & 1)) / 2;
                HexCoord h{q, r};
                if (m_grid.inBounds(h) && !m_grid.getTile(h)->occupied) {
                    m_grid.placeUnit(*placed, h);
                    break;
                }
            }
        }
    }

    buildTurnOrder();
    m_phase = CombatPhase::PlayerTurn;
    addLog("Battle started! Round 1");
}

// ── Turn order ─────────────────────────────────────────────────────────────────
void CombatEngine::buildTurnOrder()
{
    m_turnOrder.clear();
    for (auto& u : m_grid.units())
        if (u.alive) m_turnOrder.push_back(u.id);

    // Sort by speed descending, player units win ties
    std::sort(m_turnOrder.begin(), m_turnOrder.end(),
        [this](uint32_t a, uint32_t b) {
            auto* ua = m_grid.getUnit(a);
            auto* ub = m_grid.getUnit(b);
            if (!ua || !ub) return false;
            if (ua->speed != ub->speed) return ua->speed > ub->speed;
            return ua->isPlayer > ub->isPlayer; // player wins ties
        });

    m_turnIndex = 0;
    m_waitQueue.clear();
}

CombatUnit* CombatEngine::activeUnit()
{
    if (m_turnIndex >= static_cast<int>(m_turnOrder.size())) return nullptr;
    return m_grid.getUnit(m_turnOrder[m_turnIndex]);
}

// ── Advance to next unit ───────────────────────────────────────────────────────
void CombatEngine::advanceTurn()
{
    m_turnIndex++;

    // Skip dead units
    while (m_turnIndex < static_cast<int>(m_turnOrder.size())) {
        auto* u = m_grid.getUnit(m_turnOrder[m_turnIndex]);
        if (u && u->alive) break;
        m_turnIndex++;
    }

    // Round over — all units acted (including wait queue)
    if (m_turnIndex >= static_cast<int>(m_turnOrder.size())) {
        // Append wait queue to end
        if (!m_waitQueue.empty()) {
            m_turnOrder.insert(m_turnOrder.end(),
                m_waitQueue.begin(), m_waitQueue.end());
            m_waitQueue.clear();
        } else {
            // New round
            m_round++;
            for (auto& u : m_grid.units()) u.newRound();
            buildTurnOrder();
            addLog("=== Round " + std::to_string(m_round) + " ===");
        }
    }

    checkVictory();
    if (m_phase == CombatPhase::Victory || m_phase == CombatPhase::Defeat) return;

    // Set phase based on whose turn it is
    auto* next = activeUnit();
    if (next) {
        m_phase = next->isPlayer ? CombatPhase::PlayerTurn : CombatPhase::EnemyTurn;
        if (!next->isPlayer) processAITurn();
    }
}

// ── Submit player action ───────────────────────────────────────────────────────
bool CombatEngine::submitAction(const CombatAction& action)
{
    if (m_phase != CombatPhase::PlayerTurn) return false;

    CombatUnit* unit = activeUnit();
    if (!unit || !unit->isPlayer) return false;

    switch (action.type) {
    case ActionType::Move: {
        if (unit->hasMoved) return false;
        auto path = m_grid.findPath(unit->pos, action.target, unit->flying);
        if (path.empty() && !(action.target == unit->pos)) return false;
        if (!path.empty()) {
            m_grid.moveUnit(unit->id, action.target);
            unit->hasMoved = true;
            applyTileEffect(*unit);
            addLog(unit->name + " moved to (" +
                std::to_string(action.target.q) + "," +
                std::to_string(action.target.r) + ")");
        }
        // Moving doesn't end turn — player can still attack
        return true;
    }
    case ActionType::Attack: {
        CombatUnit* target = m_grid.getUnit(action.targetUnitId);
        if (!target || target->isPlayer || !target->alive) return false;

        // Check adjacency for melee
        if (unit->range == 0) {
            if (HexGrid::distance(unit->pos, target->pos) > 1) return false;
        } else {
            // Ranged — check shots remaining
            if (unit->shotsLeft <= 0) return false;
            unit->shotsLeft--;
        }

        auto result = DamageCalc::attack(*unit, *target, m_grid);

        std::ostringstream ss;
        ss << unit->name << " attacks " << target->name
           << " for " << result.damage << " damage";
        if (result.killed > 0) ss << " (" << result.killed << " killed)";
        addLog(ss.str());

        if (!target->alive) {
            addLog(target->name + " destroyed!");
            m_grid.removeDeadUnits();
        }

        if (result.moraleTrigger)
            addLog(unit->name + " morale surge — bonus action!");

        unit->hasActed = true;
        if (!result.moraleTrigger) advanceTurn();
        return true;
    }
    case ActionType::Wait: {
        wait();
        return true;
    }
    case ActionType::Defend: {
        unit->defense += 3; // temporary defense bonus
        unit->hasActed = true;
        addLog(unit->name + " defends (+3 Defense this round)");
        advanceTurn();
        return true;
    }
    default:
        return false;
    }
}

void CombatEngine::wait()
{
    CombatUnit* unit = activeUnit();
    if (!unit) return;
    unit->waitUsed = true;
    addLog(unit->name + " waits");
    m_waitQueue.push_back(unit->id);
    advanceTurn();
}

void CombatEngine::skipUnit()
{
    CombatUnit* unit = activeUnit();
    if (unit) { unit->hasActed = true; unit->hasMoved = true; }
    advanceTurn();
}

// ── Simple AI ──────────────────────────────────────────────────────────────────
void CombatEngine::processAITurn()
{
    while (m_phase == CombatPhase::EnemyTurn) {
        CombatUnit* unit = activeUnit();
        if (!unit || unit->isPlayer) break;
        aiActUnit(*unit);
    }
}

void CombatEngine::aiActUnit(CombatUnit& unit)
{
    AIDifficulty diff = unit.isPlayer ? m_playerAI : m_enemyAI;
    switch (diff) {
    case AIDifficulty::Passive:  aiActPassive(unit);  break;
    case AIDifficulty::Standard: aiActStandard(unit); break;
    case AIDifficulty::Tactical: aiActTactical(unit); break;
    }
}

// ── Passive: attack a random visible enemy ─────────────────────────────────────
void CombatEngine::aiActPassive(CombatUnit& unit)
{
    std::vector<CombatUnit*> enemies;
    for (auto& u : m_grid.units())
        if (u.alive && u.isPlayer != unit.isPlayer) enemies.push_back(&u);
    if (enemies.empty()) { skipUnit(); return; }

    CombatUnit* target = enemies[static_cast<size_t>(rand()) % enemies.size()];
    int dist = HexGrid::distance(unit.pos, target->pos);

    if (dist == 1) {
        auto result = DamageCalc::attack(unit, *target, m_grid);
        std::ostringstream ss;
        ss << unit.name << " attacks " << target->name << " for " << result.damage;
        addLog(ss.str());
        if (!target->alive) { addLog(target->name + " destroyed!"); m_grid.removeDeadUnits(); }
        unit.hasActed = true; advanceTurn(); return;
    }

    auto melee = m_grid.meleePositions(target->pos);
    if (!melee.empty()) {
        auto path = m_grid.findPath(unit.pos, melee[0], unit.flying);
        if (!path.empty()) { m_grid.moveUnit(unit.id, path[0]); unit.hasMoved = true; }
    }
    unit.hasActed = true; advanceTurn();
}

// ── Standard: nearest enemy, move/attack ──────────────────────────────────────
void CombatEngine::aiActStandard(CombatUnit& unit)
{
    CombatUnit* target = nullptr;
    int bestDist = 9999;
    for (auto& u : m_grid.units()) {
        if (!u.alive || u.isPlayer == unit.isPlayer) continue;
        int d = HexGrid::distance(unit.pos, u.pos);
        if (d < bestDist) { bestDist = d; target = &u; }
    }
    if (!target) { skipUnit(); return; }

    if (bestDist == 1) {
        auto result = DamageCalc::attack(unit, *target, m_grid);
        std::ostringstream ss;
        ss << unit.name << " attacks " << target->name << " for " << result.damage << " dmg";
        if (result.killed) ss << " (" << result.killed << " killed)";
        addLog(ss.str());
        if (!target->alive) { addLog(target->name + " destroyed!"); m_grid.removeDeadUnits(); }
        unit.hasActed = true; advanceTurn(); return;
    }

    auto melee = m_grid.meleePositions(target->pos);
    if (!melee.empty()) {
        HexCoord best = melee[0]; int d = HexGrid::distance(unit.pos, best);
        for (auto& h : melee) { int nd = HexGrid::distance(unit.pos, h); if (nd < d) { d = nd; best = h; } }
        auto path = m_grid.findPath(unit.pos, best, unit.flying);
        if (!path.empty()) { m_grid.moveUnit(unit.id, path[0]); unit.hasMoved = true; addLog(unit.name + " moves toward " + target->name); }
    }
    unit.hasActed = true; advanceTurn();
}

// ── Tactical: focus weakest stack, ranged units kite back ─────────────────────
void CombatEngine::aiActTactical(CombatUnit& unit)
{
    // Ranged units: move away from melee threats, shoot lowest-HP enemy in range
    if (unit.range > 0 && unit.shotsLeft > 0) {
        // Find closest enemy threatening melee
        CombatUnit* threat = nullptr;
        int threatDist = 9999;
        for (auto& u : m_grid.units()) {
            if (!u.alive || u.isPlayer == unit.isPlayer || u.range > 0) continue;
            int d = HexGrid::distance(unit.pos, u.pos);
            if (d < threatDist) { threatDist = d; threat = &u; }
        }

        // If melee threat is very close (≤2), try to back away
        if (threat && threatDist <= 2 && !unit.hasMoved) {
            // Pick the hex adjacent to us that maximises distance from threat
            auto neighbours = HexGrid::neighbors(unit.pos);
            HexCoord best = unit.pos; int bestD = threatDist;
            for (auto& h : neighbours) {
                if (!m_grid.inBounds(h)) continue;
                auto* tile = m_grid.getTile(h);
                if (!tile || tile->occupied || tile->type == CombatTileType::Obstacle) continue;
                int nd = HexGrid::distance(h, threat->pos);
                if (nd > bestD) { bestD = nd; best = h; }
            }
            if (!(best == unit.pos)) {
                m_grid.moveUnit(unit.id, best); unit.hasMoved = true;
                addLog(unit.name + " falls back");
            }
        }

        // Shoot enemy with lowest total HP
        CombatUnit* shtTarget = nullptr;
        int lowestHp = INT32_MAX;
        for (auto& u : m_grid.units()) {
            if (!u.alive || u.isPlayer == unit.isPlayer) continue;
            if (u.totalHp() < lowestHp) { lowestHp = u.totalHp(); shtTarget = &u; }
        }
        if (shtTarget) {
            auto result = DamageCalc::attack(unit, *shtTarget, m_grid);
            std::ostringstream ss;
            ss << unit.name << " shoots " << shtTarget->name << " for " << result.damage;
            addLog(ss.str());
            if (!shtTarget->alive) { addLog(shtTarget->name + " destroyed!"); m_grid.removeDeadUnits(); }
            unit.hasActed = true; advanceTurn(); return;
        }
    }

    // Melee: focus weakest enemy stack by total HP
    CombatUnit* target = nullptr;
    int lowestHp = INT32_MAX;
    int bestDist = 9999;
    for (auto& u : m_grid.units()) {
        if (!u.alive || u.isPlayer == unit.isPlayer) continue;
        int hp = u.totalHp();
        int d  = HexGrid::distance(unit.pos, u.pos);
        // Prefer adjacent weak targets; if none adjacent, pick weakest reachable
        if (d == 1 && hp < lowestHp) { lowestHp = hp; bestDist = d; target = &u; }
        else if (bestDist > 1 && hp < lowestHp) { lowestHp = hp; bestDist = d; target = &u; }
    }
    if (!target) { skipUnit(); return; }

    if (bestDist == 1) {
        auto result = DamageCalc::attack(unit, *target, m_grid);
        std::ostringstream ss;
        ss << unit.name << " attacks " << target->name << " for " << result.damage << " dmg";
        if (result.killed) ss << " (" << result.killed << " killed)";
        addLog(ss.str());
        if (!target->alive) { addLog(target->name + " destroyed!"); m_grid.removeDeadUnits(); }
        unit.hasActed = true; advanceTurn(); return;
    }

    auto melee = m_grid.meleePositions(target->pos);
    if (!melee.empty()) {
        HexCoord best = melee[0]; int d = HexGrid::distance(unit.pos, best);
        for (auto& h : melee) { int nd = HexGrid::distance(unit.pos, h); if (nd < d) { d = nd; best = h; } }
        auto path = m_grid.findPath(unit.pos, best, unit.flying);
        if (!path.empty()) { m_grid.moveUnit(unit.id, path[0]); unit.hasMoved = true; }
    }
    unit.hasActed = true; advanceTurn();
}

// ── Headless simulation ────────────────────────────────────────────────────────
CombatPhase CombatEngine::runHeadless(int maxRounds)
{
    // Process all player-side units using the same AI as the enemy
    auto processPlayerAI = [this]() {
        while (m_phase == CombatPhase::PlayerTurn) {
            CombatUnit* unit = activeUnit();
            if (!unit || !unit->isPlayer) break;
            aiActUnit(*unit);
        }
    };

    // Initial turn may already be PlayerTurn after startBattle
    if (m_phase == CombatPhase::PlayerTurn)
        processPlayerAI();

    int safety = maxRounds * 300;
    while (--safety > 0 && m_round <= maxRounds) {
        if (m_phase == CombatPhase::Victory || m_phase == CombatPhase::Defeat)
            break;
        if (m_phase == CombatPhase::EnemyTurn)
            processAITurn();
        if (m_phase == CombatPhase::Victory || m_phase == CombatPhase::Defeat)
            break;
        if (m_phase == CombatPhase::PlayerTurn)
            processPlayerAI();
    }
    return m_phase;
}

// ── Tile effects on entry ──────────────────────────────────────────────────────
void CombatEngine::applyTileEffect(CombatUnit& unit)
{
    const CombatTile* tile = m_grid.getTile(unit.pos);
    if (!tile) return;
    if (tile->type == CombatTileType::Speed && !unit.moraleImmune)
        unit.morale = std::min(100, unit.morale + 5);
}

// ── Victory check ──────────────────────────────────────────────────────────────
void CombatEngine::checkVictory()
{
    bool playerAlive = false, enemyAlive = false;
    for (auto& u : m_grid.units()) {
        if (!u.alive) continue;
        if (u.isPlayer)  playerAlive = true;
        else             enemyAlive  = true;
    }
    if (!enemyAlive)  { m_phase = CombatPhase::Victory; addLog("VICTORY!"); }
    if (!playerAlive) { m_phase = CombatPhase::Defeat;  addLog("DEFEAT!"); }
}
