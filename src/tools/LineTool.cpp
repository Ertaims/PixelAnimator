#include "tools/LineTool.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>

namespace
{
    /**
     * @brief 在指定像素位置盖一个“方形笔刷印章”。
     *
     * 说明：
     * - 为了与 Brush 工具行为一致，线段粗细复用 context.getBrushSize() 语义；
     * - 若存在像素选区，仅允许在可编辑区域内写入像素。
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

    /**
     * @brief 使用 Bresenham 算法在目标缓冲中栅格化一条直线。
     *
     * 说明：
     * - 每个离散线段点都会调用 stampBrushSquare(...)，支持可配置线宽；
     * - 算法为纯整数步进，适合像素编辑器。
     */
    void rasterizeLine(std::vector<uint32_t>& pixels,
                       int canvasWidth,
                       int canvasHeight,
                       int x0,
                       int y0,
                       int x1,
                       int y1,
                       int brushSize,
                       uint32_t color,
                       const AppContext& context)
    {
        int x = x0;
        int y = y0;
        const int dx = std::abs(x1 - x0);
        const int dy = std::abs(y1 - y0);
        const int sx = (x0 < x1) ? 1 : -1;
        const int sy = (y0 < y1) ? 1 : -1;
        int err = dx - dy;

        while (true)
        {
            stampBrushSquare(pixels, canvasWidth, canvasHeight, x, y, brushSize, color, context);
            if (x == x1 && y == y1) break;
            const int e2 = err * 2;
            if (e2 > -dy)
            {
                err -= dy;
                x += sx;
            }
            if (e2 < dx)
            {
                err += dx;
                y += sy;
            }
        }
    }
} // namespace

bool LineTool::apply(Project::Frame& frame,
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

void LineTool::handleInteraction(AppContext& context,
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

    // 弹窗期间停止交互；若正在预览，则恢复到拖拽前快照避免误提交。
    if (anyPopupOpen)
    {
        resetInteractionState(&frame);
        return;
    }

    // 鼠标按下：开始一条新线段拖拽，记录起点并捕获像素快照。
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

    // 拖拽期间实时更新终点并基于快照重算预览。
    state_.endX = mousePixelX;
    state_.endY = mousePixelY;

    std::vector<uint32_t> previewPixels = state_.basePixels;
    rasterizeLine(previewPixels,
                  canvasWidth,
                  canvasHeight,
                  state_.startX,
                  state_.startY,
                  state_.endX,
                  state_.endY,
                  context.getBrushSize(),
                  context.getColorRGBA(),
                  context);
    frame.pixels.swap(previewPixels);

    // 鼠标抬起：结束拖拽并提交结果。
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        outPixelsCommitted = (frame.pixels != state_.basePixels);
        state_ = {};
    }
}

void LineTool::renderOverlay(const AppContext& context,
                             ImDrawList* drawList,
                             const ImVec2& imagePos,
                             int zoom,
                             bool anyPopupOpen) const
{
    (void)context;
    if (anyPopupOpen || !drawList || !state_.drawing) return;

    // 叠加层辅助线：用于明确显示当前拖拽方向与终点。
    const ImVec2 p0(
        imagePos.x + (static_cast<float>(state_.startX) + 0.5f) * static_cast<float>(zoom),
        imagePos.y + (static_cast<float>(state_.startY) + 0.5f) * static_cast<float>(zoom));
    const ImVec2 p1(
        imagePos.x + (static_cast<float>(state_.endX) + 0.5f) * static_cast<float>(zoom),
        imagePos.y + (static_cast<float>(state_.endY) + 0.5f) * static_cast<float>(zoom));

    drawList->AddLine(p0, p1, IM_COL32(255, 240, 80, 220), 1.5f);
}

void LineTool::resetInteractionState(Project::Frame* frame)
{
    // 若当前正处于预览拖拽中，且提供了帧指针，则恢复到快照避免“半成品残留”。
    if (frame && state_.drawing && state_.hasBasePixels)
    {
        frame->pixels = state_.basePixels;
    }
    state_ = {};
}
