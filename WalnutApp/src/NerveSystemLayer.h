#pragma once
#include "Walnut/Layer.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>


	// ── Helper: draws a glowing border outline only ───────────────
	static void DrawGlowBorder(ImDrawList* dl, ImVec2 rMin, ImVec2 rMax,
		ImU32 hotColor, ImU32 outerColor,
		float rounding, int glowLayers = 6, float glowSpread = 1.2f)
	{
		for (int i = glowLayers; i >= 1; --i)
		{
			float expand = (float)i * glowSpread;
			int alpha = (int)(180.0f * ((float)(glowLayers - i + 1) / (float)glowLayers)
				* ((float)(glowLayers - i + 1) / (float)glowLayers));
			ImU32 col = (outerColor & 0x00FFFFFF) | ((ImU32)alpha << 24);
			dl->AddRect(
				ImVec2(rMin.x - expand, rMin.y - expand),
				ImVec2(rMax.x + expand, rMax.y + expand),
				col, rounding + expand, 0, 1.0f);
		}
		// Hot inner border
		dl->AddRect(rMin, rMax, hotColor, rounding, 0, 1.5f);
	}


// ── Helper: draws a glowing filled rect with bloom ────────────
	static void DrawGlowRect(ImDrawList* dl, ImVec2 rMin, ImVec2 rMax,
		ImU32 coreColor, ImU32 midColor, ImU32 outerColor,
		float rounding, int glowLayers = 8, float glowSpread = 1.2f)
	{
		// Outer bloom — many transparent layers expanding outward
		for (int i = glowLayers; i >= 1; --i)
		{
			float expand = (float)i * glowSpread;
			// Alpha falls off with distance — further = more transparent
			int alpha = (int)(120.0f * ((float)(glowLayers - i + 1) / (float)glowLayers)
				* ((float)(glowLayers - i + 1) / (float)glowLayers)); // quadratic falloff

			// Extract RGB from outerColor and apply our own alpha
			ImU32 col = (outerColor & 0x00FFFFFF) | ((ImU32)alpha << 24);

			dl->AddRectFilled(
				ImVec2(rMin.x - expand, rMin.y - expand),
				ImVec2(rMax.x + expand, rMax.y + expand),
				col, rounding + expand);
		}

		// Core fill — layered from edge inward getting hotter
		dl->AddRectFilled(rMin, rMax, outerColor, rounding);

		dl->AddRectFilled(
			ImVec2(rMin.x + 1, rMin.y + 1),
			ImVec2(rMax.x - 1, rMax.y - 1),
			midColor, rounding - 0.5f);

		dl->AddRectFilled(
			ImVec2(rMin.x + 2, rMin.y + 2),
			ImVec2(rMax.x - 2, rMax.y - 2),
			coreColor, rounding - 1.0f);
	}

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


    static void DrawRotatedRect(ImDrawList* dl, ImVec2 center, float w, float h,
                             float angleDeg, ImU32 fillColor, ImU32 borderColor,
                             float borderThickness = 1.5f)
{
    float rad = angleDeg * 3.14159f / 180.0f;
    float cosA = cosf(rad), sinA = sinf(rad);

    auto rot = [&](float lx, float ly) -> ImVec2 {
        return ImVec2(center.x + lx * cosA - ly * sinA,
                      center.y + lx * sinA + ly * cosA);
    };

    float hw = w * 0.5f, hh = h * 0.5f;
    ImVec2 p0 = rot(-hw, -hh);
    ImVec2 p1 = rot( hw, -hh);
    ImVec2 p2 = rot( hw,  hh);
    ImVec2 p3 = rot(-hw,  hh);

    dl->AddQuadFilled(p0, p1, p2, p3, fillColor);
    dl->AddQuad(p0, p1, p2, p3, borderColor, borderThickness);
}

static void DrawRotatedRectGlow(ImDrawList* dl, ImVec2 center, float w, float h,
                                float angleDeg, ImU32 fillColor,
                                ImU32 glowHot, ImU32 glowBloom)
{
    float rad = angleDeg * 3.14159f / 180.0f;
    float cosA = cosf(rad), sinA = sinf(rad);

    auto rot = [&](float lx, float ly) -> ImVec2 {
        return ImVec2(center.x + lx * cosA - ly * sinA,
                      center.y + lx * sinA + ly * cosA);
    };

    // Glow layers — expand outward
    for (int g = 4; g >= 1; --g)
    {
        float expand = (float)g * 1.0f;
        float hw = (w + expand * 2) * 0.5f;
        float hh = (h + expand * 2) * 0.5f;
        int alpha = (int)(160.0f * (float)(4 - g + 1) / 4.0f
                        * (float)(4 - g + 1) / 4.0f);
        ImU32 col = (glowBloom & 0x00FFFFFF) | ((ImU32)alpha << 24);
        ImVec2 p0 = rot(-hw,-hh), p1 = rot(hw,-hh);
        ImVec2 p2 = rot( hw, hh), p3 = rot(-hw, hh);
        dl->AddQuad(p0, p1, p2, p3, col, 1.0f);
    }

    // Fill
    {
        float hw = w * 0.5f, hh = h * 0.5f;
        ImVec2 p0 = rot(-hw,-hh), p1 = rot(hw,-hh);
        ImVec2 p2 = rot( hw, hh), p3 = rot(-hw, hh);
        dl->AddQuadFilled(p0, p1, p2, p3, fillColor);
        dl->AddQuad(p0, p1, p2, p3, glowHot, 1.5f);
    }
}

