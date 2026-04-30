#include "render/SelectionOverlayRenderer.h"

#include <algorithm>
#include <cmath>

namespace render
{
    namespace
    {
        // 把画布像素矩形转换为 ImGui 绘制用的屏幕坐标矩形。
        void pixelRectToScreen(const AppContext::PixelRect& rect,
                               const ImVec2& imagePos,
                               int zoom,
                               ImVec2& outMin,
                               ImVec2& outMax)
        {
            outMin = ImVec2(imagePos.x + static_cast<float>(rect.x * zoom),
                            imagePos.y + static_cast<float>(rect.y * zoom));
            outMax = ImVec2(imagePos.x + static_cast<float>((rect.x + rect.width) * zoom),
                            imagePos.y + static_cast<float>((rect.y + rect.height) * zoom));
        }

        // 绘制一条黑白交替边，timePhase 用于形成“蚂蚁线”动画。
        void drawMarchingAntsEdge(ImDrawList* drawList,
                                  const ImVec2& p0,
                                  const ImVec2& p1,
                                  float edgeLen,
                                  float segmentLength,
                                  float timePhase,
                                  float offsetBase)
        {
            if (!drawList || edgeLen <= 0.0f || segmentLength <= 0.0f) return;

            const ImU32 whiteColor = IM_COL32(255, 255, 255, 255);
            const ImU32 blackColor = IM_COL32(20, 20, 20, 255);
            const float patternLength = segmentLength * 2.0f;
            const float phase = std::fmod(timePhase, patternLength);
            const ImVec2 dir((p1.x - p0.x) / edgeLen, (p1.y - p0.y) / edgeLen);

            for (float s = -phase; s < edgeLen; s += segmentLength)
            {
                const float start = std::max(0.0f, s);
                const float end = std::min(edgeLen, s + segmentLength);
                if (end <= start) continue;

                const int stripeIndex = static_cast<int>(std::floor((s + phase + offsetBase) / segmentLength));
                const ImU32 col = ((stripeIndex & 1) == 0) ? whiteColor : blackColor;
                const ImVec2 a(p0.x + dir.x * start, p0.y + dir.y * start);
                const ImVec2 b(p0.x + dir.x * end, p0.y + dir.y * end);
                drawList->AddLine(a, b, col, 2.0f);
            }
        }
    }

    std::array<ImVec2, 8> getSelectionHandleCenters(const AppContext::PixelRect& rect,
                                                    const ImVec2& imagePos,
                                                    int zoom,
                                                    float handleHalfSize)
    {
        ImVec2 minP(0.0f, 0.0f);
        ImVec2 maxP(0.0f, 0.0f);
        pixelRectToScreen(rect, imagePos, zoom, minP, maxP);

        const float screenWidth = std::max(0.0f, maxP.x - minP.x);
        const float screenHeight = std::max(0.0f, maxP.y - minP.y);
        const float insetX = std::min(handleHalfSize, screenWidth * 0.5f);
        const float insetY = std::min(handleHalfSize, screenHeight * 0.5f);

        const float leftX = minP.x + insetX;
        const float rightX = maxP.x - insetX;
        const float topY = minP.y + insetY;
        const float bottomY = maxP.y - insetY;
        const float midX = (leftX + rightX) * 0.5f;
        const float midY = (topY + bottomY) * 0.5f;

        return {{
            ImVec2(leftX, topY),
            ImVec2(midX, topY),
            ImVec2(rightX, topY),
            ImVec2(rightX, midY),
            ImVec2(rightX, bottomY),
            ImVec2(midX, bottomY),
            ImVec2(leftX, bottomY),
            ImVec2(leftX, midY)
        }};
    }

    void drawMaskSolidOutline(ImDrawList* drawList,
                              const std::vector<uint8_t>& mask,
                              int canvasWidth,
                              int canvasHeight,
                              const ImVec2& imagePos,
                              int zoom,
                              ImU32 color,
                              float thickness)
    {
        if (!drawList || zoom <= 0) return;
        if (mask.size() != static_cast<size_t>(canvasWidth) * static_cast<size_t>(canvasHeight)) return;

        const auto isSel = [&](int x, int y) -> bool {
            if (x < 0 || y < 0 || x >= canvasWidth || y >= canvasHeight) return false;
            const size_t idx = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth) + static_cast<size_t>(x);
            return mask[idx] != 0;
        };

