#pragma once
#include <string>
#include <array>
#include "../hero/Hero.h"

struct SimConfig
{
    FactionId faction1      = FactionId::HolyOrder;
    FactionId faction2      = FactionId::CrimsonWardens;
    bool      allVsAll      = false;   // run all 9x9 matchups
    int       weeks         = 4;       // campaign weeks simulated
    int       numBattles    = 1000;    // battles per matchup
    uint32_t  seed          = 42;
};

struct FactionMatchup
{
    FactionId f1            = FactionId::None;
    FactionId f2            = FactionId::None;
    int       battles       = 0;
    float     winRate1      = 0.0f;   // [0.0-1.0] faction1 win rate
    float     avgRounds     = 0.0f;
    float     avgF1Survival = 0.0f;   // surviving HP fraction on f1 wins
    float     avgF2Survival = 0.0f;
    bool      imbalanced    = false;  // |winRate1 - 0.5| > 0.15
};

struct SimResult
{
    int weeksSimulated   = 0;
    int battlesPerMatchup = 0;
    // 9x9 grid — matchups[i][j] = faction i vs faction j
    std::array<std::array<FactionMatchup, 9>, 9> matchups{};
    std::string balanceReport;
    bool        done = false;
};