// static void DrawSignalRow(ImDrawList* dl, const char* label, const char* idText,
//                           bool connected, bool isOutput,
//                           ImVec2 rowPos, float windowWidth, int rowIndex, int totalRows)
// {
//     ImU32 glowHot    = IM_COL32(247, 183,  32, 255);
//     ImU32 glowBloom  = IM_COL32(149,  43,  32, 255);
//     ImU32 redHot     = IM_COL32(200,  30,  10, 255);
//     ImU32 redBloom   = IM_COL32(100,  10,   5, 255);
//     ImU32 labelColor = IM_COL32( 80, 255, 120, 255);
//     ImU32 idColor    = IM_COL32( 80, 255, 120, 255);

//     const float rowH   = 30.0f;
//     const float connW  = 24.0f;
//     const float connH  = 9.0f;
//     const float margin = 10.0f;
//     const float labelW = 48.0f;
//     const float angle  = -45.0f;
//     const float diagLen = 14.0f; // horizontal extent of each 45° bend (= vertical drop too)

//     float centerY   = rowPos.y + rowH * 0.5f;
//     float wireLeft  = rowPos.x + margin + labelW;
//     float wireRight = rowPos.x + windowWidth - margin;

//     // ── Label ─────────────────────────────────────────────────
//     dl->AddText(
//         ImVec2(rowPos.x + margin,
//                rowPos.y + (rowH - ImGui::GetTextLineHeight()) * 0.5f),
//         labelColor, label);

//     // ── PCB trace geometry ────────────────────────────────────
//     // The trace does: H → diag → H → diag → H
//     // Each diagonal drops diagLen pixels vertically
//     // To keep it on one row height, we center it vertically
//     // so it drops by diagLen/2 then rises by diagLen/2

//     // Total wire span
//     float totalW = wireRight - wireLeft;

//     // Horizontal run lengths
//     float hLeft  = totalW * 0.12f;  // short left run
//     float hRight = totalW * 0.12f;  // short right run
//     float hMid   = totalW - hLeft - hRight - diagLen * 2.0f; // long center run

//     // Y positions — trace dips down then comes back up
//     float yTop    = centerY - diagLen * 0.5f;
//     float yBottom = centerY + diagLen * 0.5f;

//     // Trace key points:
//     // A ──── B                        G ──── H     (yTop)
//     //         \                      /
//     //          C ─────────────────── F             (yBottom)
//     //               D(conn) E(id)

//     ImVec2 ptA = ImVec2(wireLeft,                    yTop);
//     ImVec2 ptB = ImVec2(wireLeft  + hLeft,           yTop);
//     ImVec2 ptC = ImVec2(wireLeft  + hLeft + diagLen, yBottom);
//     ImVec2 ptF = ImVec2(wireRight - hRight - diagLen,yBottom);
//     ImVec2 ptG = ImVec2(wireRight - hRight,          yTop);
//     ImVec2 ptH = ImVec2(wireRight,                   yTop);

//     // Left connector on the diagonal (between B and C)
//     float lcx = (ptB.x + ptC.x) * 0.5f;
//     float lcy = (ptB.y + ptC.y) * 0.5f;

//     // Right connector on the diagonal (between F and G)
//     float rcx = (ptF.x + ptG.x) * 0.5f;
//     float rcy = (ptF.y + ptG.y) * 0.5f;

//     // ID label in center of horizontal mid run
//     float idCX = (ptC.x + ptF.x) * 0.5f;
//     float idCY = yBottom;

//     // Two small blocks flanking the ID label on the mid run
//     float block1X = ptC.x + (ptF.x - ptC.x) * 0.25f;
//     float block2X = ptC.x + (ptF.x - ptC.x) * 0.75f;

//     // Disconnected gap
//     bool leftDisconnected  = isOutput  && !connected;
//     bool rightDisconnected = !isOutput && !connected;
//     float gapSize = 12.0f;

//     ImU32 leftHot    = leftDisconnected  ? redHot   : glowHot;
//     ImU32 leftBloom  = leftDisconnected  ? redBloom : glowBloom;
//     ImU32 rightHot   = rightDisconnected ? redHot   : glowHot;
//     ImU32 rightBloom = rightDisconnected ? redBloom : glowBloom;

//     auto drawWire = [&](ImVec2 from, ImVec2 to, ImU32 hot, ImU32 bloom)
//     {
//         dl->AddLine(from, to, (bloom & 0x00FFFFFF) | (40u  << 24), 4.0f);
//         dl->AddLine(from, to, (bloom & 0x00FFFFFF) | (120u << 24), 2.5f);
//         dl->AddLine(from, to, (hot   & 0x00FFFFFF) | (220u << 24), 1.2f);
//     };

//     // ── Draw trace ────────────────────────────────────────────
//     // Left horizontal run (with gap if output disconnected)
//     if (leftDisconnected)
//     {
//         drawWire(ptA, ImVec2(ptB.x - gapSize, ptB.y), redHot, redBloom);
//         // gap here — no wire drawn to connector
//     }
//     else
//     {
//         drawWire(ptA, ptB, glowHot, glowBloom);
//         // Left diagonal
//         drawWire(ptB, ptC, glowHot, glowBloom);
//     }

//     // Middle horizontal run
//     drawWire(ptC, ptF, glowHot, glowBloom);

