#include "NerveSystemLayer.h"
#include "rs232.h" // Your serial library header

// Call this right after ImGui::Begin("My Window") each frame
static void DrawEVAGlowBorder(float rounding = 4.0f)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos();
    ImVec2 sz = ImGui::GetWindowSize();
    ImVec2 pmax = ImVec2(p.x + sz.x, p.y + sz.y);

    // Layer outward: each rect is 1px larger, color shifts dark-red → gold
    // Format: AddRect(min, max, color, rounding, flags, thickness)

    // Outermost — deep red, ghostly
    dl->AddRect(
        ImVec2(p.x - 3, p.y - 3), ImVec2(pmax.x + 3, pmax.y + 3),
        IM_COL32(149, 43, 32, 80),   // #952B20, low alpha
        rounding + 3, 0, 1.0f);

    // Mid-outer — burnt orange
    dl->AddRect(
        ImVec2(p.x - 2, p.y - 2), ImVec2(pmax.x + 2, pmax.y + 2),
        IM_COL32(199, 84, 51, 160),  // #C75433
        rounding + 2, 0, 1.0f);

    // Inner glow — amber
    dl->AddRect(
        ImVec2(p.x - 1, p.y - 1), ImVec2(pmax.x + 1, pmax.y + 1),
        IM_COL32(244, 144, 39, 220), // #F49027
        rounding + 1, 0, 1.5f);

    // Innermost — hot gold, full brightness, thicker
    dl->AddRect(
        p, pmax,
        IM_COL32(247, 183, 32, 255), // #F7B720
        rounding, 0, 2.0f);
}

void NerveSystemLayer::OnAttach()
{
    // Open your serial port here
    const char* portName = "COM3"; // Change to your port
    m_PortNumber = RS232_GetPortnr(portName);
    
    if (RS232_OpenComport(m_PortNumber, 9600, "8N1", 0) == 0) {
        // Port opened successfully
        RS232_enableDTR(m_PortNumber);
        RS232_enableRTS(m_PortNumber);
        m_DtrControl.isConnected = true;
        m_RtsControl.isConnected = true;
    }
}

void NerveSystemLayer::OnDetach()
{
    if (m_PortNumber >= 0) {
        RS232_CloseComport(m_PortNumber);
    }
}

void NerveSystemLayer::CheckPortStatus()
{
    if (m_PortNumber < 0) {
        m_CtsSignal.isConnected = false;
        m_DsrSignal.isConnected = false;
        m_DcdSignal.isConnected = false;
        m_RingSignal.isConnected = false;
        return;
    }
    
    m_CtsSignal.isConnected = RS232_IsCTSEnabled(m_PortNumber) != 0;
    m_DsrSignal.isConnected = RS232_IsDSREnabled(m_PortNumber) != 0;
    m_DcdSignal.isConnected = RS232_IsDCDEnabled(m_PortNumber) != 0;
    m_RingSignal.isConnected = RS232_IsRINGEnabled(m_PortNumber) != 0;
}

void NerveSystemLayer::OnUIRender()
{
    ImGui::Begin("NERV - Serial Control System");
    DrawEVAGlowBorder();
    // Update animations
    float dt = ImGui::GetIO().DeltaTime;
    m_CtsSignal.Update(dt);
    m_DsrSignal.Update(dt);
    m_DcdSignal.Update(dt);
    m_RingSignal.Update(dt);
    m_DtrControl.Update(dt);
    m_RtsControl.Update(dt);
    
    // Check port status
    CheckPortStatus();
    
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "NERVE CONNECTION STATUS");
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Text("INPUT SIGNALS (Device -> Computer)");
    ImGui::Spacing();
    
    ImVec2 startPos = ImGui::GetCursorScreenPos();
    startPos.x += 60;
    
    m_CtsSignal.Render(ImVec2(startPos.x, startPos.y), false);
    ImGui::Dummy(ImVec2(0, 30));
    
    m_DsrSignal.Render(ImVec2(startPos.x, startPos.y + 35), false);
    ImGui::Dummy(ImVec2(0, 30));
    
    m_DcdSignal.Render(ImVec2(startPos.x, startPos.y + 70), false);
    ImGui::Dummy(ImVec2(0, 30));
    
    m_RingSignal.Render(ImVec2(startPos.x, startPos.y + 105), false);
    ImGui::Dummy(ImVec2(0, 30));
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("OUTPUT CONTROLS (Computer -> Device)");
    ImGui::Spacing();
    
    ImVec2 controlPos = ImGui::GetCursorScreenPos();
    controlPos.x += 60;
    
    m_DtrControl.Render(ImVec2(controlPos.x, controlPos.y), true);
    if (ImGui::IsItemClicked() && m_PortNumber >= 0) {
        if (m_DtrControl.isConnected) {
            RS232_enableDTR(m_PortNumber);
        } else {
            RS232_disableDTR(m_PortNumber);
        }
    }
    ImGui::Dummy(ImVec2(0, 30));
    
    m_RtsControl.Render(ImVec2(controlPos.x, controlPos.y + 35), true);
    if (ImGui::IsItemClicked() && m_PortNumber >= 0) {
        if (m_RtsControl.isConnected) {
            RS232_enableRTS(m_PortNumber);
        } else {
            RS232_disableRTS(m_PortNumber);
        }
    }
    ImGui::Dummy(ImVec2(0, 30));
    
    ImGui::Separator();
    
    bool allConnected = m_CtsSignal.isConnected && m_DsrSignal.isConnected;
    if (allConnected) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.4f, 1.0f), 
                          "ALL NERVE CONNECTIONS INTACT");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), 
                          "NERVE CONNECTIONS SEVERED");
    }
    
    ImGui::End();
}