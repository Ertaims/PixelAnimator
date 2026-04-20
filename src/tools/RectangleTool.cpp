#include "tools/RectangleTool.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
    /**
     * @brief 在指定像素位置盖一个“方形笔刷印章”
     *
     * 说明：
     * - 复用 brushSize 语义，使矩形描边粗细与 Brush/Line 一致；
     * - 受像素选区约束，选区外不会被修改
     * @param pixels 缓冲区
     * @param canvasWidth 缓冲区宽
     * @param canvasHeight 缓冲区高
     * @param cx 绘制位置 X 坐标
     * @param cy 绘制位置 Y 坐标
     * @param brushSize 笔刷大小
     * @param color 绘制颜色
     * @param context 应用上下文
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
        const int radius = std::max(0, brushSize - 1);              // 笔刷半径
        const int X_Left = std::max(0, cx - radius);                  // 绘制区域最小 X 坐标
        const int X_Right = std::min(canvasWidth - 1, cx + radius);    // 绘制区域最大 X 坐标
        const int Y_Top = std::max(0, cy - radius);                  // 绘制区域最小 Y 坐标
        const int Y_Bottom = std::min(canvasHeight - 1, cy + radius);   // 绘制区域最大 Y 坐标

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

    /**
     * @brief Bresenham 线段栅格化，用于矩形四条边绘制
     * @pram pixels 缓冲区
     * @param x0 线段起点 X 坐标
     * @param y0 线段起点 Y 坐标
     * @param x1 线段终点 X 坐标
     * @param y1 线段终点 Y 坐标
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

    /**
     * @brief 在像素缓冲中绘制“描边矩形”
     *
     * 说明：
     * - 输入为任意两个对角点，会先归一化成 min/max；
     * - 采用四边线段绘制，保证与 Line 工具风格一致；
     * - 单点或单行/单列退化情形由 Bresenham 自动处理
     * 
     * @param x0 矩形左上角 X 坐标
     * @param y0 矩形左上角 Y 坐标
     * @param x1 矩形右下角 X 坐标
     * @param y1 矩形右下角 Y 坐标
     **/
    void rasterizeRectangleOutline(std::vector<uint32_t>& pixels,
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
        const int X_Left = std::min(x0, x1);
        const int X_Right = std::max(x0, x1);
        const int Y_Top = std::min(y0, y1);
        const int Y_Bottom = std::max(y0, y1);

        // 上边 + 下边
        rasterizeLine(pixels, canvasWidth, canvasHeight, X_Left, Y_Top, X_Right, Y_Top, brushSize, color, context);
        if (Y_Bottom != Y_Top) rasterizeLine(pixels, canvasWidth, canvasHeight, X_Left, Y_Bottom, X_Right, Y_Bottom, brushSize, color, context);

        // 左边 + 右边
        rasterizeLine(pixels, canvasWidth, canvasHeight, X_Left, Y_Top, X_Left, Y_Bottom, brushSize, color, context);
        if (X_Right != X_Left) rasterizeLine(pixels, canvasWidth, canvasHeight, X_Right, Y_Top, X_Right, Y_Bottom, brushSize, color, context);
    }
} // namespace

bool RectangleTool::apply(Project::Frame& frame,
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

void RectangleTool::handleInteraction(AppContext& context,
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

    // 开始新矩形拖拽
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

    // 拖拽实时预览：每帧都从快照重算矩形，避免重复叠加
    state_.endX = mousePixelX;
    state_.endY = mousePixelY;

    std::vector<uint32_t> previewPixels = state_.basePixels;
    rasterizeRectangleOutline(previewPixels,
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

    // 抬起提交
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        outPixelsCommitted = (frame.pixels != state_.basePixels);
        state_ = {};
    }
}

void RectangleTool::renderOverlay(const AppContext& context,
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
    // 当前版本不额外绘制叠加层，实时预览直接体现在像素缓冲中
}

void RectangleTool::resetInteractionState(Project::Frame* frame)
{
    if (frame && state_.drawing && state_.hasBasePixels) frame->pixels = state_.basePixels;
    state_ = {};
}