//     // Right diagonal + right run
//     if (rightDisconnected)
//     {
//         drawWire(ptF, ptG, glowHot, glowBloom);
//         // gap — no wire from G to H
//         drawWire(ImVec2(ptH.x - (wireRight - ptG.x) + gapSize, ptH.y),
//                  ptH, redHot, redBloom);
//     }
//     else
//     {
//         drawWire(ptF, ptG, glowHot, glowBloom);
//         drawWire(ptG, ptH, glowHot, glowBloom);
//     }

//     // ── Small blocks on mid horizontal run ────────────────────
//     // Just 2, flanking the ID label
//     float blockW = 13.0f, blockH = 6.0f;
//     DrawRotatedRectGlow(dl, ImVec2(block1X, idCY),
//         blockW, blockH, 0.0f,   // 0° — flat, not rotated, like reference
//         IM_COL32(10,10,10,255), glowHot, glowBloom);
//     DrawRotatedRectGlow(dl, ImVec2(block2X, idCY),
//         blockW, blockH, 0.0f,
//         IM_COL32(10,10,10,255), glowHot, glowBloom);

//     // ── Left connector on diagonal ────────────────────────────
//     DrawRotatedRectGlow(dl, ImVec2(lcx, lcy),
//         connW, connH, angle,
//         IM_COL32(15,15,15,255), leftHot, leftBloom);

//     // ── Right connector on diagonal ───────────────────────────
//     DrawRotatedRectGlow(dl, ImVec2(rcx, rcy),
//         connW, connH, angle,
//         IM_COL32(15,15,15,255), rightHot, rightBloom);

//     // ── ID label ──────────────────────────────────────────────
//     DrawRotatedRectGlow(dl, ImVec2(idCX, idCY),
//         44.0f, 13.0f, 0.0f,    // flat label box like reference
//         IM_COL32(10,10,10,255), glowHot, glowBloom);

//     ImVec2 idSize = ImGui::CalcTextSize(idText);
//     dl->AddText(
//         ImVec2(idCX - idSize.x * 0.5f, idCY - idSize.y * 0.5f),
//         idColor, idText);

//     ImGui::Dummy(ImVec2(windowWidth, rowH + 2.0f));
// }



