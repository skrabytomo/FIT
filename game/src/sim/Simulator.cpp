#include "Simulator.h"
#include "ArmyBuilder.h"
#include <cstdlib>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>

static const char* factionName(FactionId f)
{
    switch (f) {
    case FactionId::HolyOrder:      return "Holy Order";
    case FactionId::CrimsonWardens: return "Crimson Wardens";
    case FactionId::Thornkin:       return "Thornkin";
    case FactionId::EternalEmpire:  return "Eternal Empire";
    case FactionId::Bloodsworn:     return "Bloodsworn";
    case FactionId::Voidkin:        return "Voidkin";
    case FactionId::IronAssembly:   return "Iron Assembly";
    case FactionId::Amalgamate:     return "Amalgamate";
    case FactionId::Convergence:    return "Convergence";
    default:                        return "Unknown";
    }
}

FactionMatchup Simulator::runMatchup(FactionId f1, FactionId f2,
                                     int weeks, int numBattles,
                                     uint32_t baseSeed)
{
    FactionMatchup m;
    m.f1      = f1;
    m.f2      = f2;
    m.battles = numBattles;

    int    wins1 = 0;
    double totalRounds      = 0.0;
    double totalF1Survival  = 0.0;
    double totalF2Survival  = 0.0;
    int    f1WinCount       = 0;
    int    f2WinCount       = 0;

    auto army1Base = ArmyBuilder::buildArmy(f1, weeks);
    auto army2Base = ArmyBuilder::buildArmy(f2, weeks);
    Hero hero1     = ArmyBuilder::buildHero(f1, weeks);
    Hero hero2     = ArmyBuilder::buildHero(f2, weeks);

    CombatEngine engine;
    engine.setSilent(true);

    for (int b = 0; b < numBattles; ++b) {
        // Seed rand for this battle to get variance but reproducibility
        srand(baseSeed + static_cast<uint32_t>(b));

        engine.startBattle(hero1, army1Base, hero2, army2Base);
        CombatPhase result = engine.runHeadless(MAX_ROUNDS);

        totalRounds += engine.round();

        if (result == CombatPhase::Victory) {
            ++wins1;
            // Measure surviving HP fraction for f1
            double totalHp = 0.0, aliveHp = 0.0;
            for (auto& u : engine.grid().units()) {
                if (u.isPlayer) {
                    totalHp += static_cast<double>(u.maxHp) * u.count;
                    aliveHp += u.alive ? u.totalHp() : 0.0;
                }
            }
            if (totalHp > 0.0) { totalF1Survival += aliveHp / totalHp; ++f1WinCount; }
        } else if (result == CombatPhase::Defeat) {
            // Measure surviving HP fraction for f2
            double totalHp = 0.0, aliveHp = 0.0;
            for (auto& u : engine.grid().units()) {
                if (!u.isPlayer) {
                    totalHp += static_cast<double>(u.maxHp) * u.count;
                    aliveHp += u.alive ? u.totalHp() : 0.0;
                }
            }
            if (totalHp > 0.0) { totalF2Survival += aliveHp / totalHp; ++f2WinCount; }
        }
    }

    m.winRate1      = static_cast<float>(wins1) / static_cast<float>(numBattles);
    m.avgRounds     = static_cast<float>(totalRounds / numBattles);
    m.avgF1Survival = f1WinCount > 0 ? static_cast<float>(totalF1Survival / f1WinCount) : 0.0f;
    m.avgF2Survival = f2WinCount > 0 ? static_cast<float>(totalF2Survival / f2WinCount) : 0.0f;
    m.imbalanced    = std::abs(m.winRate1 - 0.5f) > IMBALANCE_THRESHOLD;

    return m;
}

