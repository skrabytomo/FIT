#include "CampaignHUD.h"
#include <imgui.h>
#include <cmath>
#include <cstdio>

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

void CampaignHUD::render(CampaignManager& mgr, LuaEngine& lua)
{
    const CampaignMission* mission = mgr.currentMission();
    if (!mission) return;

    // Auto-show briefing when mission changes
    if (mission->id != m_lastMissionId) {
        m_lastMissionId = mission->id;
        m_showBriefing  = true;
    }

    // ── Mission briefing popup ─────────────────────────────────────────────────
    if (m_showBriefing) {
        ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_Always);
        ImGui::SetNextWindowPos(
            ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f,
                   ImGui::GetIO().DisplaySize.y * 0.5f),
            ImGuiCond_Always, ImVec2(0.5f, 0.5f));

        if (ImGui::Begin("Mission Briefing", nullptr,
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoCollapse))
        {
            ImGui::TextColored(ImVec4(1,0.85f,0.3f,1), "%s", mission->name.c_str());
            ImGui::Separator();
            ImGui::TextWrapped("%s", mission->briefing.c_str());
            ImGui::Spacing();
            if (ImGui::Button("Begin Mission", ImVec2(-1, 0)))
                m_showBriefing = false;
        }
        ImGui::End();
        return;  // Don't render other panels while briefing is open
    }

    // ── Decision modal — blocks everything else ────────────────────────────────
    if (mgr.hasPendingDecision()) {
        drawDecisionModal(mgr, lua);
        return;
    }

    // ── End-campaign screen ────────────────────────────────────────────────────
    if (mgr.isCampaignOver()) {
        drawEndScreen(mgr, false);
        return;
    }

    // ── Objectives panel (top-left) ────────────────────────────────────────────
    ImGui::SetNextWindowPos(ImVec2(8, 8), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(260, 0), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.75f);
    if (ImGui::Begin("##objectives", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextColored(ImVec4(1,0.85f,0.3f,1), "%s", mission->name.c_str());
        ImGui::Separator();
        drawObjectives(*mission);
    }
    ImGui::End();

    // ── Alignment compass (top-right) ─────────────────────────────────────────
    float displayW = ImGui::GetIO().DisplaySize.x;
    ImGui::SetNextWindowPos(ImVec2(displayW - 180, 8), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(172, 180), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.75f);
    if (ImGui::Begin("##alignment", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove))
    {
        drawAlignmentCompass(mgr.alignment());
    }
    ImGui::End();
}

void CampaignHUD::drawObjectives(const CampaignMission& mission)
{
    for (const auto& obj : mission.objectives) {
        if (!obj.required) continue;
        ImVec4 col = obj.completed
            ? ImVec4(0.3f,1.0f,0.3f,1.0f)
            : ImVec4(0.9f,0.9f,0.9f,1.0f);
        ImGui::TextColored(col, "%s  %s",
            obj.completed ? "[x]" : "[ ]",
            obj.description.c_str());
    }

    bool anyBonus = false;
    for (const auto& obj : mission.objectives) if (!obj.required) { anyBonus = true; break; }
    if (anyBonus) {
        ImGui::Spacing();
        ImGui::TextDisabled("Bonus:");
        for (const auto& obj : mission.objectives) {
            if (obj.required) continue;
            ImVec4 col = obj.completed
                ? ImVec4(0.8f,1.0f,0.4f,1.0f)
                : ImVec4(0.6f,0.6f,0.6f,1.0f);
            ImGui::TextColored(col, "  %s  %s",
                obj.completed ? "[x]" : "[ ]",
                obj.description.c_str());
        }
    }
}

void CampaignHUD::drawAlignmentCompass(const AlignmentSystem& align)
{
    ImGui::TextDisabled("Alignment");
    ImGui::TextColored(ImVec4(1,0.85f,0.3f,1), "%s", align.getTitle());
    ImGui::Spacing();

    // Draw a 120×120 compass square using ImDrawList
    ImVec2 origin = ImGui::GetCursorScreenPos();
    const float sz = 120.0f;
    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Background grid
    draw->AddRectFilled(origin, ImVec2(origin.x+sz, origin.y+sz),
        IM_COL32(30,30,50,200));
    draw->AddRect(origin, ImVec2(origin.x+sz, origin.y+sz),
        IM_COL32(80,80,120,255));

    // Axis lines
    float cx = origin.x + sz * 0.5f;
    float cy = origin.y + sz * 0.5f;
    draw->AddLine({origin.x, cy}, {origin.x+sz, cy}, IM_COL32(60,60,80,255));
    draw->AddLine({cx, origin.y}, {cx, origin.y+sz}, IM_COL32(60,60,80,255));

    // Labels
    draw->AddText({origin.x+sz*0.5f-8, origin.y+2}, IM_COL32(200,200,255,200), "Light");
    draw->AddText({origin.x+sz*0.5f-8, origin.y+sz-14}, IM_COL32(200,100,100,200), "Dark");
    draw->AddText({origin.x+2, cy-8}, IM_COL32(200,200,255,200), "Order");
    draw->AddText({origin.x+sz-32, cy-8}, IM_COL32(200,200,255,200), "Chaos");

    // Player dot
    float nx = align.orderNorm();   // +1 = Order, -1 = Chaos
    float ny = align.lightNorm();   // +1 = Light, -1 = Dark
    // Map: order(+1) = left, chaos(-1) = right; light(+1) = top, dark(-1) = bottom
    float dotX = cx - nx * (sz * 0.45f);  // order is left axis
    float dotY = cy - ny * (sz * 0.45f);  // light is up

    draw->AddCircleFilled({dotX, dotY}, 6.0f, IM_COL32(255,220,50,255));
    draw->AddCircle({dotX, dotY}, 6.0f, IM_COL32(255,255,255,200));

    ImGui::Dummy(ImVec2(sz, sz));
    ImGui::Spacing();
    ImGui::TextWrapped("%s", align.getDescription());
}