static void DrawSignalRow(ImDrawList* dl, const char* label, const char* idText,
                          bool connected, bool isOutput,
                          ImVec2 rowPos, float windowWidth, int rowIndex, int totalRows)
{
    ImU32 glowHot    = IM_COL32(247, 183,  32, 255);
    ImU32 glowBloom  = IM_COL32(149,  43,  32, 255);
    ImU32 redHot     = IM_COL32(200,  30,  10, 255);
    ImU32 redBloom   = IM_COL32(100,  10,   5, 255);
    ImU32 labelColor = IM_COL32( 80, 255, 120, 255);
    ImU32 idColor    = IM_COL32( 80, 255, 120, 255);
    ImU32 fillDark   = IM_COL32( 12,  12,  12, 255);

    const float rowH     = 32.0f;
    const float margin   = 10.0f;
    const float labelW   = 52.0f;

    // Connector dimensions — wide flat rect like your sketch
    const float connW    = 30.0f;
    const float connH    = 14.0f;
    const float nubW     =  7.0f;
    const float nubH     =  6.0f;
    const float round    =  2.0f;

    // The trace spans the full window width
    float xLeft  = rowPos.x + margin + labelW;
    float xRight = rowPos.x + windowWidth - margin;

    // ── Staircase Y positions ─────────────────────────────────
    // Each row starts at a different Y so diagonals stack like stairs
    // Row 0 starts at top of its row, last row ends at bottom
    float yStart = rowPos.y + rowH * 0.2f;  // wire enters high
    float yEnd   = rowPos.y + rowH * 0.8f;  // wire exits low

    // The trace shape (based on your sketch):
    // xLeft,yStart ──── diag down ──── xMid,yMid (connector zone) ──── diag down ──── xRight,yEnd
    // But per your sketch it's more: short horiz → steep diag → long horiz → short diag → short horiz

    float totalSpan = xRight - xLeft;
    float d1W  = totalSpan * 0.08f;   // first short horiz
    float diag1W = (yEnd - yStart);   // diagonal width = height drop (45°)
    float midW = totalSpan * 0.55f;   // long center horizontal
    float diag2W = (yEnd - yStart) * 0.5f;
    float d2W  = totalSpan - d1W - diag1W - midW - diag2W; // remaining right horiz

    // Clamp
    if (d2W < 10.0f) { midW -= (10.0f - d2W); d2W = 10.0f; }

    // Key X positions
    float x0 = xLeft;                          // wire enters
    float x1 = x0 + d1W;                       // start of first diagonal
    float x2 = x1 + diag1W;                    // end of first diagonal (connector zone starts)
    float x3 = x2 + midW;                      // end of center run (start of second diag)
    float x4 = x3 + diag2W;                    // end of second diagonal
    float x5 = xRight;                         // wire exits

    // Y at each key X
    float y0 = yStart;
    float y1 = yStart;
    float y2 = yEnd;
    float y3 = yEnd;
    float y4 = yEnd;
    float y5 = yEnd;

    // Left connector sits on the first diagonal, centered
    float lcx = (x1 + x2) * 0.5f;
    float lcy = (y1 + y2) * 0.5f;

    // Right connector sits just before x4 on the flat run
    float rcx = x3 + (x4 - x3) * 0.5f;
    float rcy = y3;

    // ID label in center of the long horizontal run
    float idCX = (x2 + x3) * 0.5f;
    float idCY = y2;

    // Small blocks on the center run
    float blk1X = x2 + (x3 - x2) * 0.28f;
    float blk2X = x2 + (x3 - x2) * 0.72f;

    // ── Connection state ──────────────────────────────────────
    bool leftDisc  = isOutput  && !connected;
    bool rightDisc = !isOutput && !connected;
    float gapSize  = 13.0f;

    ImU32 leftHot    = leftDisc  ? redHot   : glowHot;
    ImU32 leftBloom  = leftDisc  ? redBloom : glowBloom;
    ImU32 rightHot   = rightDisc ? redHot   : glowHot;
    ImU32 rightBloom = rightDisc ? redBloom : glowBloom;

    auto drawWire = [&](ImVec2 from, ImVec2 to, ImU32 hot, ImU32 bloom)
    {
        if (from.x >= to.x && from.y >= to.y && from.x == to.x) return;
        dl->AddLine(from, to, (bloom & 0x00FFFFFF) | (45u  << 24), 4.5f);
        dl->AddLine(from, to, (bloom & 0x00FFFFFF) | (130u << 24), 2.8f);
        dl->AddLine(from, to, (hot   & 0x00FFFFFF) | (230u << 24), 1.3f);
    };

    // ── Trace segments ────────────────────────────────────────
    // Segment 1: left edge to start of diagonal
    if (leftDisc)
        drawWire(ImVec2(x0, y0), ImVec2(x1 - gapSize, y1), redHot, redBloom);
    else
        drawWire(ImVec2(x0, y0), ImVec2(x1, y1), glowHot, glowBloom);

    // Segment 2: diagonal (skip if left disconnected — gap shows here)
    if (!leftDisc)
        drawWire(ImVec2(x1, y1), ImVec2(x2, y2), glowHot, glowBloom);

    // Segment 3: long center horizontal
    drawWire(ImVec2(x2, y2), ImVec2(x3, y3), glowHot, glowBloom);

    // Segment 4: second short diagonal
    if (!rightDisc)
        drawWire(ImVec2(x3, y3), ImVec2(x4, y4), glowHot, glowBloom);

    // Segment 5: right exit
    if (rightDisc)
        drawWire(ImVec2(x4 + gapSize, y5), ImVec2(x5, y5), redHot, redBloom);
    else
        drawWire(ImVec2(x4, y4), ImVec2(x5, y5), glowHot, glowBloom);

    // ── Label ─────────────────────────────────────────────────
    dl->AddText(
        ImVec2(rowPos.x + margin,
               rowPos.y + (rowH - ImGui::GetTextLineHeight()) * 0.5f),
        labelColor, label);

    // ── Left connector — flat rect with nub on right ──────────
    {
        float cx    = lcx, cy = lcy;
        ImVec2 bMin = ImVec2(cx - connW * 0.5f, cy - connH * 0.5f);
        ImVec2 bMax = ImVec2(cx + connW * 0.5f, cy + connH * 0.5f);
        dl->AddRectFilled(bMin, bMax, fillDark, round);
        DrawGlowBorder(dl, bMin, bMax, leftHot, leftBloom, round, 4, 0.8f);

        // Nub on right side
        ImVec2 nMin = ImVec2(bMax.x,          cy - nubH * 0.5f);
        ImVec2 nMax = ImVec2(bMax.x + nubW,   cy + nubH * 0.5f);
        dl->AddRectFilled(nMin, nMax, fillDark, 1.0f);
        DrawGlowBorder(dl, nMin, nMax, leftHot, leftBloom, 1.0f, 3, 0.6f);
    }

    // ── Right connector — flat rect with nub on left ──────────
    {
        float cx = rcx, cy = rcy;
        ImVec2 bMin = ImVec2(cx - connW * 0.5f, cy - connH * 0.5f);
        ImVec2 bMax = ImVec2(cx + connW * 0.5f, cy + connH * 0.5f);
        dl->AddRectFilled(bMin, bMax, fillDark, round);
        DrawGlowBorder(dl, bMin, bMax, rightHot, rightBloom, round, 4, 0.8f);

        // Nub on left side
        ImVec2 nMin = ImVec2(bMin.x - nubW, cy - nubH * 0.5f);
        ImVec2 nMax = ImVec2(bMin.x,        cy + nubH * 0.5f);
        dl->AddRectFilled(nMin, nMax, fillDark, 1.0f);
        DrawGlowBorder(dl, nMin, nMax, rightHot, rightBloom, 1.0f, 3, 0.6f);
    }

    // ── Small blocks on center run ────────────────────────────
    for (float bx : { blk1X, blk2X })
    {
        ImVec2 bMin = ImVec2(bx - 8.0f, idCY - 4.0f);
        ImVec2 bMax = ImVec2(bx + 8.0f, idCY + 4.0f);
        dl->AddRectFilled(bMin, bMax, fillDark, 1.0f);
        DrawGlowBorder(dl, bMin, bMax, glowHot, glowBloom, 1.0f, 3, 0.7f);
    }

    // ── ID label box ──────────────────────────────────────────
    {
        ImVec2 bMin = ImVec2(idCX - 22.0f, idCY - 7.0f);
        ImVec2 bMax = ImVec2(idCX + 22.0f, idCY + 7.0f);
        dl->AddRectFilled(bMin, bMax, fillDark, round);
        DrawGlowBorder(dl, bMin, bMax, glowHot, glowBloom, round, 4, 0.8f);

        ImVec2 idSize = ImGui::CalcTextSize(idText);
        dl->AddText(
            ImVec2(idCX - idSize.x * 0.5f, idCY - idSize.y * 0.5f),
            idColor, idText);
    }

    ImGui::Dummy(ImVec2(windowWidth, rowH + 2.0f));
}



    // ── Connector shapes ──────────────────────────────────────────
