#include "NerveERROR.h"

#include <imgui.h>
#include <cmath>


EvaErrorLayer* g_ErrorLayer = nullptr;

#define IM_PI  M_PI

// =========================================================
// CONSTRUCTOR
// =========================================================
EvaErrorLayer::EvaErrorLayer()
{
    g_ErrorLayer = this;
    GenerateField();
}

// =========================================================
// PUBLIC API
// =========================================================
void EvaErrorLayer::TriggerError(const std::string& msg)
{
    m_Message = msg;
    m_Active = true;

    m_Time = 0.0f;
    m_ATExpansion = 0.0f;
    m_Dismiss = false;
    m_DismissProgress = 0.0f;
    m_Flash = 1.0f;
}

// =========================================================
// MAIN RENDER
// =========================================================
void EvaErrorLayer::OnUIRender()
{
    //if (!m_Resetting) {
    //        return;
   // }
    //if (!m_Active)
    //    return;

float dt = ImGui::GetIO().DeltaTime;

if (!m_Active && !m_Resetting)
    return;

m_Time += dt;

if (m_Resetting)
{
    m_ResetTimer += dt;

    float t = m_ResetTimer / m_ResetDuration;

    if (t > 1.0f)
        t = 1.0f;

    // collapse factor
    float k = 1.0f - (t * t);

    m_ATExpansion *= k;
    m_InfectionTime *= k;

    for (auto& h : m_Hexes)
    {
        h.Infection *= k;
    }

    if (m_ResetTimer >= m_ResetDuration)
    {
        // TRUE FINAL RESET
        m_Resetting = false;
        m_Active = false;
        m_Dismiss = false;

        m_Time = 0.0f;
        m_ATExpansion = 0.0f;
        m_DismissProgress = 0.0f;
        m_Flash = 0.0f;
        m_InfectionTime = 0.0f;

        for (auto& h : m_Hexes)
        {
            h.Infection = 0.0f;
        }

        return;
    }
}
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImDrawList* draw = ImGui::GetForegroundDrawList();

    ImVec2 origin = vp->Pos;
    ImVec2 size   = vp->Size;

    ImVec2 center =
    {
        origin.x + size.x * 0.5f,
        origin.y + size.y * 0.5f
    };

    // =========================================================
    // FULL INPUT BLOCKER WINDOW (modal behavior)
    // =========================================================
    ImGui::SetNextWindowPos(origin);
    ImGui::SetNextWindowSize(size);

    //ImGui::Begin(
    //    "##eva_blocker",
    //    nullptr,
    //    ImGuiWindowFlags_NoDecoration |
     //   ImGuiWindowFlags_NoMove |
    //    ImGuiWindowFlags_NoSavedSettings |
     //   ImGuiWindowFlags_NoBackground |
    //    ImGuiWindowFlags_NoBringToFrontOnFocus
    //);

    ImGui::InvisibleButton("##full_block", size);




    





    // =========================================================
    // AT FIELD EXPANSION
    // =========================================================
    m_ATExpansion += dt * 2.5f;
    float at = EaseOutExpo(m_ATExpansion);
    float fieldRadius = at * (size.x * 0.6f);

    DrawATField(draw, center, fieldRadius);

    // =========================================================
    // DARK OVERLAY (intensity increases with AT field)
    // =========================================================
    int alpha = (int)(120 + at * 110);
    if (alpha > 230) alpha = 230;

    draw->AddRectFilled(
        origin,
        ImVec2(origin.x + size.x, origin.y + size.y),
        IM_COL32(10, 0, 0, alpha)
    );

    // =========================================================
    // INFECTION WAVE UPDATE
    // =========================================================
    m_InfectionTime += dt * m_WaveSpeed;

    // =========================================================
    // HEX FIELD RENDER
    // =========================================================
    for (auto& h : m_Hexes)
    {
        float dx = h.BasePos.x - center.x;
        float dy = h.BasePos.y - center.y;

        float dist = sqrtf(dx * dx + dy * dy);

        float target = (dist < m_InfectionTime) ? 1.0f : 0.0f;

        // smooth infection (IMPORTANT for EVA feel)
        h.Infection += (target - h.Infection) * dt * 3.5f;

        float t = h.Infection;

        // subtle corruption distortion
        float wobble =
            sinf(m_Time * 3.0f + h.Phase) * t * 3.0f;

        ImVec2 pos =
        {
            h.BasePos.x + wobble,
            h.BasePos.y + wobble
        };

        // =====================================================
        // NOT INFECTED (clean grid)
        // =====================================================
        if (t < 0.25f)
        {
            DrawHex(
                draw,
                pos,
                h.Radius,
                IM_COL32(255, 90, 0, 160),
                1.8f
            );
        }
        // =====================================================
        // INFECTED (EVA ERROR HEX)
        // =====================================================
        else
        {
            // glowing red fill
            ImU32 fill =
                IM_COL32(
                    255,
                    30,
                    30,
                    (int)(40 + 180 * t)
                );

            DrawFilledHex(draw, pos, h.Radius * 0.95f, fill);

            // outer shell
            DrawHex(
                draw,
                pos,
                h.Radius,
                IM_COL32(255, 80, 80, 220),
                2.2f
            );

            // =================================================
            // CENTER TEXT
            // =================================================
            const char* text = "ERROR";

            ImVec2 textSize = ImGui::CalcTextSize(text);

            ImVec2 textPos =
            {
                pos.x - textSize.x * 0.5f,
                pos.y - textSize.y * 0.5f
            };

            draw->AddText(
                nullptr,
                22.0f,
                textPos,
                IM_COL32(0, 0, 0, 255),
                text
            );

            // =================================================
            // TRIANGLES (EVA UI SIGNATURE)
            // =================================================
            float tri = 6.0f + t * 6.0f;

            DrawTriangleUp(
                draw,
                { pos.x, textPos.y - 10 },
                tri
            );

            DrawTriangleDown(
                draw,
                { pos.x, textPos.y + 25 },
                tri
            );
        }
    }

    // =========================================================
    // CENTRAL ERROR CORE (click to dismiss)
    // =========================================================
    const char* msg = m_Message.c_str();

    ImVec2 msgSize = ImGui::CalcTextSize(msg);

    ImVec2 msgPos =
    {
        center.x - msgSize.x * 0.5f,
        center.y - msgSize.y * 0.5f
    };

    ImGui::SetCursorScreenPos(msgPos);
    ImGui::InvisibleButton("##error_core", msgSize);


    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            m_Dismiss = true;
        }
    if (ImGui::IsItemClicked())
    {
        m_Dismiss = true;
    }
    draw->AddText(
        nullptr,
        42.0f,
        msgPos,
        IM_COL32(255, 140, 0, 255),
        msg
    );

    // =========================================================
    // DISMISS ANIMATION
    // =========================================================
    if (m_Dismiss)
    {
        m_DismissProgress += dt * 2.5f;

        if (m_DismissProgress > 1.0f)
        {
            m_Active = false;
            Reset();
        }
    }

    //ImGui::End();
}

