#pragma once
#include "Walnut/Layer.h"
#include "imgui.h"
#include <cmath>
#include <cstdio>

// Paste the EvaSignalConnector class here
class EvaSignalConnector {
public:
    const char* label;
    bool isConnected;
    float animProgress;
    int signalID;
    
    EvaSignalConnector(const char* lbl, int id) 
        : label(lbl), isConnected(false), animProgress(0.0f), signalID(id) {}
    
    void Update(float dt) {
        if (isConnected) {
            animProgress = std::min(1.0f, animProgress + dt * 3.0f);
        } else {
            animProgress = std::max(0.0f, animProgress - dt * 2.0f);
        }
    }
    
    void Render(ImVec2 pos, bool asButton = false) {
        ///ImDrawList* draw = ImGui::GetDrawList();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        
        const float connectorWidth = 40.0f;
        const float connectorHeight = 20.0f;
        const float nubWidth = 8.0f;
        const float wireLength = 80.0f;
        const float gap = 20.0f;
        
        float separation = gap * (1.0f - animProgress);
        float waveAmount = (1.0f - animProgress) * 5.0f;
        
        ImU32 connectorColor = IM_COL32(60, 60, 80, 255);
        ImU32 wireColor = IM_COL32(100, 100, 120, 255);
        ImU32 textColor = isConnected ? 
            IM_COL32(0, 255, 100, 255) : 
            IM_COL32(255, 50, 50, 255);
        
        // Male connector
        ImVec2 maleStart = pos;
        ImVec2 maleEnd = ImVec2(pos.x + connectorWidth, pos.y + connectorHeight);
        draw->AddRectFilled(maleStart, maleEnd, connectorColor);
        draw->AddRect(maleStart, maleEnd, IM_COL32(150, 150, 180, 255));
        
        // Male nub
        ImVec2 nubStart = ImVec2(maleEnd.x, pos.y + connectorHeight * 0.3f);
        ImVec2 nubEnd = ImVec2(maleEnd.x + nubWidth, pos.y + connectorHeight * 0.7f);
        draw->AddRectFilled(nubStart, nubEnd, connectorColor);
        draw->AddRect(nubStart, nubEnd, IM_COL32(150, 150, 180, 255));
        
        // Female connector
        float femaleX = pos.x + connectorWidth + nubWidth + wireLength + separation;
        ImVec2 femaleStart = ImVec2(femaleX, pos.y);
        ImVec2 femaleEnd = ImVec2(femaleX + connectorWidth, pos.y + connectorHeight);
        draw->AddRectFilled(femaleStart, femaleEnd, connectorColor);
        draw->AddRect(femaleStart, femaleEnd, IM_COL32(150, 150, 180, 255));
        
        // Female socket
        ImVec2 socketStart = ImVec2(femaleStart.x - nubWidth, pos.y + connectorHeight * 0.3f);
        ImVec2 socketEnd = ImVec2(femaleStart.x, pos.y + connectorHeight * 0.7f);
        draw->AddRectFilled(socketStart, socketEnd, IM_COL32(30, 30, 50, 255));
        draw->AddRect(socketStart, socketEnd, IM_COL32(100, 100, 120, 255));
        
        // Wire
        ImVec2 wireStart = ImVec2(maleEnd.x + nubWidth, pos.y + connectorHeight * 0.5f);
        ImVec2 wireEnd = ImVec2(femaleStart.x - nubWidth, pos.y + connectorHeight * 0.5f);
        
        if (waveAmount > 0.1f) {
            ImVec2 prev = wireStart;
            int segments = 20;
            for (int i = 1; i <= segments; i++) {
                float t = (float)i / segments;
                float x = wireStart.x + (wireEnd.x - wireStart.x) * t;
                float wave = sin(t * 3.14159f * 2.0f) * waveAmount;
                ImVec2 point = ImVec2(x, wireStart.y + wave);
                draw->AddLine(prev, point, wireColor, 2.0f);
                prev = point;
            }
        } else {
            draw->AddLine(wireStart, wireEnd, wireColor, 2.0f);
        }
        
        // Center block
        if (animProgress > 0.1f) {
            float centerX = pos.x + connectorWidth + nubWidth + wireLength * 0.5f;
            float blockWidth = 60.0f * animProgress;
            
            ImVec2 blockStart = ImVec2(centerX - blockWidth * 0.5f, pos.y - 2.5f);
            ImVec2 blockEnd = ImVec2(centerX + blockWidth * 0.5f, pos.y + connectorHeight + 2.5f);
            
            draw->AddRectFilled(blockStart, blockEnd, IM_COL32(40, 40, 60, 200));
            draw->AddRect(blockStart, blockEnd, IM_COL32(150, 150, 180, 255));
            
            char idText[16];
            snprintf(idText, sizeof(idText), "%05d", signalID);
            ImVec2 textSize = ImGui::CalcTextSize(idText);
            ImVec2 textPos = ImVec2(
                centerX - textSize.x * 0.5f,
                pos.y + connectorHeight * 0.5f - textSize.y * 0.5f
            );
            draw->AddText(textPos, textColor, idText);
        }
        
        // Button interaction
        if (asButton) {
            ImVec2 buttonStart = ImVec2(pos.x, pos.y - 5);
            ImVec2 buttonEnd = ImVec2(femaleEnd.x, femaleEnd.y + 5);
            
            ImGui::SetCursorScreenPos(buttonStart);
            ImGui::InvisibleButton(label, ImVec2(buttonEnd.x - buttonStart.x, buttonEnd.y - buttonStart.y));
            
            if (ImGui::IsItemHovered()) {
                draw->AddRect(buttonStart, buttonEnd, IM_COL32(200, 200, 255, 100), 0.0f, 0, 2.0f);
            }
            
            if (ImGui::IsItemClicked()) {
                isConnected = !isConnected;
            }
        }
        
        // Label
        ImVec2 labelPos = ImVec2(pos.x - 50, pos.y + connectorHeight * 0.5f - 7);
        draw->AddText(labelPos, IM_COL32(150, 150, 180, 255), label);
    }
};

class NerveSystemLayer : public Walnut::Layer
{
public:
    virtual void OnAttach() override;
    virtual void OnDetach() override;
    virtual void OnUIRender() override;

private:
    void CheckPortStatus();
    void RenderConnector(EvaSignalConnector& connector, ImVec2& pos, bool asButton);

    int m_PortNumber = -1;
    
    EvaSignalConnector m_CtsSignal{"CTS", 132};
    EvaSignalConnector m_DsrSignal{"DSR", 133};
    EvaSignalConnector m_DcdSignal{"DCD", 134};
    EvaSignalConnector m_RingSignal{"RING", 135};
    EvaSignalConnector m_DtrControl{"DTR", 201};
    EvaSignalConnector m_RtsControl{"RTS", 202};
};