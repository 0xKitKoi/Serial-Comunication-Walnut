#pragma once

#include <Walnut/Layer.h>
#include <imgui.h>
#include <string>
#include <vector>

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
    };

    void OnUIRender() override;

    void TriggerError(const std::string& msg);
    bool IsActive() const { return m_Active; }

private:

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