std::string Simulator::buildReport(const SimResult& r)
{
    std::ostringstream ss;
    ss << "=== Balance Report — " << r.weeksSimulated << " weeks, "
       << r.battlesPerMatchup << " battles/matchup ===\n\n";

    int imbalanceCount = 0;
    for (int i = 0; i < 9; ++i)
        for (int j = i + 1; j < 9; ++j) {
            const auto& m = r.matchups[i][j];
            if (m.imbalanced) ++imbalanceCount;
        }

    ss << "Imbalanced matchups: " << imbalanceCount << " / 36\n\n";

    // List flagged matchups
    ss << "Flagged matchups (|win rate - 50%| > 15%):\n";
    bool any = false;
    for (int i = 0; i < 9; ++i) {
        for (int j = i + 1; j < 9; ++j) {
            const auto& m = r.matchups[i][j];
            if (!m.imbalanced) continue;
            any = true;
            float pct = m.winRate1 * 100.0f;
            ss << "  " << factionName(m.f1) << " vs " << factionName(m.f2)
               << "  →  " << std::fixed << std::setprecision(1) << pct << "% / "
               << (100.0f - pct) << "%"
               << "  avg " << std::setprecision(1) << m.avgRounds << " rounds\n";
        }
    }
    if (!any) ss << "  None — all matchups within balance range.\n";

    ss << "\nWin Rate Matrix (row = attacker, % = row faction win rate):\n";
    ss << std::setw(18) << " ";
    for (int j = 0; j < 9; ++j)
        ss << std::setw(8) << std::string(factionName(static_cast<FactionId>(j))).substr(0, 6);
    ss << "\n";

    for (int i = 0; i < 9; ++i) {
        ss << std::setw(18) << std::string(factionName(static_cast<FactionId>(i))).substr(0, 16);
        for (int j = 0; j < 9; ++j) {
            if (i == j) { ss << std::setw(8) << "  --  "; continue; }
            const auto& m = (i < j) ? r.matchups[i][j] : r.matchups[j][i];
            float wr = (i < j) ? m.winRate1 : (1.0f - m.winRate1);
            ss << std::setw(7) << std::fixed << std::setprecision(0)
               << (wr * 100.0f) << "%";
        }
        ss << "\n";
    }

    return ss.str();
}

SimResult Simulator::run(const SimConfig& cfg, ProgressCallback onProgress)
{
    SimResult result;
    result.weeksSimulated    = cfg.weeks;
    result.battlesPerMatchup = cfg.numBattles;

    int fi_start = cfg.allVsAll ? 0 : static_cast<int>(cfg.faction1);
    int fi_end   = cfg.allVsAll ? 9 : static_cast<int>(cfg.faction1) + 1;
    int fj_start = cfg.allVsAll ? 0 : static_cast<int>(cfg.faction2);
    int fj_end   = cfg.allVsAll ? 9 : static_cast<int>(cfg.faction2) + 1;

    int total = 0;
    if (cfg.allVsAll) {
        for (int i = 0; i < 9; ++i)
            for (int j = i; j < 9; ++j)
                ++total;
    } else {
        total = 1;
    }
    int done = 0;

    if (cfg.allVsAll) {
        for (int i = 0; i < 9; ++i) {
            for (int j = i; j < 9; ++j) {
                FactionId f1 = static_cast<FactionId>(i);
                FactionId f2 = static_cast<FactionId>(j);
                uint32_t seed = cfg.seed ^ (static_cast<uint32_t>(i) * 31 + static_cast<uint32_t>(j));
                auto m = runMatchup(f1, f2, cfg.weeks, cfg.numBattles, seed);
                result.matchups[i][j] = m;
                // Mirror: flip win rate for [j][i]
                FactionMatchup mirror = m;
                mirror.f1      = f2;
                mirror.f2      = f1;
                mirror.winRate1 = 1.0f - m.winRate1;
                std::swap(mirror.avgF1Survival, mirror.avgF2Survival);
                result.matchups[j][i] = mirror;
                ++done;
                if (onProgress) onProgress(done, total);
            }
        }
    } else {
        int i = static_cast<int>(cfg.faction1);
        int j = static_cast<int>(cfg.faction2);
        auto m = runMatchup(cfg.faction1, cfg.faction2,
                            cfg.weeks, cfg.numBattles, cfg.seed);
        result.matchups[i][j] = m;
        FactionMatchup mirror  = m;
        mirror.f1      = cfg.faction2;
        mirror.f2      = cfg.faction1;
        mirror.winRate1 = 1.0f - m.winRate1;
        std::swap(mirror.avgF1Survival, mirror.avgF2Survival);
        result.matchups[j][i]  = mirror;
        if (onProgress) onProgress(1, 1);
    }

    result.balanceReport = buildReport(result);
    result.done          = true;
    return result;
}
