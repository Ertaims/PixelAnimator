#include "tools/CurveTool.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>

namespace
{
    struct CurvePoint
    {
        double x = 0.0;
        double y = 0.0;
    };

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

    // 第一阶段只是拖拽确定起点和终点，此时预览应与 LineTool 完全一致。
    // 使用同一套 Bresenham 整数栅格化，避免贝塞尔采样把“直线预览”画出多余像素。
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

    CurvePoint midpoint(const CurvePoint& a, const CurvePoint& b)
    {
        return CurvePoint{(a.x + b.x) * 0.5, (a.y + b.y) * 0.5};
    }

    double distancePointToLine(const CurvePoint& p, const CurvePoint& a, const CurvePoint& b)
    {
        const double dx = b.x - a.x;
        const double dy = b.y - a.y;
        const double lenSq = dx * dx + dy * dy;
        if (lenSq <= 1e-6)
        {
            const double px = p.x - a.x;
            const double py = p.y - a.y;
            return std::sqrt(px * px + py * py);
        }

        // 用点到弦线段的垂直距离衡量当前曲线段是否“足够平”。
        const double area2 = std::abs(dy * p.x - dx * p.y + b.x * a.y - b.y * a.x);
        return area2 / std::sqrt(lenSq);
    }

    void appendAdaptiveCubicPolyline(const CurvePoint& p0,
                                     const CurvePoint& p1,
                                     const CurvePoint& p2,
                                     const CurvePoint& p3,
                                     int depth,
                                     std::vector<CurvePoint>& outPoints)
    {
        const double flatness = std::max(distancePointToLine(p1, p0, p3),
                                         distancePointToLine(p2, p0, p3));
        const double controlLength =
            std::hypot(p1.x - p0.x, p1.y - p0.y)
            + std::hypot(p2.x - p1.x, p2.y - p1.y)
            + std::hypot(p3.x - p2.x, p3.y - p2.y);

        // 曲线已经接近一条像素线时停止细分；否则继续拆成更短的贝塞尔段。
        if (depth >= 14 || flatness <= 0.18 || controlLength <= 1.0)
        {
            outPoints.push_back(p3);
            return;
        }

        const CurvePoint p01 = midpoint(p0, p1);
        const CurvePoint p12 = midpoint(p1, p2);
        const CurvePoint p23 = midpoint(p2, p3);
        const CurvePoint p012 = midpoint(p01, p12);
        const CurvePoint p123 = midpoint(p12, p23);
        const CurvePoint p0123 = midpoint(p012, p123);

        appendAdaptiveCubicPolyline(p0, p01, p012, p0123, depth + 1, outPoints);
        appendAdaptiveCubicPolyline(p0123, p123, p23, p3, depth + 1, outPoints);
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
    // 1. 先按曲率自适应细分贝塞尔，弯得厉害的位置自动给更多点；
    // 2. 相邻细分点之间使用 Bresenham 补线，保持像素线段稳定干净；
    // 3. 预览与最终提交共用该函数，避免“看起来”和“落下来”不一致。
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
        std::vector<CurvePoint> curvePoints;
        curvePoints.reserve(64);
        curvePoints.push_back(CurvePoint{static_cast<double>(x0), static_cast<double>(y0)});
        appendAdaptiveCubicPolyline(
            CurvePoint{static_cast<double>(x0), static_cast<double>(y0)},
            CurvePoint{static_cast<double>(c1x), static_cast<double>(c1y)},
            CurvePoint{static_cast<double>(c2x), static_cast<double>(c2y)},
            CurvePoint{static_cast<double>(x1), static_cast<double>(y1)},
            0,
            curvePoints);

        int lastX = static_cast<int>(std::lround(curvePoints.front().x));
        int lastY = static_cast<int>(std::lround(curvePoints.front().y));
        stampBrushSquare(pixels, canvasWidth, canvasHeight, lastX, lastY, brushSize, color, context);

        for (size_t i = 1; i < curvePoints.size(); ++i)
        {
            const int curX = static_cast<int>(std::lround(curvePoints[i].x));
            const int curY = static_cast<int>(std::lround(curvePoints[i].y));
            if (curX == lastX && curY == lastY) continue;

            rasterizeLine(
                pixels,
                canvasWidth,
                canvasHeight,
                lastX,
                lastY,
                curX,
                curY,
                brushSize,
                color,
                context);
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

    void buildCubicControlsForFirstBend(int startX,
                                        int startY,
                                        int bendX,
                                        int bendY,
                                        int endX,
                                        int endY,
                                        int& outC1x,
                                        int& outC1y,
                                        int& outC2x,
                                        int& outC2y)
    {
        // 第一弯需要介于两种手感之间：
        // - 直接把鼠标当控制点：太保守，曲线不够跟手；
        // - 强制曲线经过鼠标：太激进，容易“跟过头”。
        // 所以这里把两者插值，得到更接近像素动画软件的柔和弯曲手感。
        constexpr double followStrength = 0.45;
        const double handleControlX = static_cast<double>(bendX);
        const double handleControlY = static_cast<double>(bendY);
        const double throughPointControlX =
            2.0 * static_cast<double>(bendX)
            - 0.5 * (static_cast<double>(startX) + static_cast<double>(endX));
        const double throughPointControlY =
            2.0 * static_cast<double>(bendY)
            - 0.5 * (static_cast<double>(startY) + static_cast<double>(endY));
        const double quadControlX =
            handleControlX + (throughPointControlX - handleControlX) * followStrength;
        const double quadControlY =
            handleControlY + (throughPointControlY - handleControlY) * followStrength;

        // 把调好手感的二次贝塞尔等价转换成三次贝塞尔，便于第二弯继续调整。
        outC1x = static_cast<int>(std::lround(static_cast<double>(startX) + (2.0 / 3.0) * (quadControlX - static_cast<double>(startX))));
        outC1y = static_cast<int>(std::lround(static_cast<double>(startY) + (2.0 / 3.0) * (quadControlY - static_cast<double>(startY))));
        outC2x = static_cast<int>(std::lround(static_cast<double>(endX) + (2.0 / 3.0) * (quadControlX - static_cast<double>(endX))));
        outC2y = static_cast<int>(std::lround(static_cast<double>(endY) + (2.0 / 3.0) * (quadControlY - static_cast<double>(endY))));
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

    auto renderPreviewWithControls = [&](int c1x, int c1y, int c2x, int c2y)
    {
        std::vector<uint32_t> preview = m_state.basePixels;
        rasterizeCubicBezier(preview,
                             canvasWidth,
                             canvasHeight,
                             m_state.startX,
                             m_state.startY,
                             c1x,
                             c1y,
                             c2x,
                             c2y,
                             m_state.endX,
                             m_state.endY,
                             context.getBrushSize(),
                             context.getColorRGBA(),
                             context);
        frame.pixels.swap(preview);
    };

    auto renderPreviewFromBase = [&]()
    {
        renderPreviewWithControls(
            m_state.control1X,
            m_state.control1Y,
            m_state.control2X,
            m_state.control2Y);
    };

    auto updateControl2FromMouseDelta = [&]()
    {
        const int dx = mousePixelX - m_state.control2AnchorX;
        const int dy = mousePixelY - m_state.control2AnchorY;
        // 控制点2不是直接跳到鼠标位置，而是从确认第一弯时的位置开始按鼠标位移平移。
        // 这样鼠标刚开始移动时，曲线会从上一帧预览连续变化，不会突然变样。
        m_state.control2X = m_state.control2StartX + dx;
        m_state.control2Y = m_state.control2StartY + dy;
    };

    // 左键点击驱动三阶段状态机：
    // Phase::None -> Phase::DefiningSegment（开始定义端点）
    // Phase::AdjustingControl1 -> Phase::AdjustingControl2（锁定控制点1）
    // Phase::AdjustingControl2 -> Commit（锁定控制点2并提交）
    if (canvasHitboxHovered && hoveredOnImage && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        if (m_state.phase == InteractionState::Phase::None)
        {
            // 第一次点击：初始化起点并捕获像素快照。
            // 后续预览始终基于 basePixels 重算，避免预览叠加污染。
            m_state.phase = InteractionState::Phase::DefiningSegment;
            m_state.startX = mousePixelX;
            m_state.startY = mousePixelY;
            m_state.endX = mousePixelX;
            m_state.endY = mousePixelY;
            buildDefaultControlPoints(
                m_state.startX,
                m_state.startY,
                m_state.endX,
                m_state.endY,
                m_state.control1X,
                m_state.control1Y,
                m_state.control2X,
                m_state.control2Y);
            m_state.basePixels = frame.pixels;
            m_state.hasBasePixels = true;
        }
        else if (m_state.phase == InteractionState::Phase::AdjustingControl1)
        {
            // 第二次左键：锁定控制点1，进入控制点2调整阶段。
            // 第一弯按“二次贝塞尔弯点”理解，先转换成等价三次控制点。
            buildCubicControlsForFirstBend(
                m_state.startX,
                m_state.startY,
                mousePixelX,
                mousePixelY,
                m_state.endX,
                m_state.endY,
                m_state.control1X,
                m_state.control1Y,
                m_state.control2X,
                m_state.control2Y);
            m_state.control2AnchorX = mousePixelX;
            m_state.control2AnchorY = mousePixelY;
            m_state.control2StartX = m_state.control2X;
            m_state.control2StartY = m_state.control2Y;
            m_state.waitingForControl2Move = true;
            m_state.phase = InteractionState::Phase::AdjustingControl2;
            renderPreviewFromBase();
            return;
        }
        else if (m_state.phase == InteractionState::Phase::AdjustingControl2)
        {
            // 第三次左键：提交最终曲线。
            // 这里再次从 basePixels 重算最终像素，保证提交结果与最后一次预览一致。
            if (!(m_state.waitingForControl2Move
                  && mousePixelX == m_state.control2AnchorX
                  && mousePixelY == m_state.control2AnchorY))
            {
                updateControl2FromMouseDelta();
            }
            m_state.waitingForControl2Move = false;
            std::vector<uint32_t> finalPixels = m_state.basePixels;
            rasterizeCubicBezier(finalPixels,
                                 canvasWidth,
                                 canvasHeight,
                                 m_state.startX,
                                 m_state.startY,
                                 m_state.control1X,
                                 m_state.control1Y,
                                 m_state.control2X,
                                 m_state.control2Y,
                                 m_state.endX,
                                 m_state.endY,
                                 context.getBrushSize(),
                                 context.getColorRGBA(),
                                 context);
            outPixelsCommitted = (finalPixels != m_state.basePixels);
            frame.pixels.swap(finalPixels);
            m_state = {};
            return;
        }
    }

    if (m_state.phase == InteractionState::Phase::None || !m_state.hasBasePixels) return;

    if (m_state.phase == InteractionState::Phase::DefiningSegment)
    {
        // 拖拽定义端点阶段：鼠标当前位置作为终点。
        m_state.endX = mousePixelX;
        m_state.endY = mousePixelY;
        buildDefaultControlPoints(
            m_state.startX,
            m_state.startY,
            m_state.endX,
            m_state.endY,
            m_state.control1X,
            m_state.control1Y,
            m_state.control2X,
            m_state.control2Y);

        std::vector<uint32_t> preview = m_state.basePixels;
        rasterizeLine(preview,
                      canvasWidth,
                      canvasHeight,
                      m_state.startX,
                      m_state.startY,
                      m_state.endX,
                      m_state.endY,
                      context.getBrushSize(),
                      context.getColorRGBA(),
                      context);
        frame.pixels.swap(preview);

        // 松开左键后，端点确定，进入控制点1调整阶段。
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            m_state.phase = InteractionState::Phase::AdjustingControl1;
        }
        return;
    }

    // 右键取消本次曲线交互并恢复快照。
    if (canvasHitboxHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        resetInteractionState(&frame);
        return;
    }

    // 控制点调整阶段：
    // - AdjustingControl1：鼠标位置是“第一弯”的二次曲线弯点；
    // - AdjustingControl2：按鼠标位移平移控制点2，避免从默认控制点瞬移到鼠标位置。
    if (m_state.phase == InteractionState::Phase::AdjustingControl1)
    {
        int previewC1X = 0;
        int previewC1Y = 0;
        int previewC2X = 0;
        int previewC2Y = 0;
        buildCubicControlsForFirstBend(
            m_state.startX,
            m_state.startY,
            mousePixelX,
            mousePixelY,
            m_state.endX,
            m_state.endY,
            previewC1X,
            previewC1Y,
            previewC2X,
            previewC2Y);
        renderPreviewWithControls(previewC1X, previewC1Y, previewC2X, previewC2Y);
        return;
    }
    else if (m_state.phase == InteractionState::Phase::AdjustingControl2)
    {
        if (m_state.waitingForControl2Move
            && mousePixelX == m_state.control2AnchorX
            && mousePixelY == m_state.control2AnchorY)
        {
            // 刚确认控制点1后，鼠标通常还停在确认位置。
            // 此时保持控制点2原值，让“确认前一帧预览”和“确认后一帧预览”一致。
        }
        else
        {
            m_state.waitingForControl2Move = false;
            updateControl2FromMouseDelta();
        }
    }

    renderPreviewFromBase();
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

bool CurveTool::hasActiveInteraction() const
{
    return m_state.phase != InteractionState::Phase::None && m_state.hasBasePixels;
}

void CurveTool::resetInteractionState(Project::Frame* frame)
{
    // 若当前处于交互中，恢复到交互开始前快照，确保不会留下半成品。
    if (frame && m_state.hasBasePixels && m_state.phase != InteractionState::Phase::None)
    {
        frame->pixels = m_state.basePixels;
    }
    m_state = {};
}

