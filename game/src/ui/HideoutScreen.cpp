#include "HideoutScreen.h"
#include <imgui.h>
#include <cstdio>

// XP cost to unlock each tier, indexed by (tier - 1)
static constexpr int CASTLE_COSTS[]   = { 100, 300, 600 };
static constexpr int BARRACKS_COSTS[] = { 150, 400 };
static constexpr int VAULT_COSTS[]    = { 200, 500 };
static constexpr int SHRINE_COSTS[]   = { 250 };
static constexpr int SANCTUM_COSTS[]  = { 400 };

// Approximate XP needed for the next level (mirrors HideoutDB XP pool)
static constexpr int XP_DISPLAY_MAX = 1000;

void HideoutScreen::draw(HideoutDB& db, bool& open)
{
    if (!open) return;
    if (!db.isOpen()) {
        ImGui::Begin("Hideout", &open);
        ImGui::TextDisabled("Database not available.");
        ImGui::End();
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(480, 520), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Hideout  [F6]", &open)) { ImGui::End(); return; }

    drawXPBar(db);
    ImGui::Separator();

    if (ImGui::BeginTabBar("HideoutTabs")) {
        if (ImGui::BeginTabItem("Upgrades")) {
            drawBranch(db, HideoutBranch::CASTLE,   "Castle",   3, CASTLE_COSTS);
            drawBranch(db, HideoutBranch::BARRACKS, "Barracks", 2, BARRACKS_COSTS);
            drawBranch(db, HideoutBranch::VAULT,    "Vault",    2, VAULT_COSTS);
            drawBranch(db, HideoutBranch::SHRINE,   "Shrine",   1, SHRINE_COSTS);
            drawBranch(db, HideoutBranch::SANCTUM,  "Sanctum",  1, SANCTUM_COSTS);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Milestones")) {
            drawMilestones(db);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    // Convergence status
    ImGui::Separator();
    if (db.isConvergenceUnlocked()) {
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f),
                           "Convergence UNLOCKED — 9th faction accessible");
    } else {
        ImGui::TextDisabled("Convergence locked (need Castle T2 + Barracks T1 + Vault T1)");
    }

    ImGui::End();
}

void HideoutScreen::drawXPBar(HideoutDB& db)
{
    int xp = db.getXP();
    char buf[64];
    std::snprintf(buf, sizeof(buf), "XP: %d", xp);
    float frac = static_cast<float>(xp % XP_DISPLAY_MAX) / static_cast<float>(XP_DISPLAY_MAX);
    ImGui::ProgressBar(frac, ImVec2(-1, 18), buf);
    ImGui::TextDisabled("Total accumulated XP: %d", xp);
}

void HideoutScreen::drawBranch(HideoutDB& db, const char* branch, const char* label,
                                int maxTiers, const int tierCosts[])
{
    int current = db.getUpgradeLevel(branch);
    ImGui::PushID(branch);

    ImGui::Text("%s  [%d / %d tiers]", label, current, maxTiers);
    ImGui::SameLine();

    if (current >= maxTiers) {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "MAX");
    } else {
        int cost = tierCosts[current]; // current is 0-based index into costs
        char btnLabel[64];
        std::snprintf(btnLabel, sizeof(btnLabel), "Unlock T%d  (%d XP)##%s", current + 1, cost, branch);
        bool canAfford = db.canUnlockNextTier(branch, cost);
        if (!canAfford) ImGui::BeginDisabled();
        if (ImGui::Button(btnLabel)) {
            db.unlockNextTier(branch, cost);
        }
        if (!canAfford) ImGui::EndDisabled();
    }
    ImGui::PopID();
}

void HideoutScreen::drawMilestones(HideoutDB& db)
{
    struct MilestoneEntry { const char* key; const char* desc; };
    static const MilestoneEntry ENTRIES[] = {
        { Milestone::CASTLE_T1,          "Castle Tier 1 unlocked" },
        { Milestone::CASTLE_T2,          "Castle Tier 2 unlocked" },
        { Milestone::CASTLE_T3,          "Castle Tier 3 unlocked" },
        { Milestone::BARRACKS_T1,        "Barracks Tier 1 unlocked" },
        { Milestone::BARRACKS_T2,        "Barracks Tier 2 unlocked" },
        { Milestone::VAULT_T1,           "Vault Tier 1 unlocked" },
        { Milestone::VAULT_T2,           "Vault Tier 2 unlocked" },
        { Milestone::CONVERGENCE_UNLOCK, "Convergence faction unlocked" },
    };

    for (auto& e : ENTRIES) {
        bool done = db.isMilestoneComplete(e.key);
        if (done)
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[x] %s", e.desc);
        else
            ImGui::TextDisabled("[ ] %s", e.desc);
    }
}
