#include "tools/CurveTool.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>

namespace
{
    // 以方形印章盖章，语义与 Brush/Line 统一。
    // 参数说明：
    // - cx/cy：当前落笔中心像素。
    // - brushSize：线宽（与画笔大小一致，1 表示单像素）。
    // - color：RGBA8888 目标颜色。
    // - context：用于判断选区约束（仅允许在可编辑区域改像素）。
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

    // 三次贝塞尔采样并栅格化到像素缓冲。
    //
    // 曲线定义：
    // P(t) = (1-t)^3 * P0
    //      + 3(1-t)^2 t * P1
    //      + 3(1-t)t^2 * P2
    //      + t^3 * P3,  t∈[0,1]
    //
    // 其中：
    // - P0 = 起点(start)
    // - P1 = 控制点1(control1)
    // - P2 = 控制点2(control2)
    // - P3 = 终点(end)
    //
    // 实现要点：
    // 1. 用 steps 对 [0,1] 做均匀采样，得到一串离散点；
    // 2. 相邻采样点之间再做一次线性补点，避免高曲率处出现“断点”；
    // 3. 每个离散点最终通过 stampBrushSquare 落笔，线宽与 BrushSize 一致。
    void rasterizeCubicBezier(std::vector<uint32_t>& pixels,
                              int canvasWidth,
                              int canvasHeight,
                              int x0,
                              int y0,
                              int c1x,
                              int c1y,
                              int c2x,
                              int c2y,
                              int x1,
                              int y1,
                              int brushSize,
                              uint32_t color,
                              const AppContext& context)
    {
        // 根据控制多边形跨度动态估算采样密度。
        // span 越大，steps 越大，曲线越平滑。
        const int chordDx = std::abs(x1 - x0);
        const int chordDy = std::abs(y1 - y0);
        const int controlDx0 = std::abs(c1x - x0);
        const int controlDy0 = std::abs(c1y - y0);
        const int controlDx1 = std::abs(c2x - c1x);
        const int controlDy1 = std::abs(c2y - c1y);
        const int controlDx2 = std::abs(x1 - c2x);
        const int controlDy2 = std::abs(y1 - c2y);
        const int span = std::max({chordDx, chordDy, controlDx0, controlDy0, controlDx1, controlDy1, controlDx2, controlDy2});
        const int steps = std::max(16, span * 4);

        // 先落起点，确保曲线首端一定被绘制。
        int lastX = x0;
        int lastY = y0;
        stampBrushSquare(pixels, canvasWidth, canvasHeight, lastX, lastY, brushSize, color, context);

        for (int i = 1; i <= steps; ++i)
        {
            // t 从 0->1 均匀推进，逐点计算贝塞尔曲线上的浮点坐标。
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            const float omt = 1.0f - t;
            const float px = omt * omt * omt * static_cast<float>(x0)
                + 3.0f * omt * omt * t * static_cast<float>(c1x)
                + 3.0f * omt * t * t * static_cast<float>(c2x)
                + t * t * t * static_cast<float>(x1);
            const float py = omt * omt * omt * static_cast<float>(y0)
                + 3.0f * omt * omt * t * static_cast<float>(c1y)
                + 3.0f * omt * t * t * static_cast<float>(c2y)
                + t * t * t * static_cast<float>(y1);

            const int curX = static_cast<int>(std::lround(px));
            const int curY = static_cast<int>(std::lround(py));

            // 将浮点坐标四舍五入到像素网格。
            const int dx = curX - lastX;
            const int dy = curY - lastY;
            const int lineSteps = std::max(std::abs(dx), std::abs(dy));
            // 邻采样点之间再插值，避免快速变化时断点。
            if (lineSteps <= 0)
            {
                stampBrushSquare(pixels, canvasWidth, canvasHeight, curX, curY, brushSize, color, context);
            }
            else
            {
                for (int s = 1; s <= lineSteps; ++s)
                {
                    const float lt = static_cast<float>(s) / static_cast<float>(lineSteps);
                    const int lx = lastX + static_cast<int>(std::lround(static_cast<float>(dx) * lt));
                    const int ly = lastY + static_cast<int>(std::lround(static_cast<float>(dy) * lt));
                    stampBrushSquare(pixels, canvasWidth, canvasHeight, lx, ly, brushSize, color, context);
                }
            }

            lastX = curX;
            lastY = curY;
        }
    }

    // 从起终点计算一组默认控制点：将线段三等分。
    // 这样在未调整控制点时，曲线会退化为接近直线且初始形态稳定。
    void buildDefaultControlPoints(int startX, int startY, int endX, int endY,
                                   int& outC1x, int& outC1y,
                                   int& outC2x, int& outC2y)
    {
        outC1x = static_cast<int>(std::lround((2.0 * startX + endX) / 3.0));
        outC1y = static_cast<int>(std::lround((2.0 * startY + endY) / 3.0));
        outC2x = static_cast<int>(std::lround((startX + 2.0 * endX) / 3.0));
        outC2y = static_cast<int>(std::lround((startY + 2.0 * endY) / 3.0));
    }
} // namespace

