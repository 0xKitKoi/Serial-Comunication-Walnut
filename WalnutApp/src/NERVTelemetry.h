#pragma once

#include <imgui.h>

#include <deque>
#include <string>
#include <vector>
#include <cstdlib>
#include <cmath>
#include <imgui_internal.h>

class EvaBinaryPanel
{
public:

    // =====================================================
    // CONFIG
    // =====================================================

    float Width = 320.0f;
    float Height = 320.0f;

    std::string Header = "DATA STREAM";
    std::string Subtitle = "<CENTRAL DOGMA>";
    std::string SideLabel = "BINARY   STREAM";

    int MaxLines = 12;

    // =====================================================
    // PUBLIC API
    // =====================================================

        void PushString(const std::string& text)
        {
            std::string binary;

            for (unsigned char c : text)
            {
                for (int i = 7; i >= 0; i--)
                {
                    binary += ((c >> i) & 1) ? '1' : '0';
                }

                binary += ' ';
            }

            // =================================================
            // WRAP INTO MULTIPLE LINES
            // =================================================

            const int charsPerLine = 26;

            for (size_t i = 0; i < binary.size(); i += charsPerLine)
            {
                m_BinaryLines.push_back(
                    binary.substr(i, charsPerLine)
                );
            }

            while ((int)m_BinaryLines.size() > MaxLines)
            {
                m_BinaryLines.pop_front();
            }
        }

    // =====================================================
    // MAIN RENDER
    // =====================================================