void EvaErrorLayer::DrawATField(ImDrawList* draw, ImVec2 c, float r)
{
    if (r <= 0.0f)
        return;

    for (int i = 0; i < 24; i++)
    {
        float a = (IM_PI * 2.0f) * (i / 24.0f);

        ImVec2 p =
        {
            c.x + cosf(a) * r,
            c.y + sinf(a) * r
        };

        draw->AddCircle(p, 2.0f, IM_COL32(255, 120, 0, 120), 8, 1.5f);
    }
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
            //h.Radius = 32.0f;
            h.Radius = 50.0f;
            h.Phase = (float)((x * 31 + y * 17) % 100) * 0.1f;
            h.Infection = 0.0f;

            m_Hexes.push_back(h);
        }
    }
}

// =========================================================
// DRAW HELPERS
// =========================================================
void EvaErrorLayer::DrawHex(ImDrawList* draw, ImVec2 c, float r, ImU32 col, float t)
{
    ImVec2 v[6];

    for (int i = 0; i < 6; i++)
    {
        float a = IM_PI / 3.0f * i + IM_PI / 6.0f;

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

void EvaErrorLayer::DrawFilledHex(ImDrawList* draw, ImVec2 c, float r, ImU32 col)
{
    ImVec2 v[6];

    for (int i = 0; i < 6; i++)
    {
        float a = IM_PI / 3.0f * i + IM_PI / 6.0f;

        v[i] =
        {
            c.x + cosf(a) * r,
            c.y + sinf(a) * r
        };
    }

    draw->AddConvexPolyFilled(v, 6, col);
}

void EvaErrorLayer::DrawTriangleUp(ImDrawList* draw, ImVec2 c, float s)
{
    ImVec2 v[3] =
    {
        { c.x, c.y - s },
        { c.x - s, c.y + s },
        { c.x + s, c.y + s }
    };

    draw->AddConvexPolyFilled(v, 3, IM_COL32(0, 0, 0, 255));
}

void EvaErrorLayer::DrawTriangleDown(ImDrawList* draw, ImVec2 c, float s)
{
    ImVec2 v[3] =
    {
        { c.x, c.y + s },
        { c.x - s, c.y - s },
        { c.x + s, c.y - s }
    };

    draw->AddConvexPolyFilled(v, 3, IM_COL32(0, 0, 0, 255));
}

// =========================================================
// EASING
// =========================================================
float EvaErrorLayer::EaseOutExpo(float x)
{
    return (x >= 1.0f) ? 1.0f : 1.0f - powf(2.0f, -10.0f * x);
}

//
//void EvaErrorLayer::Reset()
//{
    /*
    m_Active = false;
    m_Dismiss = false;

    m_Time = 0.0f;
    m_ATExpansion = 0.0f;
    m_DismissProgress = 0.0f;
    m_Flash = 0.0f;
    m_InfectionTime = 0.0f;

    m_Message = "ERROR";

    // reset all hex states
    for (auto& h : m_Hexes)
    {
        h.Infection = 0.0f;
        for (auto& h : m_Hexes)
            {
                h.Phase = rand() % 100 * 0.1f;
            }
    }*/
//    m_Resetting = false;
//    m_ResetTimer = 0.0f;

//    m_Dismiss = false; // reuse your dismiss pipeline

//    m_Flash = 1.0f;
//}

void EvaErrorLayer::Reset()
{
    if (m_Resetting)
        return;

    m_Resetting = true;
    m_ResetTimer = 0.0f;
}