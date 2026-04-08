#include "tools/RectFilledTool.h"

#include "imgui.h"

#include <algorithm>
#include <vector>

namespace
{
    /**
     * @brief 栅格化填充矩形（包含边界），并受选区约束。
     */
    void rasterizeFilledRectangle(std::vector<uint32_t>& pixels,
                                  int canvasWidth,
                                  int canvasHeight,
                                  int x0,
                                  int y0,
                                  int x1,
                                  int y1,
                                  uint32_t color,
                                  const AppContext& context)
    {
        const int minX = std::max(0, std::min(x0, x1));
        const int maxX = std::min(canvasWidth - 1, std::max(x0, x1));
        const int minY = std::max(0, std::min(y0, y1));
        const int maxY = std::min(canvasHeight - 1, std::max(y0, y1));

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
} // namespace

bool RectFilledTool::apply(Project::Frame& frame,
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

void RectFilledTool::handleInteraction(AppContext& context,
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

    state_.endX = mousePixelX;
    state_.endY = mousePixelY;

    std::vector<uint32_t> previewPixels = state_.basePixels;
    rasterizeFilledRectangle(previewPixels,
                             canvasWidth,
                             canvasHeight,
                             state_.startX,
                             state_.startY,
                             state_.endX,
                             state_.endY,
                             context.getColorRGBA(),
                             context);
    frame.pixels.swap(previewPixels);

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        outPixelsCommitted = (frame.pixels != state_.basePixels);
        state_ = {};
    }
}

void RectFilledTool::renderOverlay(const AppContext& context,
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
}

void RectFilledTool::resetInteractionState(Project::Frame* frame)
{
    if (frame && state_.drawing && state_.hasBasePixels) frame->pixels = state_.basePixels;
    state_ = {};
}