void CampaignHUD::drawDecisionModal(CampaignManager& mgr, LuaEngine& lua)
{
    const CampaignDecision* dec = mgr.pendingDecision();
    if (!dec) return;

    ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_Always);
    ImGui::SetNextWindowPos(
        ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f,
               ImGui::GetIO().DisplaySize.y * 0.5f),
        ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse;
    if (ImGui::Begin(dec->prompt.c_str(), nullptr, flags)) {
        ImGui::TextWrapped("%s", dec->contextText.c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        for (int i = 0; i < static_cast<int>(dec->choices.size()); ++i) {
            const auto& c = dec->choices[i];

            // Alignment delta badge — colour-coded
            char badge[32] = {};
            int ord = c.alignment.orderDelta;
            int lgt = c.alignment.lightDelta;
            if (ord != 0 || lgt != 0) {
                std::snprintf(badge, sizeof(badge), " [%s%d / %s%d]",
                    ord >= 0 ? "+" : "", ord,
                    lgt >= 0 ? "+" : "", lgt);
            }

            std::string btnLabel = c.label + badge;
            ImGui::PushID(i);

            // Colour the delta badge: Order=blue-ish, Light=warm
            bool hasGoodOrder = ord > 0;
            bool hasBadOrder  = ord < 0;
            bool hasGoodLight = lgt > 0;
            bool hasBadLight  = lgt < 0;
            bool allNeg = (ord < 0 && lgt < 0);

            if (allNeg)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f,0.1f,0.1f,0.9f));
            else if (hasGoodOrder && hasGoodLight)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f,0.35f,0.45f,0.9f));
            else
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.2f,0.3f,0.9f));

            if (ImGui::Button(btnLabel.c_str(), ImVec2(-1, 0)))
                mgr.resolveDecision(i, lua);
            ImGui::PopStyleColor();

            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(c.tooltip.c_str());
                if (ord != 0 || lgt != 0) {
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f,0.85f,1.0f,1.0f),
                        "Order  %+d", ord);
                    ImGui::TextColored(ImVec4(1.0f,0.9f,0.6f,1.0f),
                        "Light  %+d", lgt);
                }
                ImGui::EndTooltip();
            }
            ImGui::PopID();
            ImGui::Spacing();
        }
    }
    ImGui::End();
}

void CampaignHUD::drawEndScreen(CampaignManager& mgr, bool convergenceEligible)
{
    ImGui::SetNextWindowSize(ImVec2(480, 0), ImGuiCond_Always);
    ImGui::SetNextWindowPos(
        ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f,
               ImGui::GetIO().DisplaySize.y * 0.5f),
        ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
    if (ImGui::Begin("##endscreen", nullptr, flags)) {
        if (mgr.playerWon()) {
            ImGui::TextColored(ImVec4(1,0.85f,0.3f,1), "CAMPAIGN COMPLETE");
        } else {
            ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "CAMPAIGN FAILED");
        }
        ImGui::Separator();
        ImGui::Spacing();

        const auto& align = mgr.alignment();
        ImGui::Text("Final Alignment: %s", align.getTitle());
        ImGui::TextWrapped("%s", align.getDescription());
        ImGui::Spacing();

        FactionId unlock = mgr.unlockedFaction(convergenceEligible);
        ImGui::TextColored(ImVec4(0.4f,1.0f,0.6f,1),
            "Faction Unlocked: %s", factionName(unlock));

        if (unlock == FactionId::Convergence) {
            ImGui::TextColored(ImVec4(0.8f,0.6f,1.0f,1),
                "(Secret faction — Hideout conditions met)");
        }

        ImGui::Spacing();
        drawAlignmentCompass(align);
    }
    ImGui::End();
}