// Male: rect with a tab protruding from the right
static void DrawMaleConnector(ImDrawList* dl, ImVec2 pos, float w, float h, ImU32 fillColor, ImU32 glowHot, ImU32 glowBloom)
{
    float tabW = h * 0.3f;
    float tabH = h * 0.5f;
    float tabY = pos.y + (h - tabH) * 0.5f;

    // Main body
    ImVec2 bodyMin = pos;
    ImVec2 bodyMax = ImVec2(pos.x + w, pos.y + h);

    // Tab protruding right
    ImVec2 tabMin = ImVec2(bodyMax.x, tabY);
    ImVec2 tabMax = ImVec2(bodyMax.x + tabW, tabY + tabH);

    // Glow fill
    DrawGlowRect(dl, bodyMin, bodyMax, fillColor, fillColor, 
        IM_COL32(80, 20, 5, 255), 2.0f, 6, 1.0f);
    dl->AddRectFilled(tabMin, tabMax, fillColor, 1.0f);

    // Border glow
    // Draw as one outline using polyline so tab is included
    ImVec2 pts[] = {
        ImVec2(bodyMin.x, bodyMin.y),
        ImVec2(bodyMax.x, bodyMin.y),
        ImVec2(bodyMax.x, tabMin.y),
        ImVec2(tabMax.x,  tabMin.y),
        ImVec2(tabMax.x,  tabMax.y),
        ImVec2(bodyMax.x, tabMax.y),
        ImVec2(bodyMax.x, bodyMax.y),
        ImVec2(bodyMin.x, bodyMax.y),
        ImVec2(bodyMin.x, bodyMin.y),
    };
    // Glow layers
    for (int g = 3; g >= 0; --g)
    {
        float expand = (float)g * 1.0f;
        int   alpha  = (g == 0) ? 255 : (g == 1) ? 200 : (g == 2) ? 140 : 70;
        ImU32 col    = (g == 0) ? glowHot : (g == 1) ? glowHot 
                     : (g == 2) ? glowBloom : glowBloom;
        col = (col & 0x00FFFFFF) | ((ImU32)alpha << 24);
        // Offset all points outward by expand (approximate)
        dl->AddPolyline(pts, 9, col, ImDrawFlags_None, (g == 0) ? 1.5f : 1.0f);
    }
}

// Female: rect with an indent on the left side
static void DrawFemaleConnector(ImDrawList* dl, ImVec2 pos, float w, float h, ImU32 fillColor, ImU32 glowHot, ImU32 glowBloom)
{
    float tabW = h * 0.3f;
    float tabH = h * 0.5f;
    float tabY = pos.y + (h - tabH) * 0.5f;

    // Main body starts after the indent
    ImVec2 bodyMin = ImVec2(pos.x + tabW, pos.y);
    ImVec2 bodyMax = ImVec2(pos.x + w,    pos.y + h);

    // Fill body
    DrawGlowRect(dl, bodyMin, bodyMax, fillColor, fillColor,
        IM_COL32(80, 20, 5, 255), 2.0f, 6, 1.0f);

    // Fill the top and bottom parts of the indent wall
    dl->AddRectFilled(ImVec2(pos.x, pos.y),        ImVec2(pos.x + tabW, tabY),         fillColor);
    dl->AddRectFilled(ImVec2(pos.x, tabY + tabH),  ImVec2(pos.x + tabW, pos.y + h),    fillColor);

    // Outline
    ImVec2 pts[] = {
        ImVec2(bodyMax.x,       bodyMin.y),
        ImVec2(bodyMin.x,       bodyMin.y),
        ImVec2(bodyMin.x,       tabY),
        ImVec2(pos.x,           tabY),
        ImVec2(pos.x,           tabY + tabH),
        ImVec2(bodyMin.x,       tabY + tabH),
        ImVec2(bodyMin.x,       bodyMax.y),
        ImVec2(bodyMax.x,       bodyMax.y),
        ImVec2(bodyMax.x,       bodyMin.y),
    };
    for (int g = 1; g >= 0; --g)
    {
        int   alpha = (g == 0) ? 255 : 140;
        ImU32 col   = (g == 0) ? glowHot : glowBloom;
        col = (col & 0x00FFFFFF) | ((ImU32)alpha << 24);
        dl->AddPolyline(pts, 9, col, ImDrawFlags_None, (g == 0) ? 1.5f : 1.0f);
    }
}

// ── Angled connecting line ─────────────────────────────────────
static void DrawConnectorLine(ImDrawList* dl, ImVec2 from, ImVec2 to, ImU32 glowHot, ImU32 glowBloom)
{
    // Angled line: goes right a bit, then diagonally, then right to end
    float midX = (from.x + to.x) * 0.5f;
    float diagOffset = (to.y - from.y) * 0.5f;

    ImVec2 p0 = from;
    ImVec2 p1 = ImVec2(midX - diagOffset * 0.5f, from.y);
    ImVec2 p2 = ImVec2(midX + diagOffset * 0.5f, to.y);
    ImVec2 p3 = to;

    // Glow layers
    for (int g = 3; g >= 0; --g)
    {
        float thick = (g == 0) ? 1.5f : 1.0f;
        int   alpha = (g == 0) ? 255 : (g == 1) ? 180 : (g == 2) ? 100 : 50;
        ImU32 col   = (g == 0) ? glowHot : glowBloom;
        col = (col & 0x00FFFFFF) | ((ImU32)alpha << 24);
        dl->AddLine(p0, p1, col, thick + g);
        dl->AddLine(p1, p2, col, thick + g);
        dl->AddLine(p2, p3, col, thick + g);
    }
}