        for (int y = 0; y < canvasHeight; ++y)
        {
            for (int x = 0; x < canvasWidth; ++x)
            {
                if (!isSel(x, y)) continue;

                const float x0 = imagePos.x + static_cast<float>(x * zoom);
                const float y0 = imagePos.y + static_cast<float>(y * zoom);
                const float x1 = x0 + static_cast<float>(zoom);
                const float y1 = y0 + static_cast<float>(zoom);

                if (!isSel(x, y - 1)) drawList->AddLine(ImVec2(x0, y0), ImVec2(x1, y0), color, thickness);
                if (!isSel(x + 1, y)) drawList->AddLine(ImVec2(x1, y0), ImVec2(x1, y1), color, thickness);
                if (!isSel(x, y + 1)) drawList->AddLine(ImVec2(x0, y1), ImVec2(x1, y1), color, thickness);
                if (!isSel(x - 1, y)) drawList->AddLine(ImVec2(x0, y0), ImVec2(x0, y1), color, thickness);
            }
        }
    }

    void drawMarchingAntsMask(ImDrawList* drawList,
                              const std::vector<uint8_t>& mask,
                              int canvasWidth,
                              int canvasHeight,
                              const ImVec2& imagePos,
                              int zoom,
                              float segmentLength,
                              float timePhase)
    {
        if (!drawList || zoom <= 0) return;
        if (mask.size() != static_cast<size_t>(canvasWidth) * static_cast<size_t>(canvasHeight)) return;

        const auto isSel = [&](int x, int y) -> bool {
            if (x < 0 || y < 0 || x >= canvasWidth || y >= canvasHeight) return false;
            const size_t idx = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth) + static_cast<size_t>(x);
            return mask[idx] != 0;
        };

        const float edgeLen = static_cast<float>(zoom);
        for (int y = 0; y < canvasHeight; ++y)
        {
            for (int x = 0; x < canvasWidth; ++x)
            {
                if (!isSel(x, y)) continue;

                const float x0 = imagePos.x + static_cast<float>(x * zoom);
                const float y0 = imagePos.y + static_cast<float>(y * zoom);
                const float x1 = x0 + static_cast<float>(zoom);
                const float y1 = y0 + static_cast<float>(zoom);
                const float base = static_cast<float>((x + y) * zoom);

                if (!isSel(x, y - 1)) drawMarchingAntsEdge(drawList, ImVec2(x0, y0), ImVec2(x1, y0), edgeLen, segmentLength, timePhase, base);
                if (!isSel(x + 1, y)) drawMarchingAntsEdge(drawList, ImVec2(x1, y0), ImVec2(x1, y1), edgeLen, segmentLength, timePhase, base + edgeLen);
                if (!isSel(x, y + 1)) drawMarchingAntsEdge(drawList, ImVec2(x0, y1), ImVec2(x1, y1), edgeLen, segmentLength, timePhase, base + edgeLen * 2.0f);
                if (!isSel(x - 1, y)) drawMarchingAntsEdge(drawList, ImVec2(x0, y0), ImVec2(x0, y1), edgeLen, segmentLength, timePhase, base + edgeLen * 3.0f);
            }
        }
    }

    void drawSelectionHandles(ImDrawList* drawList,
                              const AppContext::PixelRect& rect,
                              const ImVec2& imagePos,
                              int zoom,
                              float handleHalfSize)
    {
        if (!drawList) return;

        const auto handleCenters = getSelectionHandleCenters(rect, imagePos, zoom, handleHalfSize);
        for (const ImVec2& c : handleCenters)
        {
            drawList->AddRectFilled(
                ImVec2(c.x - handleHalfSize, c.y - handleHalfSize),
                ImVec2(c.x + handleHalfSize, c.y + handleHalfSize),
                IM_COL32(40, 120, 200, 255));
            drawList->AddRect(
                ImVec2(c.x - handleHalfSize, c.y - handleHalfSize),
                ImVec2(c.x + handleHalfSize, c.y + handleHalfSize),
                IM_COL32(255, 255, 255, 255));
        }
    }

    void drawPolygonLassoGuide(ImDrawList* drawList,
                               const std::vector<ImVec2>& previewVertices,
                               const std::vector<ImVec2>& fixedVertices,
                               const ImVec2& imagePos,
                               int zoom)
    {
        if (!drawList || previewVertices.empty()) return;

        const ImU32 guideColor = IM_COL32(0, 0, 0, 255);
        for (size_t i = 1; i < previewVertices.size(); ++i)
        {
            const ImVec2 p0(
                imagePos.x + (previewVertices[i - 1].x + 0.5f) * static_cast<float>(zoom),
                imagePos.y + (previewVertices[i - 1].y + 0.5f) * static_cast<float>(zoom));
            const ImVec2 p1(
                imagePos.x + (previewVertices[i].x + 0.5f) * static_cast<float>(zoom),
                imagePos.y + (previewVertices[i].y + 0.5f) * static_cast<float>(zoom));
            drawList->AddLine(p0, p1, guideColor, 1.0f);
        }

        const float vertexHalf = std::max(1.0f, static_cast<float>(zoom) * 0.12f);
        for (const ImVec2& v : fixedVertices)
        {
            const ImVec2 c(
                imagePos.x + (v.x + 0.5f) * static_cast<float>(zoom),
                imagePos.y + (v.y + 0.5f) * static_cast<float>(zoom));
            drawList->AddRectFilled(
                ImVec2(c.x - vertexHalf, c.y - vertexHalf),
                ImVec2(c.x + vertexHalf, c.y + vertexHalf),
                guideColor);
        }
    }
}