bool CurveTool::apply(Project::Frame& frame,
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

void CurveTool::handleInteraction(AppContext& context,
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

    // 任意弹窗打开时中断当前曲线交互，恢复快照，避免误提交。
    if (anyPopupOpen)
    {
        resetInteractionState(&frame);
        return;
    }

    // 左键点击驱动三阶段状态机：
    // Phase::None -> Phase::DefiningSegment（开始定义端点）
    // Phase::AdjustingControl1 -> Phase::AdjustingControl2（锁定控制点1）
    // Phase::AdjustingControl2 -> Commit（锁定控制点2并提交）
    if (canvasHitboxHovered && hoveredOnImage && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        if (state_.phase == InteractionState::Phase::None)
        {
            // 第一次点击：初始化起点并捕获像素快照。
            // 后续预览始终基于 basePixels 重算，避免预览叠加污染。
            state_.phase = InteractionState::Phase::DefiningSegment;
            state_.startX = mousePixelX;
            state_.startY = mousePixelY;
            state_.endX = mousePixelX;
            state_.endY = mousePixelY;
            buildDefaultControlPoints(
                state_.startX,
                state_.startY,
                state_.endX,
                state_.endY,
                state_.control1X,
                state_.control1Y,
                state_.control2X,
                state_.control2Y);
            state_.basePixels = frame.pixels;
            state_.hasBasePixels = true;
        }
        else if (state_.phase == InteractionState::Phase::AdjustingControl1)
        {
            // 第二次左键：锁定控制点1，进入控制点2调整阶段。
            state_.control1X = mousePixelX;
            state_.control1Y = mousePixelY;
            state_.phase = InteractionState::Phase::AdjustingControl2;
        }
        else if (state_.phase == InteractionState::Phase::AdjustingControl2)
        {
            // 第三次左键：提交最终曲线。
            // 这里再次从 basePixels 重算最终像素，保证提交结果与最后一次预览一致。
            state_.control2X = mousePixelX;
            state_.control2Y = mousePixelY;
            std::vector<uint32_t> finalPixels = state_.basePixels;
            rasterizeCubicBezier(finalPixels,
                                 canvasWidth,
                                 canvasHeight,
                                 state_.startX,
                                 state_.startY,
                                 state_.control1X,
                                 state_.control1Y,
                                 state_.control2X,
                                 state_.control2Y,
                                 state_.endX,
                                 state_.endY,
                                 context.getBrushSize(),
                                 context.getColorRGBA(),
                                 context);
            outPixelsCommitted = (finalPixels != state_.basePixels);
            frame.pixels.swap(finalPixels);
            state_ = {};
            return;
        }
    }

    if (state_.phase == InteractionState::Phase::None || !state_.hasBasePixels) return;

    if (state_.phase == InteractionState::Phase::DefiningSegment)
    {
        // 拖拽定义端点阶段：鼠标当前位置作为终点。
        state_.endX = mousePixelX;
        state_.endY = mousePixelY;
        buildDefaultControlPoints(
            state_.startX,
            state_.startY,
            state_.endX,
            state_.endY,
            state_.control1X,
            state_.control1Y,
            state_.control2X,
            state_.control2Y);

        std::vector<uint32_t> preview = state_.basePixels;
        rasterizeCubicBezier(preview,
                             canvasWidth,
                             canvasHeight,
                             state_.startX,
                             state_.startY,
                             state_.control1X,
                             state_.control1Y,
                             state_.control2X,
                             state_.control2Y,
                             state_.endX,
                             state_.endY,
                             context.getBrushSize(),
                             context.getColorRGBA(),
                             context);
        frame.pixels.swap(preview);

        // 松开左键后，端点确定，进入控制点1调整阶段。
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            state_.phase = InteractionState::Phase::AdjustingControl1;
        }
        return;
    }

    // 控制点调整阶段：鼠标位置即当前控制点，实时预览。
    // - AdjustingControl1：更新控制点1
    // - AdjustingControl2：更新控制点2
    if (state_.phase == InteractionState::Phase::AdjustingControl1)
    {
        state_.control1X = mousePixelX;
        state_.control1Y = mousePixelY;
    }
    else if (state_.phase == InteractionState::Phase::AdjustingControl2)
    {
        state_.control2X = mousePixelX;
        state_.control2Y = mousePixelY;
    }

    std::vector<uint32_t> preview = state_.basePixels;
    rasterizeCubicBezier(preview,
                         canvasWidth,
                         canvasHeight,
                         state_.startX,
                         state_.startY,
                         state_.control1X,
                         state_.control1Y,
                         state_.control2X,
                         state_.control2Y,
                         state_.endX,
                         state_.endY,
                         context.getBrushSize(),
                         context.getColorRGBA(),
                         context);
    frame.pixels.swap(preview);

    // 右键取消本次曲线交互并恢复快照。
    if (canvasHitboxHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        resetInteractionState(&frame);
        return;
    }
}

void CurveTool::renderOverlay(const AppContext& context,
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

void CurveTool::resetInteractionState(Project::Frame* frame)
{
    // 若当前处于交互中，恢复到交互开始前快照，确保不会留下半成品。
    if (frame && state_.hasBasePixels && state_.phase != InteractionState::Phase::None)
    {
        frame->pixels = state_.basePixels;
    }
    state_ = {};
}