// ── Full signal row ────────────────────────────────────────────
static void DrawSignalRow(ImDrawList* dl, const char* label, bool connected,
                          ImVec2 rowPos, float windowWidth)
{
    ImU32 glowHot   = IM_COL32(247, 183,  32, 255); // #F7B720
    ImU32 glowBloom = IM_COL32(149,  43,  32, 255); // #952B20
    ImU32 fillColor = IM_COL32( 30,  30,  30, 255); // dark fill

    const float labelW    = 50.0f;
    const float connW     = 30.0f;
    const float connH     = 16.0f;
    const float rowH      = connH + 8.0f;
    const float lineGap   = 6.0f;   // gap between connector and line when disconnected

    float rowCenterY = rowPos.y + rowH * 0.5f;
    float connY      = rowPos.y + (rowH - connH) * 0.5f;

    // ── Label ─────────────────────────────────────────────────
    ImVec2 labelPos = ImVec2(rowPos.x, rowPos.y + (rowH - ImGui::GetTextLineHeight()) * 0.5f);
    dl->AddText(labelPos, glowHot, label);

    // ── Male connector (left block) ───────────────────────────
    float maleX = rowPos.x + labelW;
    DrawMaleConnector(dl,
        ImVec2(maleX, connY), connW, connH,
        fillColor, glowHot, glowBloom);

    // Right edge of male tab
    float maleEndX = maleX + connW + connH * 0.3f;

    // ── Female connector (right block, extends to window edge) ─
    float femaleEndX  = rowPos.x + windowWidth - 4.0f; // flush to window edge
    float femaleStartX = femaleEndX - connW;

    DrawFemaleConnector(dl,
        ImVec2(femaleStartX, connY), connW, connH,
        fillColor, glowHot, glowBloom);

    // Left edge of female indent
    float femaleLineX = femaleStartX;

    // ── Connecting line ───────────────────────────────────────
    if (connected)
    {
        // Straight line — locked together
        DrawConnectorLine(dl,
            ImVec2(maleEndX,    rowCenterY),
            ImVec2(femaleLineX, rowCenterY),
            glowHot, glowBloom);
    }
    else
    {
        // Two separate lines going to window edges — disconnected
        // Male side goes left to window edge
        DrawConnectorLine(dl,
            ImVec2(rowPos.x, rowCenterY),
            ImVec2(maleX,    rowCenterY),
            IM_COL32(200, 30, 10, 255),
            IM_COL32(100, 10,  5, 255));

        // Female side goes right to window edge
        DrawConnectorLine(dl,
            ImVec2(femaleEndX,           rowCenterY),
            ImVec2(rowPos.x + windowWidth, rowCenterY),
            IM_COL32(200, 30, 10, 255),
            IM_COL32(100, 10,  5, 255));
    }

    // Advance ImGui cursor
    ImGui::Dummy(ImVec2(windowWidth, rowH + 4.0f));
}

    
    // void Render(ImVec2 pos, bool asButton = false) {
        
    //     ///ImDrawList* draw = ImGui::GetDrawList();
    //     ImDrawList* draw = ImGui::GetWindowDrawList();
        
    //     const float connectorWidth = 40.0f;
    //     const float connectorHeight = 20.0f;
    //     const float nubWidth = 8.0f;
    //     const float wireLength = 80.0f;
    //     const float gap = 20.0f;
        
    //     float separation = gap * (1.0f - animProgress);
    //     float waveAmount = (1.0f - animProgress) * 5.0f;
        
    //     ImU32 connectorColor = IM_COL32(60, 60, 80, 255);
    //     ImU32 wireColor = IM_COL32(100, 100, 120, 255);
    //     ImU32 textColor = isConnected ? 
    //         IM_COL32(0, 255, 100, 255) : 
    //         IM_COL32(255, 50, 50, 255);
        
    //     // Male connector
    //     ImVec2 maleStart = pos;
    //     ImVec2 maleEnd = ImVec2(pos.x + connectorWidth, pos.y + connectorHeight);
    //     draw->AddRectFilled(maleStart, maleEnd, connectorColor);
    //     draw->AddRect(maleStart, maleEnd, IM_COL32(150, 150, 180, 255));
        
    //     // Male nub
    //     ImVec2 nubStart = ImVec2(maleEnd.x, pos.y + connectorHeight * 0.3f);
    //     ImVec2 nubEnd = ImVec2(maleEnd.x + nubWidth, pos.y + connectorHeight * 0.7f);
    //     draw->AddRectFilled(nubStart, nubEnd, connectorColor);
    //     draw->AddRect(nubStart, nubEnd, IM_COL32(150, 150, 180, 255));
        
    //     // Female connector
    //     float femaleX = pos.x + connectorWidth + nubWidth + wireLength + separation;
    //     ImVec2 femaleStart = ImVec2(femaleX, pos.y);
    //     ImVec2 femaleEnd = ImVec2(femaleX + connectorWidth, pos.y + connectorHeight);
    //     draw->AddRectFilled(femaleStart, femaleEnd, connectorColor);
    //     draw->AddRect(femaleStart, femaleEnd, IM_COL32(150, 150, 180, 255));
        
    //     // Female socket
    //     ImVec2 socketStart = ImVec2(femaleStart.x - nubWidth, pos.y + connectorHeight * 0.3f);
    //     ImVec2 socketEnd = ImVec2(femaleStart.x, pos.y + connectorHeight * 0.7f);
    //     draw->AddRectFilled(socketStart, socketEnd, IM_COL32(30, 30, 50, 255));
    //     draw->AddRect(socketStart, socketEnd, IM_COL32(100, 100, 120, 255));
        
    //     // Wire
    //     ImVec2 wireStart = ImVec2(maleEnd.x + nubWidth, pos.y + connectorHeight * 0.5f);
    //     ImVec2 wireEnd = ImVec2(femaleStart.x - nubWidth, pos.y + connectorHeight * 0.5f);
        
    //     if (waveAmount > 0.1f) {
    //         ImVec2 prev = wireStart;
    //         int segments = 20;
    //         for (int i = 1; i <= segments; i++) {
    //             float t = (float)i / segments;
    //             float x = wireStart.x + (wireEnd.x - wireStart.x) * t;
    //             float wave = sin(t * 3.14159f * 2.0f) * waveAmount;
    //             ImVec2 point = ImVec2(x, wireStart.y + wave);
    //             draw->AddLine(prev, point, wireColor, 2.0f);
    //             prev = point;
    //         }
    //     } else {
    //         draw->AddLine(wireStart, wireEnd, wireColor, 2.0f);
    //     }
        
    //     // Center block
    //     if (animProgress > 0.1f) {
    //         float centerX = pos.x + connectorWidth + nubWidth + wireLength * 0.5f;
    //         float blockWidth = 60.0f * animProgress;
            
    //         ImVec2 blockStart = ImVec2(centerX - blockWidth * 0.5f, pos.y - 2.5f);
    //         ImVec2 blockEnd = ImVec2(centerX + blockWidth * 0.5f, pos.y + connectorHeight + 2.5f);
            
    //         draw->AddRectFilled(blockStart, blockEnd, IM_COL32(40, 40, 60, 200));
    //         draw->AddRect(blockStart, blockEnd, IM_COL32(150, 150, 180, 255));
            
    //         char idText[16];
    //         snprintf(idText, sizeof(idText), "%05d", signalID);
    //         ImVec2 textSize = ImGui::CalcTextSize(idText);
    //         ImVec2 textPos = ImVec2(
    //             centerX - textSize.x * 0.5f,
    //             pos.y + connectorHeight * 0.5f - textSize.y * 0.5f
    //         );
    //         draw->AddText(textPos, textColor, idText);
    //     }
        
    //     // Button interaction
    //     if (asButton) {
    //         ImVec2 buttonStart = ImVec2(pos.x, pos.y - 5);
    //         ImVec2 buttonEnd = ImVec2(femaleEnd.x, femaleEnd.y + 5);
            
    //         ImGui::SetCursorScreenPos(buttonStart);
    //         ImGui::InvisibleButton(label, ImVec2(buttonEnd.x - buttonStart.x, buttonEnd.y - buttonStart.y));
            
    //         if (ImGui::IsItemHovered()) {
    //             draw->AddRect(buttonStart, buttonEnd, IM_COL32(200, 200, 255, 100), 0.0f, 0, 2.0f);
    //         }
            
    //         if (ImGui::IsItemClicked()) {
    //             isConnected = !isConnected;
    //         }
    //     }
        
    //     // Label
    //     ImVec2 labelPos = ImVec2(pos.x - 50, pos.y + connectorHeight * 0.5f - 7);
    //     draw->AddText(labelPos, IM_COL32(150, 150, 180, 255), label);
    // }
    void Render(ImVec2 pos, bool asButton = false)
{
    ImDrawList* draw = ImGui::GetWindowDrawList();

    // ── EVA palette ───────────────────────────────────────────
    ImU32 glowHot    = IM_COL32(247, 183,  32, 255); // #F7B720
    ImU32 glowBloom  = IM_COL32(149,  43,  32, 255); // #952B20
    ImU32 fillDark   = IM_COL32( 20,  20,  20, 255);
    ImU32 redHot     = IM_COL32(200,  30,  10, 255);
    ImU32 redBloom   = IM_COL32(100,  10,   5, 255);
    ImU32 textColor  = isConnected ?
        IM_COL32(247, 183, 32, 255) :   // gold when connected
        IM_COL32(200,  30, 10, 255);    // red when severed

    // ── Dimensions ────────────────────────────────────────────
    const float connW   = 40.0f;
    const float connH   = 20.0f;
    const float nubW    = 8.0f;
    const float nubH    = connH * 0.4f;
    const float wireLen = 80.0f;
    const float gap     = 20.0f;
    const float round   = 2.0f;

    float separation = gap * (1.0f - animProgress);
    float waveAmount = (1.0f - animProgress) * 5.0f;

    ImU32 activeGlow  = isConnected ? glowHot   : redHot;
    ImU32 activeBloom = isConnected ? glowBloom  : redBloom;

    // ── Male connector ────────────────────────────────────────
    ImVec2 maleMin = pos;
    ImVec2 maleMax = ImVec2(pos.x + connW, pos.y + connH);
    DrawGlowRect(draw, maleMin, maleMax,
        fillDark, fillDark, IM_COL32(40, 40, 40, 255), round, 4, 0.8f);
    DrawGlowBorder(draw, maleMin, maleMax,
        activeGlow, activeBloom, round, 4, 0.9f);

    // Male nub (tab protruding right)
    float nubY    = pos.y + (connH - nubH) * 0.5f;
    ImVec2 nubMin = ImVec2(maleMax.x, nubY);
    ImVec2 nubMax = ImVec2(maleMax.x + nubW, nubY + nubH);
    draw->AddRectFilled(nubMin, nubMax, fillDark, 1.0f);
    DrawGlowBorder(draw, nubMin, nubMax,
        activeGlow, activeBloom, 1.0f, 3, 0.8f);

    // ── Female connector ──────────────────────────────────────
    float femaleX      = pos.x + connW + nubW + wireLen + separation;
    ImVec2 femaleMin   = ImVec2(femaleX, pos.y);
    ImVec2 femaleMax   = ImVec2(femaleX + connW, pos.y + connH);
    DrawGlowRect(draw, femaleMin, femaleMax,
        fillDark, fillDark, IM_COL32(40, 40, 40, 255), round, 4, 0.8f);
    DrawGlowBorder(draw, femaleMin, femaleMax,
        activeGlow, activeBloom, round, 4, 0.9f);

    // Female socket (indent on left)
    ImVec2 sockMin = ImVec2(femaleMin.x - nubW, nubY);
    ImVec2 sockMax = ImVec2(femaleMin.x,        nubY + nubH);
    draw->AddRectFilled(sockMin, sockMax, IM_COL32(10, 10, 10, 255), 1.0f);
    DrawGlowBorder(draw, sockMin, sockMax,
        activeGlow, activeBloom, 1.0f, 3, 0.8f);

    // ── Wire ──────────────────────────────────────────────────
    ImVec2 wireStart = ImVec2(maleMax.x + nubW,       pos.y + connH * 0.5f);
    ImVec2 wireEnd   = ImVec2(femaleMin.x - nubW,     pos.y + connH * 0.5f);

    if (waveAmount > 0.1f)
    {
        // Wavy disconnected wire
        ImVec2 prev = wireStart;
        int segments = 20;
        for (int i = 1; i <= segments; i++)
        {
            float t    = (float)i / segments;
            float x    = wireStart.x + (wireEnd.x - wireStart.x) * t;
            float wave = sin(t * 3.14159f * 2.0f) * waveAmount;
            ImVec2 pt  = ImVec2(x, wireStart.y + wave);
            // Glow layers on wire
            draw->AddLine(prev, pt, (activeBloom & 0x00FFFFFF) | (80u  << 24), 3.0f);
            draw->AddLine(prev, pt, (activeGlow  & 0x00FFFFFF) | (180u << 24), 1.5f);
            prev = pt;
        }
    }
    else
    {
        // Straight connected wire with glow
        draw->AddLine(wireStart, wireEnd, (activeBloom & 0x00FFFFFF) | (80u  << 24), 3.0f);
        draw->AddLine(wireStart, wireEnd, (activeGlow  & 0x00FFFFFF) | (220u << 24), 1.5f);
    }

    // ── Center block (connection ID) ──────────────────────────
    if (animProgress > 0.1f)
    {
        float  centerX   = pos.x + connW + nubW + wireLen * 0.5f;
        float  blockW    = 60.0f * animProgress;
        ImVec2 blockMin  = ImVec2(centerX - blockW * 0.5f, pos.y - 2.0f);
        ImVec2 blockMax  = ImVec2(centerX + blockW * 0.5f, pos.y + connH + 2.0f);

        DrawGlowRect(draw, blockMin, blockMax,
            fillDark, fillDark, IM_COL32(40, 40, 40, 255), 2.0f, 4, 0.8f);
        DrawGlowBorder(draw, blockMin, blockMax,
            activeGlow, activeBloom, 2.0f, 4, 0.9f);

        char idText[16];
        snprintf(idText, sizeof(idText), "%05d", signalID);
        ImVec2 textSize = ImGui::CalcTextSize(idText);
        ImVec2 textPos  = ImVec2(
            centerX - textSize.x * 0.5f,
            pos.y   + connH * 0.5f - textSize.y * 0.5f);
        draw->AddText(textPos, textColor, idText);
    }

    // ── Button interaction ─────────────────────────────────────
    if (asButton)
    {
        ImVec2 btnMin = ImVec2(pos.x, pos.y - 5);
        ImVec2 btnMax = ImVec2(femaleMax.x, femaleMax.y + 5);

        ImGui::SetCursorScreenPos(btnMin);
        ImGui::InvisibleButton(label,
            ImVec2(btnMax.x - btnMin.x, btnMax.y - btnMin.y));

        if (ImGui::IsItemHovered())
            DrawGlowBorder(draw, btnMin, btnMax,
                glowHot, glowBloom, 2.0f, 3, 0.8f);

        if (ImGui::IsItemClicked())
            isConnected = !isConnected;
    }

    // ── Label ─────────────────────────────────────────────────
    ImVec2 labelPos = ImVec2(
        pos.x - 50,
        pos.y + connH * 0.5f - ImGui::GetTextLineHeight() * 0.5f);
    draw->AddText(labelPos, textColor, label);
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