#include "tools/CircleTool.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
    /**
     * @brief 在指定像素位置盖一个“方形笔刷印章”。
     *
     * 说明：
     * - 复用 brushSize 语义，使圆形描边粗细与 Brush/Line 一致；
     * - 受像素选区约束，选区外不会被修改。
     */
    void stampBrushSquare(std::vector<uint32_t>& pixels,
                          int canvasWidth,
                          int canvasHeight,
                          int cx,
                          int cy,
                          int brushSize,
                          uint32_t color,
                          const AppContext& context)
    {
        const int radius = std::max(0, brushSize - 1);
        const int minX = std::max(0, cx - radius);
        const int maxX = std::min(canvasWidth - 1, cx + radius);
        const int minY = std::max(0, cy - radius);
        const int maxY = std::min(canvasHeight - 1, cy + radius);

        for (int y = minY; y <= maxY; ++y)
        {
            const size_t rowOffset = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth);
            for (int x = minX; x <= maxX; ++x)
            {
                if (!context.canEditPixel(x, y, canvasWidth, canvasHeight)) continue;
                pixels[rowOffset + static_cast<size_t>(x)] = color;
            }
        }
    }

    void plotBrushPoint(std::vector<uint32_t>& pixels,
                        int canvasWidth,
                        int canvasHeight,
                        int x,
                        int y,
                        int brushSize,
                        uint32_t color,
                        const AppContext& context)
    {
        if (x < 0 || x >= canvasWidth || y < 0 || y >= canvasHeight) return;
        stampBrushSquare(pixels, canvasWidth, canvasHeight, x, y, brushSize, color, context);
    }

    /**
     * @brief 按外接矩形绘制椭圆描边。
     *
     * 说明：
     * - 拖拽框不再被强制压成正方形，宽高不同即可得到椭圆；
     * - 同时按 x/y 两个方向采样边界，避免小尺寸像素画布上出现断点；
     * - 使用半像素圆心，让 16x16 这类偶数尺寸能对称贴住四边。
     */
    void rasterizeEllipseOutline(std::vector<uint32_t>& pixels,
                                 int canvasWidth,
                                 int canvasHeight,
                                 int minX,
                                 int minY,
                                 int maxX,
                                 int maxY,
                                 int brushSize,
                                 uint32_t color,
                                 const AppContext& context)
    {
        const int width = maxX - minX + 1;
        const int height = maxY - minY + 1;
        if (width <= 0 || height <= 0) return;

        if (width == 1 && height == 1)
        {
            plotBrushPoint(pixels, canvasWidth, canvasHeight, minX, minY, brushSize, color, context);
            return;
        }

        if (width == 1)
        {
            for (int y = minY; y <= maxY; ++y)
            {
                plotBrushPoint(pixels, canvasWidth, canvasHeight, minX, y, brushSize, color, context);
            }
            return;
        }

        if (height == 1)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                plotBrushPoint(pixels, canvasWidth, canvasHeight, x, minY, brushSize, color, context);
            }
            return;
        }

        const double centerX = (static_cast<double>(minX) + static_cast<double>(maxX)) * 0.5;
        const double centerY = (static_cast<double>(minY) + static_cast<double>(maxY)) * 0.5;
        const double radiusX = static_cast<double>(width - 1) * 0.5;
        const double radiusY = static_cast<double>(height - 1) * 0.5;

        for (int x = minX; x <= maxX; ++x)
        {
            const double normalizedX = (static_cast<double>(x) - centerX) / radiusX;
            const double yOffset = radiusY * std::sqrt(std::max(0.0, 1.0 - normalizedX * normalizedX));
            const int topY = static_cast<int>(std::lround(centerY - yOffset));
            const int bottomY = static_cast<int>(std::lround(centerY + yOffset));
            plotBrushPoint(pixels, canvasWidth, canvasHeight, x, topY, brushSize, color, context);
            plotBrushPoint(pixels, canvasWidth, canvasHeight, x, bottomY, brushSize, color, context);
        }

        for (int y = minY; y <= maxY; ++y)
        {
            const double normalizedY = (static_cast<double>(y) - centerY) / radiusY;
            const double xOffset = radiusX * std::sqrt(std::max(0.0, 1.0 - normalizedY * normalizedY));
            const int leftX = static_cast<int>(std::lround(centerX - xOffset));
            const int rightX = static_cast<int>(std::lround(centerX + xOffset));
            plotBrushPoint(pixels, canvasWidth, canvasHeight, leftX, y, brushSize, color, context);
            plotBrushPoint(pixels, canvasWidth, canvasHeight, rightX, y, brushSize, color, context);
        }
    }
}

bool CircleTool::apply(Project::Frame& frame,
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

void CircleTool::handleInteraction(AppContext& context,
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
        m_interactionState.drawing = true;
        m_interactionState.startX = mousePixelX;
        m_interactionState.startY = mousePixelY;
        m_interactionState.endX = mousePixelX;
        m_interactionState.endY = mousePixelY;
        m_interactionState.basePixels = frame.pixels;
        m_interactionState.hasBasePixels = true;
    }

    if (!m_interactionState.drawing || !m_interactionState.hasBasePixels) return;

    // 拖拽实时预览：每帧都从快照重算圆形，避免重复叠加。
    m_interactionState.endX = mousePixelX;
    m_interactionState.endY = mousePixelY;

    std::vector<uint32_t> previewPixels = m_interactionState.basePixels;
    
    // 基于拖拽外接矩形绘制圆/椭圆。
    const int minX = std::min(m_interactionState.startX, m_interactionState.endX);
    const int maxX = std::max(m_interactionState.startX, m_interactionState.endX);
    const int minY = std::min(m_interactionState.startY, m_interactionState.endY);
    const int maxY = std::max(m_interactionState.startY, m_interactionState.endY);

    rasterizeEllipseOutline(
        previewPixels,
        canvasWidth,
        canvasHeight,
        minX,
        minY,
        maxX,
        maxY,
        context.getBrushSize(),
        context.getColorRGBA(),
        context);

    frame.pixels.swap(previewPixels);

    // 抬起提交。
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        outPixelsCommitted = (frame.pixels != m_interactionState.basePixels);
        m_interactionState = {};
    }
}

void CircleTool::renderOverlay(const AppContext& context,
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

void CircleTool::resetInteractionState(Project::Frame* frame)
{
    if (frame && m_interactionState.drawing && m_interactionState.hasBasePixels) frame->pixels = m_interactionState.basePixels;
    m_interactionState = {};
}
