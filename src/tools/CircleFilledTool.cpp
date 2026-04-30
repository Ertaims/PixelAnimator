#include "tools/CircleFilledTool.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
    int roundHalfUp(double value)
    {
        return static_cast<int>(std::floor(value + 0.5));
    }

    void fillHorizontalSpan(std::vector<uint32_t>& pixels,
                            int canvasWidth,
                            int canvasHeight,
                            int leftX,
                            int rightX,
                            int y,
                            uint32_t color,
                            const AppContext& context)
    {
        if (y < 0 || y >= canvasHeight) return;

        const int clippedLeft = std::max(0, std::min(leftX, rightX));
        const int clippedRight = std::min(canvasWidth - 1, std::max(leftX, rightX));
        const size_t rowOffset = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth);

        for (int x = clippedLeft; x <= clippedRight; ++x)
        {
            if (!context.canEditPixel(x, y, canvasWidth, canvasHeight)) continue;
            pixels[rowOffset + static_cast<size_t>(x)] = color;
        }
    }

    /**
     * @brief 按外接矩形绘制填充椭圆。
     *
     * 说明：
     * - 拖拽框宽高相同时得到正圆，宽高不同时得到椭圆；
     * - 使用与描边圆一致的 Aseprite-style 内收边界，避免填充圆显得过方；
     * - 受像素选区约束，选区外不会被修改。
     */
    void rasterizeEllipseFilled(std::vector<uint32_t>& pixels,
                                int canvasWidth,
                                int canvasHeight,
                                int minX,
                                int minY,
                                int maxX,
                                int maxY,
                                uint32_t color,
                                const AppContext& context)
    {
        const int width = maxX - minX + 1;
        const int height = maxY - minY + 1;
        if (width <= 0 || height <= 0) return;

        if (width == 1)
        {
            for (int y = minY; y <= maxY; ++y)
            {
                fillHorizontalSpan(pixels, canvasWidth, canvasHeight, minX, minX, y, color, context);
            }
            return;
        }

        if (height == 1)
        {
            fillHorizontalSpan(pixels, canvasWidth, canvasHeight, minX, maxX, minY, color, context);
            return;
        }

        const double centerX = (static_cast<double>(minX) + static_cast<double>(maxX)) * 0.5;
        const double centerY = (static_cast<double>(minY) + static_cast<double>(maxY)) * 0.5;
        const double radiusInset = 0.25;
        const double radiusX = std::max(0.5, static_cast<double>(width - 1) * 0.5 - radiusInset);
        const double radiusY = std::max(0.5, static_cast<double>(height - 1) * 0.5 - radiusInset);

        std::vector<int> rowLeft(static_cast<size_t>(height), canvasWidth);
        std::vector<int> rowRight(static_cast<size_t>(height), -1);

        auto markBoundaryPoint = [&](int x, int y) {
            if (y < minY || y > maxY) return;
            const size_t rowIndex = static_cast<size_t>(y - minY);
            rowLeft[rowIndex] = std::min(rowLeft[rowIndex], x);
            rowRight[rowIndex] = std::max(rowRight[rowIndex], x);
        };

        for (int x = minX; x <= maxX; ++x)
        {
            const double normalizedX = (static_cast<double>(x) - centerX) / radiusX;
            if (std::abs(normalizedX) > 1.0) continue;

            const double yOffset = radiusY * std::sqrt(std::max(0.0, 1.0 - normalizedX * normalizedX));
            const int topY = roundHalfUp(centerY - yOffset);
            const int bottomY = minY + maxY - topY;
            markBoundaryPoint(x, topY);
            markBoundaryPoint(x, bottomY);
        }

        for (int y = minY; y <= maxY; ++y)
        {
            const double normalizedY = (static_cast<double>(y) - centerY) / radiusY;
            if (std::abs(normalizedY) > 1.0) continue;

            const double xOffset = radiusX * std::sqrt(std::max(0.0, 1.0 - normalizedY * normalizedY));
            const int leftX = roundHalfUp(centerX - xOffset);
            const int rightX = minX + maxX - leftX;
            markBoundaryPoint(leftX, y);
            markBoundaryPoint(rightX, y);
        }

        for (int y = minY; y <= maxY; ++y)
        {
            const size_t rowIndex = static_cast<size_t>(y - minY);
            if (rowRight[rowIndex] < rowLeft[rowIndex]) continue;
            fillHorizontalSpan(pixels, canvasWidth, canvasHeight, rowLeft[rowIndex], rowRight[rowIndex], y, color, context);
        }
    }
}

bool CircleFilledTool::apply(Project::Frame& frame,
                           int canvasWidth,
                           int canvasHeight,
                           int x,
                           int y,
                           AppContext& context,
                           bool isMouseClicked) const
{
    (void)frame;
    (void)canvasWidth;
    (void)canvasHeight;
    (void)x;
    (void)y;
    (void)context;
    (void)isMouseClicked;
    return false;
}

void CircleFilledTool::handleInteraction(AppContext& context,
                                      Project::Frame& frame,
                                      bool canvasHitboxHovered,
                                      bool hoveredOnImage,
                                      bool anyPopupOpen,
                                      int mousePixelX,
                                      int mousePixelY,
                                      int canvasWidth,
                                      int canvasHeight,
                                      bool& outPixelsCommitted)
{
    outPixelsCommitted = false;

    if (anyPopupOpen)
    {
        resetInteractionState(&frame);
        return;
    }

    // 开始新圆形拖拽。
    if (canvasHitboxHovered && hoveredOnImage && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        m_state.drawing = true;
        m_state.startX = mousePixelX;
        m_state.startY = mousePixelY;
        m_state.endX = mousePixelX;
        m_state.endY = mousePixelY;
        m_state.basePixels = frame.pixels;
        m_state.hasBasePixels = true;
    }

    if (!m_state.drawing || !m_state.hasBasePixels) return;

    // 拖拽实时预览：每帧都从快照重算圆形，避免重复叠加。
    m_state.endX = mousePixelX;
    m_state.endY = mousePixelY;

    std::vector<uint32_t> previewPixels = m_state.basePixels;
    
    // 基于拖拽外接矩形绘制填充圆/椭圆。
    const int minX = std::min(m_state.startX, m_state.endX);
    const int maxX = std::max(m_state.startX, m_state.endX);
    const int minY = std::min(m_state.startY, m_state.endY);
    const int maxY = std::max(m_state.startY, m_state.endY);

    rasterizeEllipseFilled(
        previewPixels,
        canvasWidth,
        canvasHeight,
        minX,
        minY,
        maxX,
        maxY,
        context.getColorRGBA(),
        context);

    frame.pixels.swap(previewPixels);

    // 抬起提交。
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        outPixelsCommitted = (frame.pixels != m_state.basePixels);
        m_state = {};
    }
}

void CircleFilledTool::renderOverlay(const AppContext& context,
                                   ImDrawList* drawList,
                                   const ImVec2& imagePos,
                                   int zoom,
                                   bool anyPopupOpen) const
{
    (void)context;
    (void)drawList;
    (void)imagePos;
    (void)zoom;
    (void)anyPopupOpen;
    // 当前版本不额外绘制叠加层，实时预览直接体现在像素缓冲中。
}

void CircleFilledTool::resetInteractionState(Project::Frame* frame)
{
    if (frame && m_state.drawing && m_state.hasBasePixels) frame->pixels = m_state.basePixels;
    m_state = {};
}

