

#include <Walnut/Layer.h>
#include "NerveERROR.h"
#include <imgui.h>

#include <vector>
#include <string>
#include <cmath>

EvaErrorLayer* g_ErrorLayer = nullptr;


EvaErrorLayer::EvaErrorLayer()
    {
        g_ErrorLayer = this;
        GenerateField();
    }

    // =========================================================
    // PUBLIC API — trigger this from anywhere
    // =========================================================
    void EvaErrorLayer::TriggerError(const std::string& msg)
    {
        m_Message = msg;
        m_Active = true;

        m_Time = 0.0f;
        m_DismissProgress = 0.0f;

        m_ATExpansion = 0.0f;
        m_Flash = 1.0f;
    }


    // =========================================================
    // WALNUT RENDER ENTRY
    // =========================================================
    void EvaErrorLayer::OnUIRender() 
    {
        if (!m_Active)
            return;

        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImDrawList* draw = ImGui::GetForegroundDrawList();
        ImGui::SetNextWindowPos(vp->Pos);
        ImGui::SetNextWindowSize(vp->Size);

        ImGui::Begin(
            "##eva_blocker",
            nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoBackground
        );

        float dt = ImGui::GetIO().DeltaTime;
        m_Time += dt;



        // IMPORTANT: fix "sliding window" bug
        ImVec2 origin = vp->Pos;
        ImVec2 size   = vp->Size;

        ImVec2 center =
        {
            origin.x + size.x * 0.5f,
            origin.y + size.y * 0.5f
        };

        // =====================================================
        // AT FIELD SPAWN EXPANSION (shockwave)
        // =====================================================
        m_ATExpansion += dt * 2.8f;
        float at = EaseOutExpo(m_ATExpansion);
        float fieldRadius = at * (size.x * 0.6f);

        DrawATField(draw, center, fieldRadius);

        // =====================================================
        // DARK OVERLAY (intensity grows with AT field)
        // =====================================================
        float alpha = 120 + at * 90;
        if (alpha > 220) alpha = 220;

        draw->AddRectFilled(
            origin,
            ImVec2(origin.x + size.x, origin.y + size.y),
            IM_COL32(10, 0, 0, (int)alpha)
        );

        // =====================================================
        // HEX FIELD
        // =====================================================
        for (auto& h : m_Hexes)
        {
            float pulse =
                sinf(m_Time * 2.5f + h.Phase);

            pulse = pulse * 0.5f + 0.5f;

            float distortion =
                sinf(m_Time * 1.5f + h.Phase) * at * 6.0f;

            ImVec2 pos =
            {
                h.BasePos.x + distortion,
                h.BasePos.y + distortion
            };

            float radius =
                h.Radius + pulse * 4.0f + at * 2.0f;

            ImU32 col =
                IM_COL32(
                    255,
                    80 + (int)(pulse * 120),
                    0,
                    120
                );

            DrawHex(draw, pos, radius, col, 2.0f);
        }

        // =====================================================
        // ERROR CORE (click to dismiss)
        // =====================================================
        const char* text = m_Message.c_str();

        ImVec2 textSize = ImGui::CalcTextSize(text);

        float corePulse = sinf(m_Time * 5.0f) * 0.5f + 0.5f;

        ImVec2 corePos =
        {
            center.x - textSize.x * 0.5f,
            center.y - textSize.y * 0.5f
        };

        ImVec2 coreMin = corePos;
        ImVec2 coreMax = ImVec2(corePos.x + textSize.x, corePos.y + textSize.y);

        // clickable region
        ImGui::SetCursorScreenPos(coreMin);
        ImGui::InvisibleButton("##eva_error_core", textSize);

        if (ImGui::IsItemClicked())
        {
            m_Dismiss = true;
        }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            m_Dismiss = true;
        }

        // glitch when near dismissal
        float glitch =
            m_Dismiss ? sinf(m_Time * 40.0f) * 6.0f : 0.0f;

        ImU32 glow = IM_COL32(255, 80, 0, 200);

        draw->AddText(
            nullptr,
            42.0f,
            ImVec2(corePos.x - 2 + glitch, corePos.y),
            glow,
            text
        );

        draw->AddText(
            nullptr,
            42.0f,
            ImVec2(corePos.x + 2 - glitch, corePos.y),
            glow,
            text
        );

        draw->AddText(
            nullptr,
            42.0f,
            corePos,
            IM_COL32(255, 160, 0, 255),
            text
        );

        // =====================================================
        // FOOTER WARNING TEXT
        // =====================================================
        draw->AddText(
            ImVec2(origin.x + 40, origin.y + size.y - 80),
            IM_COL32(255, 120, 0, 255),
            "AT FIELD BREACH DETECTED"
        );

        draw->AddText(
            ImVec2(origin.x + 40, origin.y + size.y - 60),
            IM_COL32(255, 80, 0, 255),
            "NEURAL SYNC UNSTABLE"
        );

        // =====================================================
        // DISSOLVE OUT
        // =====================================================
        if (m_Dismiss)
        {
            m_DismissProgress += dt * 2.5f;

            if (m_DismissProgress > 1.0f)
            {
                m_Active = false;
                m_Dismiss = false;
            }
        }
        ImGui::End();
    }


    // =========================================================
    // FIELD GENERATION
    // =========================================================
    void EvaErrorLayer::GenerateField()
    {
        float sx = 90.0f;
        float sy = 78.0f;

        for (int y = 0; y < 14; y++)
        {
            for (int x = 0; x < 22; x++)
            {
                float offset = (y % 2 == 0) ? 0.0f : sx * 0.5f;

                Hex h;
                h.BasePos = { x * sx + offset, y * sy };
                h.Radius = 32.0f;
                h.Phase = (float)((x * 31 + y * 17) % 100) * 0.1f;

                m_Hexes.push_back(h);
            }
        }
    }

    // =========================================================
    // DRAWING
    // =========================================================
    void EvaErrorLayer::DrawHex(ImDrawList* draw, ImVec2 c, float r, ImU32 col, float t)
    {
        ImVec2 v[6];

        for (int i = 0; i < 6; i++)
        {
            float a = M_PI / 3.0f * i + M_PI / 6.0f;

            v[i] =
            {
                c.x + cosf(a) * r,
                c.y + sinf(a) * r
            };
        }

        for (int i = 0; i < 6; i++)
        {
            int n = (i + 1) % 6;
            draw->AddLine(v[i], v[n], col, t);
        }
    }

    void EvaErrorLayer::DrawATField(ImDrawList* draw, ImVec2 c, float r)
    {
        if (r <= 0.0f) return;

        for (int i = 0; i < 24; i++)
        {
            float a = (M_PI * 2.0f) * (i / 24.0f);
            ImVec2 p =
            {
                c.x + cosf(a) * r,
                c.y + sinf(a) * r
            };

            draw->AddCircle(p, 2.0f, IM_COL32(255, 120, 0, 120), 8, 1.5f);
        }
    }

    // =========================================================
    // EASING
    // =========================================================
    float EvaErrorLayer::EaseOutExpo(float x)
    {
        return (x >= 1.0f) ? 1.0f : 1.0f - powf(2.0f, -10.0f * x);
    }