    void Render(ImVec2 pos)
    {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        float startY = pos.y + 120.0f;

        // =================================================
        // COLORS
        // =================================================

        ImU32 red        = IM_COL32(255, 80, 80, 255);
        ImU32 brightRed  = IM_COL32(255, 120, 120, 255);
        ImU32 dark       = IM_COL32(10, 0, 0, 240);
        ImU32 black      = IM_COL32(0, 0, 0, 255);

        // =================================================
        // MAIN PANEL
        // =================================================

        ImVec2 p0 = pos;
        ImVec2 p1 = { pos.x + Width, pos.y + Height };

        draw->AddRectFilled(p0, p1, dark);
        draw->AddRect(p0, p1, red, 0.0f, 0, 2.0f);

        // =================================================
        // LEFT EVA SHAPE
        // =================================================

        float leftWidth = 46.0f;

        ImVec2 shape[6] =
        {
            { pos.x - leftWidth,      pos.y + 18 },
            { pos.x - 14,             pos.y + 18 },
            { pos.x - 2,              pos.y + 4  },
            { pos.x - 2,              pos.y + Height - 4 },
            { pos.x - 14,             pos.y + Height - 18 },
            { pos.x - leftWidth,      pos.y + Height - 18 }
        };

        draw->AddConvexPolyFilled(shape, 6, IM_COL32(140, 40, 40, 255));
        draw->AddPolyline(shape, 6, red, true, 2.0f);

        // =================================================
        // SIDE LABEL (vertical)
        // =================================================
            std::string side = SideLabel;

            float x = pos.x - 34.0f;
            //float y = pos.y + Height - 30.0f;
            float y = pos.y  - 30.0f;

            // for (int i = (int)side.size() - 1; i >= 0; i--)
            // {
            //     char s[2] = { side[i], 0 };

            //     ImVec2 textPos(x, y);

            //     draw->AddText(
            //         nullptr,
            //         16.0f,
            //         textPos,
            //         brightRed,
            //         s
            //     );

            //     x += 8.0f;
            // }
            float angle = -IM_PI * 0.5f; // 90° clockwise

                ImVec2 origin(pos.x - 34.0f, pos.y + 30.0f);

                float cosA = cosf(angle);
                float sinA = sinf(angle);

                // direction of "text flow" after rotation
                ImVec2 dir(cosA, sinA);

                // force text to always grow "upwards along the rotated axis"
                ImVec2 flow = dir;

                // perpendicular for glyph spacing
                ImVec2 step(-sinA, cosA);

                float size = 16.0f;

                for (int i = 0; i < (int)side.size(); i++)
                {
                    char s[2] = { side[i], 0 };

                    ImVec2 flow = ImVec2(-dir.x, -dir.y);
                    ImVec2 glyphPos;
                    glyphPos.x = origin.x + flow.x * (i * size);
                    glyphPos.y = origin.y + flow.y * (i * size);

                    draw->AddText(
                        nullptr,
                        size,
                        glyphPos,
                        brightRed,
                        s
                    );
                }




        // =================================================
        // DECORATIVE BAR
        // =================================================

        draw->AddLine(
            { pos.x - 36, pos.y + Height * 0.5f },
            { pos.x - 16, pos.y + Height * 0.5f },
            black,
            4.0f
        );

        // =================================================
        // ICON BOX
        // =================================================

        ImVec2 iconPos = { pos.x + 18, pos.y + 18 };
        ImVec2 iconEnd = { iconPos.x + 38, iconPos.y + 38 };

        draw->AddRect(iconPos, iconEnd, brightRed, 0.0f, 0, 2.0f);

        // upload/download arrows

        draw->AddLine(
            { iconPos.x + 10, iconPos.y + 12 },
            { iconPos.x + 10, iconPos.y + 26 },
            brightRed,
            2.0f
        );

        draw->AddTriangleFilled(
            { iconPos.x + 10, iconPos.y + 8 },
            { iconPos.x + 6,  iconPos.y + 14 },
            { iconPos.x + 14, iconPos.y + 14 },
            brightRed
        );

        draw->AddLine(
            { iconPos.x + 26, iconPos.y + 26 },
            { iconPos.x + 26, iconPos.y + 12 },
            brightRed,
            2.0f
        );

        draw->AddTriangleFilled(
            { iconPos.x + 26, iconPos.y + 30 },
            { iconPos.x + 22, iconPos.y + 24 },
            { iconPos.x + 30, iconPos.y + 24 },
            brightRed
        );

        // =================================================
        // HEADER TEXT
        // =================================================

        draw->AddText(
            nullptr,
            28.0f,
            { pos.x + 70, pos.y + 18 },
            brightRed,
            Header.c_str()
        );

        draw->AddText(
            nullptr,
            16.0f,
            { pos.x + 74, pos.y + 50 },
            brightRed,
            Subtitle.c_str()
        );

        // =================================================
        // DIVIDER
        // =================================================

        draw->AddRectFilled(
            { pos.x + 18, pos.y + 86 },
            { pos.x + Width - 18, pos.y + 98 },
            brightRed
        );

        // =================================================
        // BINARY STREAM
        // =================================================

        startY = pos.y + 120.0f;

        for (size_t i = 0; i < m_BinaryLines.size(); i++)
        {
            std::string line = m_BinaryLines[i];

            // optional corruption flicker
            if ((rand() % 100) < 4)
            {
                if (!line.empty())
                {
                    int idx = rand() % line.size();

                    if (line[idx] != ' ')
                        line[idx] = '#';
                }
            }

            ImVec2 textPos(
                pos.x + 18,
                startY + (float)i * 14.0f
            );

            draw->AddText(
                nullptr,
                16.0f,
                textPos,
                red,
                line.c_str()
            );
        }

        // =================================================
        // RIGHT EVA CUTOUT
        // =================================================

        ImVec2 notch[4] =
        {
            { p1.x,      pos.y + Height * 0.40f },
            { p1.x + 8,  pos.y + Height * 0.44f },
            { p1.x + 8,  pos.y + Height * 0.56f },
            { p1.x,      pos.y + Height * 0.60f }
        };

        draw->AddConvexPolyFilled(notch, 4, IM_COL32(120,40,40,255));
    }

private:

    std::deque<std::string> m_BinaryLines;
};