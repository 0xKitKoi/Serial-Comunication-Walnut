#pragma once

#include <Walnut/Layer.h>
#include <imgui.h>
#include <string>
#include <vector>
#define _USE_MATH_DEFINES
#include <cmath>
#ifndef M_PI
#   define M_PI 3.1415926535897932384626433832
#endif

class EvaErrorLayer : public Walnut::Layer
{
public:
    EvaErrorLayer();
    ~EvaErrorLayer() override = default;

    struct Hex
    {
        ImVec2 BasePos;
        float Radius;
        float Phase;
        float Infection = 0.0f;
    };

    void OnUIRender() override;

    void TriggerError(const std::string& msg);
    bool IsActive() const { return m_Active; }

private:

    float m_InfectionTime = 0.0f;
    float m_WaveSpeed = 500.0f;//140.0f;

    bool m_Resetting = false;
    float m_ResetTimer = 0.0f;
    float m_ResetDuration = 0.33f;//0.55f;

    void Reset();
    void DrawFilledHex(ImDrawList* draw, ImVec2 c, float r, ImU32 col);
    void DrawTriangleUp(ImDrawList* draw, ImVec2 c, float s);
    void DrawTriangleDown(ImDrawList* draw, ImVec2 c, float s);
    // =========================================================
    // STATE
    // =========================================================
    std::vector<Hex> m_Hexes;

    std::string m_Message = "ERROR";

    bool m_Active = false;
    bool m_Dismiss = false;

    float m_Time = 0.0f;
    float m_ATExpansion = 0.0f;
    float m_DismissProgress = 0.0f;
    float m_Flash = 0.0f;


    void GenerateField();
    void DrawHex(ImDrawList* draw, ImVec2 c, float r, ImU32 col, float t);
    void DrawATField(ImDrawList* draw, ImVec2 c, float r);
    float EaseOutExpo(float x);
};