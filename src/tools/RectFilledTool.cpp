#include "tools/RectFilledTool.h"

#include "imgui.h"

#include <algorithm>
#include <vector>

namespace
{
    /**
     * @brief 栅格化填充矩形（包含边界），并受选区约束。
     * @param x0 左上角x坐标
     * @param y0 左上角y坐标
     * @param x1 右下角x坐标
     * @param y1 右下角y坐标
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
        const int X_Left = std::max(0, std::min(x0, x1));
        const int X_Right = std::min(canvasWidth - 1, std::max(x0, x1));
        const int Y_Top = std::max(0, std::min(y0, y1));
        const int Y_Bottom = std::min(canvasHeight - 1, std::max(y0, y1));

        for (int y = Y_Top; y <= Y_Bottom; ++y)
        {
            const size_t rowOffset = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth);
            for (int x = X_Left; x <= X_Right; ++x)
            {
                if (!context.canEditPixel(x, y, canvasWidth, canvasHeight)) continue;
                pixels[rowOffset + static_cast<size_t>(x)] = color;
            }
        }
    }
}

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
        m_state.drawing = true;
        m_state.startX = mousePixelX;
        m_state.startY = mousePixelY;
        m_state.endX = mousePixelX;
        m_state.endY = mousePixelY;
        m_state.basePixels = frame.pixels;
        m_state.hasBasePixels = true;
    }

    if (!m_state.drawing || !m_state.hasBasePixels) return;

    m_state.endX = mousePixelX;
    m_state.endY = mousePixelY;

    std::vector<uint32_t> previewPixels = m_state.basePixels;
    rasterizeFilledRectangle(previewPixels,
                             canvasWidth,
                             canvasHeight,
                             m_state.startX,
                             m_state.startY,
                             m_state.endX,
                             m_state.endY,
                             context.getColorRGBA(),
                             context);
    frame.pixels.swap(previewPixels);

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        outPixelsCommitted = (frame.pixels != m_state.basePixels);
        m_state = {};
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
    if (frame && m_state.drawing && m_state.hasBasePixels) frame->pixels = m_state.basePixels;
    m_state = {};
}

