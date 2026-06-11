#include "Simulator.h"
#include "SimTypes.h"
#include <cstdio>
#include <chrono>

int main(int argc, char* argv[])
{
    // Parse optional args: [weeks] [battles] [seed]
    int  weeks    = (argc > 1) ? std::atoi(argv[1]) : 6;
    int  battles  = (argc > 2) ? std::atoi(argv[2]) : 500;
    int  seed     = (argc > 3) ? std::atoi(argv[3]) : 42;

    printf("=== Combat Balance Simulator ===\n");
    printf("Weeks: %d  |  Battles per matchup: %d  |  Seed: %d\n\n", weeks, battles, seed);

    // Run early game (week 4) and the requested week
    int passes = (weeks != 4) ? 2 : 1;
    int passWeeks[] = { 4, weeks };

    for (int p = 0; p < passes; ++p) {
        int w = passWeeks[p];
        if (p == 1 && w == 4) continue; // skip duplicate

        printf("--- Week %d snapshot ---\n", w);

        SimConfig cfg;
        cfg.allVsAll   = true;
        cfg.weeks      = w;
        cfg.numBattles = battles;
        cfg.seed       = static_cast<uint32_t>(seed);
        cfg.side1AI    = AIDifficulty::Tactical;
        cfg.side2AI    = AIDifficulty::Tactical;

        auto t0 = std::chrono::steady_clock::now();

        int lastPct = -1;
        SimResult r = Simulator::run(cfg, [&](int done, int total) {
            int pct = done * 100 / total;
            if (pct != lastPct) {
                printf("\r  Progress: %d%%   ", pct);
                fflush(stdout);
                lastPct = pct;
            }
        });

        auto t1  = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        printf("\r  Done in %.0f ms\n\n", ms);

        printf("%s\n", r.balanceReport.c_str());
    }

    return 0;
}
