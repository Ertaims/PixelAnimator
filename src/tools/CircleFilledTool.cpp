#include "tools/CircleFilledTool.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
    /**
     * @brief 按外接矩形绘制填充椭圆。
     *
     * 说明：
     * - 拖拽框宽高相同时得到正圆，宽高不同时得到椭圆；
     * - 使用像素中心采样，保证偶数尺寸下的形状仍保持对称；
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

        const double centerX = (static_cast<double>(minX) + static_cast<double>(maxX) + 1.0) * 0.5;
        const double centerY = (static_cast<double>(minY) + static_cast<double>(maxY) + 1.0) * 0.5;
        const double radiusX = static_cast<double>(width) * 0.5;
        const double radiusY = static_cast<double>(height) * 0.5;
        if (radiusX <= 0.0 || radiusY <= 0.0) return;

        const int clippedMinX = std::max(0, minX);
        const int clippedMaxX = std::min(canvasWidth - 1, maxX);
        const int clippedMinY = std::max(0, minY);
        const int clippedMaxY = std::min(canvasHeight - 1, maxY);

        for (int y = clippedMinY; y <= clippedMaxY; ++y)
        {
            const double normalizedY = (static_cast<double>(y) + 0.5 - centerY) / radiusY;
            const double normalizedYSquared = normalizedY * normalizedY;
            for (int x = clippedMinX; x <= clippedMaxX; ++x)
            {
                const double normalizedX = (static_cast<double>(x) + 0.5 - centerX) / radiusX;
                if ((normalizedX * normalizedX + normalizedYSquared) <= 1.0)
                {
                    if (context.canEditPixel(x, y, canvasWidth, canvasHeight))
                    {
                        const size_t offset = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth) + static_cast<size_t>(x);
                        pixels[offset] = color;
                    }
                }
            }
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
        state_.drawing = true;
        state_.startX = mousePixelX;
        state_.startY = mousePixelY;
        state_.endX = mousePixelX;
        state_.endY = mousePixelY;
        state_.basePixels = frame.pixels;
        state_.hasBasePixels = true;
    }

    if (!state_.drawing || !state_.hasBasePixels) return;

    // 拖拽实时预览：每帧都从快照重算圆形，避免重复叠加。
    state_.endX = mousePixelX;
    state_.endY = mousePixelY;

    std::vector<uint32_t> previewPixels = state_.basePixels;
    
    // 基于拖拽外接矩形绘制填充圆/椭圆。
    const int minX = std::min(state_.startX, state_.endX);
    const int maxX = std::max(state_.startX, state_.endX);
    const int minY = std::min(state_.startY, state_.endY);
    const int maxY = std::max(state_.startY, state_.endY);

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
        outPixelsCommitted = (frame.pixels != state_.basePixels);
        state_ = {};
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
    if (frame && state_.drawing && state_.hasBasePixels) frame->pixels = state_.basePixels;
    state_ = {};
}
