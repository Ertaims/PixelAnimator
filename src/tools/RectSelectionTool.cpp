#include "tools/RectSelectionTool.h"

#include "core/Project.h"
#include "render/SelectionOverlayRenderer.h"
#include "tools/selection/SelectionGeometry.h"
#include "tools/selection/SelectionTransform.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
    /**
     * @brief 将鼠标屏幕坐标转换为画布像素坐标，并做边界夹取。
     *
     * @param mousePos 鼠标在屏幕空间的位置
     * @param imagePos 画布图像左上角在屏幕空间的位置
     * @param zoom 当前画布缩放倍率（每个像素占据 zoom x zoom 屏幕像素）
     * @param canvasWidth 画布像素宽度
     * @param canvasHeight 画布像素高度
     * @param outX 输出的像素 X（已夹取到合法范围）
     * @param outY 输出的像素 Y（已夹取到合法范围）
     */
    void getClampedPixelFromMouse(const ImVec2& mousePos,
                                  const ImVec2& imagePos,
                                  int zoom,
                                  int canvasWidth,
                                  int canvasHeight,
                                  int& outX,
                                  int& outY)
    {
        const float localX = mousePos.x - imagePos.x;
        const float localY = mousePos.y - imagePos.y;
        outX = std::clamp(static_cast<int>(std::floor(localX / static_cast<float>(zoom))), 0, canvasWidth - 1);
        outY = std::clamp(static_cast<int>(std::floor(localY / static_cast<float>(zoom))), 0, canvasHeight - 1);
    }

    // 判断像素点是否位于给定矩形内。
    bool isPixelInRect(int x, int y, const AppContext::PixelRect& rect)
    {
        if (rect.width <= 0 || rect.height <= 0) return false;
        return x >= rect.x
            && y >= rect.y
            && x < (rect.x + rect.width)
            && y < (rect.y + rect.height);
    }

    // 计算“包含端点”的拖拽矩形。
    AppContext::PixelRect rectFromDragPixels(int x0, int y0, int x1, int y1)
    {
        AppContext::PixelRect rect;
        rect.x = std::min(x0, x1);
        rect.y = std::min(y0, y1);
        rect.width = std::abs(x1 - x0) + 1;
        rect.height = std::abs(y1 - y0) + 1;
        return rect;
    }

    /**
     * @brief 将像素矩形裁剪到画布范围内。
     *
     * @param rect 传入待裁剪矩形，返回裁剪后矩形
     * @param canvasWidth 画布像素宽度
     * @param canvasHeight 画布像素高度
     */
    void clampRectToCanvas(AppContext::PixelRect& rect, int canvasWidth, int canvasHeight)
    {
        if (canvasWidth <= 0 || canvasHeight <= 0)
        {
            rect = {};
            return;
        }

        const int x0 = std::clamp(rect.x, 0, canvasWidth - 1);
        const int y0 = std::clamp(rect.y, 0, canvasHeight - 1);
        const int x1 = std::clamp(rect.x + rect.width - 1, 0, canvasWidth - 1);
        const int y1 = std::clamp(rect.y + rect.height - 1, 0, canvasHeight - 1);
        rect.x = std::min(x0, x1);
        rect.y = std::min(y0, y1);
        rect.width = std::max(1, std::abs(x1 - x0) + 1);
        rect.height = std::max(1, std::abs(y1 - y0) + 1);
    }

    /**
     * @brief 测试鼠标是否命中 8 个缩放手柄。
     *
     * @param rect 当前选区外接矩形
     * @param imagePos 画布图像左上角屏幕坐标
     * @param zoom 当前缩放倍率
     * @param mousePos 鼠标屏幕坐标
     * @param handleHalfSize 手柄半尺寸（点击判定范围）
     * @return int 命中返回 0~7，未命中返回 -1
     */
    int hitTestSelectionHandle(const AppContext::PixelRect& rect,
                               const ImVec2& imagePos,
                               int zoom,
                               const ImVec2& mousePos,
                               float handleHalfSize)
    {
        const auto centers = render::getSelectionHandleCenters(rect, imagePos, zoom, handleHalfSize);
        int bestHandle = -1;
        float bestDistanceSq = 0.0f;

        for (int i = 0; i < static_cast<int>(centers.size()); ++i)
        {
            const ImVec2 c = centers[static_cast<size_t>(i)];
            if (mousePos.x >= c.x - handleHalfSize && mousePos.x <= c.x + handleHalfSize
                && mousePos.y >= c.y - handleHalfSize && mousePos.y <= c.y + handleHalfSize)
            {
                const float dx = mousePos.x - c.x;
                const float dy = mousePos.y - c.y;
                const float distanceSq = dx * dx + dy * dy;
                if (bestHandle < 0 || distanceSq < bestDistanceSq)
                {
                    bestHandle = i;
                    bestDistanceSq = distanceSq;
                }
            }
        }
        return bestHandle;
    }

    void captureSelectionMask(const AppContext& context,
                              int canvasWidth,
                              int canvasHeight,
                              std::vector<uint8_t>& outMask)
    {
        outMask.assign(static_cast<size_t>(canvasWidth) * static_cast<size_t>(canvasHeight), static_cast<uint8_t>(0));
        for (int y = 0; y < canvasHeight; ++y)
        {
            const size_t rowOffset = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth);
            for (int x = 0; x < canvasWidth; ++x)
            {
                if (context.isPixelSelected(x, y, canvasWidth, canvasHeight)) outMask[rowOffset + static_cast<size_t>(x)] = 1;
            }
        }
    }

    bool isSameRect(const AppContext::PixelRect& a, const AppContext::PixelRect& b)
    {
        return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
    }

    void captureSelectionMaskFromContext(const AppContext& context,
                                         int canvasWidth,
                                         int canvasHeight,
                                         std::vector<uint8_t>& outMask)
    {
        outMask.assign(static_cast<size_t>(canvasWidth) * static_cast<size_t>(canvasHeight), static_cast<uint8_t>(0));
        for (int y = 0; y < canvasHeight; ++y)
        {
            const size_t rowOffset = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth);
            for (int x = 0; x < canvasWidth; ++x)
            {
                if (context.isPixelSelected(x, y, canvasWidth, canvasHeight)) outMask[rowOffset + static_cast<size_t>(x)] = 1;
            }
        }
    }

    void applySelectionShapeOpToMask(std::vector<uint8_t>& ioMask,
                                     int canvasWidth,
                                     int canvasHeight,
                                     const AppContext::PixelRect& rect,
                                     AppContext::PixelSelectionOp op,
                                     RectSelectionTool::SelectionShape shape)
    {
        if (shape == RectSelectionTool::SelectionShape::Ellipse)
        {
            selection::applyEllipseOpToMask(ioMask, canvasWidth, canvasHeight, rect, op);
            return;
        }
        if (shape == RectSelectionTool::SelectionShape::MagicWand)
        {
            // 魔棒是“点击连通域”模式，不走拖拽矩形预览。
            // 这里保持原掩码不变，避免在框选预览分支误绘制矩形。
            return;
        }
        selection::applyRectOpToMask(ioMask, canvasWidth, canvasHeight, rect, op);
    }

} // namespace

bool RectSelectionTool::apply(Project::Frame& frame,
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

void RectSelectionTool::handleInteraction(AppContext& context,
                                          Project::Frame& frame,
                                          const ImVec2& mousePos,
                                          bool canvasHitboxHovered,
                                          bool hoveredOnImage,
                                          bool anyPopupOpen,
                                          const ImVec2& imagePos,
                                          int zoom,
                                          int canvasWidth,
                                          int canvasHeight,
                                          bool& outPixelsChanged)
{
    outPixelsChanged = false;

    if (anyPopupOpen)
    {
        m_state.mode = InteractionState::Mode::None;
        m_state.previewBoundsValid = false;
        m_state.previewFlipX = false;
        m_state.previewFlipY = false;
        return;
    }

    int mousePixelX = 0;
    int mousePixelY = 0;
    getClampedPixelFromMouse(mousePos, imagePos, zoom, canvasWidth, canvasHeight, mousePixelX, mousePixelY);
    m_state.hoverMouseX = mousePixelX;
    m_state.hoverMouseY = mousePixelY;

    AppContext::PixelRect currentBounds;
    const bool hasSelectionBounds = context.getPixelSelectionBounds(currentBounds);
    const float handleDrawHalf = std::max(4.0f, std::min(10.0f, static_cast<float>(zoom) * 0.45f));
    const float handleHitHalf = std::max(handleDrawHalf + 2.0f, std::min(14.0f, static_cast<float>(zoom) * 0.65f));
    const int hoveredHandle =
        hasSelectionBounds ? hitTestSelectionHandle(currentBounds, imagePos, zoom, mousePos, handleHitHalf) : -1;
    const bool insideSelection = hasSelectionBounds && isPixelInRect(mousePixelX, mousePixelY, currentBounds);

    if (canvasHitboxHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        // 手柄可能因贴边显示而略微压到画布边缘，因此这里优先相信“命中手柄”，
        // 不再强依赖 hoveredOnImage，避免边界手柄偶发点不中。
        if ((hoveredOnImage || canvasHitboxHovered) && hoveredHandle >= 0)
        {
            // 进入缩放拖拽前，准备“非破坏性变换缓存”。
            // 若当前选区与上次提交的选区不一致，则重建缓存基准。
            if (!m_sourceCacheValid || !isSameRect(currentBounds, m_lastCommittedBounds))
            {
                m_sourceFramePixels = frame.pixels;
                captureSelectionMask(context, canvasWidth, canvasHeight, m_sourceSelectionMask);
                m_sourceBounds = currentBounds;
                m_lastCommittedBounds = currentBounds;
                m_sourceCacheValid = true;
            }

            m_state.mode = InteractionState::Mode::Resizing;
            m_state.activeHandle = hoveredHandle;
            m_state.startMouseX = mousePixelX;
            m_state.startMouseY = mousePixelY;
            m_state.initialBounds = currentBounds;
            m_state.previewBounds = currentBounds;
            m_state.previewBoundsValid = true;
            m_state.previewFlipX = false;
            m_state.previewFlipY = false;
        }
        else if (hoveredOnImage && insideSelection)
        {
            /**
             * 关键修复：
             * - 平移必须严格基于“当前帧 + 当前选区”重建缓存；
             * - 不能复用缩放阶段留下的历史缓存，否则会把旧选区像素投影到新位置，
             *   导致你看到的“像素莫名移动/跑出框选区域”。
             */
            m_sourceFramePixels = frame.pixels;
            captureSelectionMask(context, canvasWidth, canvasHeight, m_sourceSelectionMask);
            m_sourceBounds = currentBounds;
            m_lastCommittedBounds = currentBounds;
            m_sourceCacheValid = true;

            m_state.mode = InteractionState::Mode::Moving;
            m_state.startMouseX = mousePixelX;
            m_state.startMouseY = mousePixelY;
            m_state.initialBounds = currentBounds;
            m_state.previewBounds = currentBounds;
            m_state.previewBoundsValid = true;
            m_state.previewFlipX = false;
            m_state.previewFlipY = false;
        }
        else if (hoveredOnImage)
        {
            if (m_selectionShape == SelectionShape::MagicWand)
            {
                const AppContext::PixelSelectionOp op = ImGui::GetIO().KeyCtrl
                    ? AppContext::PixelSelectionOp::Add
                    : AppContext::PixelSelectionOp::Replace;
                if (m_magicWandTool.applyFromSeed(
                        frame,
                        canvasWidth,
                        canvasHeight,
                        mousePixelX,
                        mousePixelY,
                        context,
                        op))
                {
                    // 选区定义发生变化，失效变换缓存。
                    m_sourceCacheValid = false;
                    m_sourceFramePixels.clear();
                    m_sourceSelectionMask.clear();
                }
                m_state = {};
            }
            else if (m_selectionShape == SelectionShape::PolygonLasso)
            {
                const AppContext::PixelSelectionOp clickOp = ImGui::GetIO().KeyCtrl
                    ? AppContext::PixelSelectionOp::Add
                    : AppContext::PixelSelectionOp::Replace;

                if (m_state.mode != InteractionState::Mode::PolygonLassoSelecting)
                {
                    m_state.mode = InteractionState::Mode::PolygonLassoSelecting;
                    m_state.removeMode = false;
                    m_state.previewOp = clickOp;
                    m_state.previewBoundsValid = false;
                    m_state.previewFlipX = false;
                    m_state.previewFlipY = false;
                    m_state.lassoPathPixels.clear();
                    m_state.lassoPathPixels.emplace_back(static_cast<float>(mousePixelX), static_cast<float>(mousePixelY));
                }
                else if (!m_state.removeMode)
                {
                    // 点击起点闭合：至少 3 个顶点时生效。
                    if (m_state.lassoPathPixels.size() >= 3
                        && selection::isCloseToFirstVertex(m_state.lassoPathPixels, mousePixelX, mousePixelY, 1))
                    {
                        std::vector<uint8_t> polygonMask;
                        if (selection::buildPolygonMaskFromVertices(m_state.lassoPathPixels, canvasWidth, canvasHeight, polygonMask))
                        {
                            context.applyMaskPixelSelection(polygonMask, canvasWidth, canvasHeight, m_state.previewOp);
                            m_sourceCacheValid = false;
                            m_sourceFramePixels.clear();
                            m_sourceSelectionMask.clear();
                        }
                        m_state = {};
                    }
                    else
                    {
                        const int lastX = static_cast<int>(m_state.lassoPathPixels.back().x);
                        const int lastY = static_cast<int>(m_state.lassoPathPixels.back().y);
                        if (lastX != mousePixelX || lastY != mousePixelY)
                            m_state.lassoPathPixels.emplace_back(static_cast<float>(mousePixelX), static_cast<float>(mousePixelY));
                    }
                }
            }
            else if (m_selectionShape == SelectionShape::Lasso)
            {
                m_state.mode = InteractionState::Mode::LassoSelecting;
                m_state.removeMode = false;
                m_state.previewOp = ImGui::GetIO().KeyCtrl
                    ? AppContext::PixelSelectionOp::Add
                    : AppContext::PixelSelectionOp::Replace;
                m_state.previewBoundsValid = false;
                m_state.previewFlipX = false;
                m_state.previewFlipY = false;
                m_state.lassoPathPixels.clear();
                m_state.lassoPathPixels.emplace_back(static_cast<float>(mousePixelX), static_cast<float>(mousePixelY));
            }
            else
            {
            m_state.mode = InteractionState::Mode::BoxSelecting;
            m_state.dragStartX = mousePixelX;
            m_state.dragStartY = mousePixelY;
            m_state.removeMode = false;
            // 左键框选：按下瞬间决定是 Replace 还是 Add，拖拽过程不再抖动。
            m_state.previewOp = ImGui::GetIO().KeyCtrl
                ? AppContext::PixelSelectionOp::Add
                : AppContext::PixelSelectionOp::Replace;
            m_state.previewBounds = rectFromDragPixels(mousePixelX, mousePixelY, mousePixelX, mousePixelY);
            m_state.previewBoundsValid = true;
            m_state.previewFlipX = false;
            m_state.previewFlipY = false;
            }
        }
    }

    if (canvasHitboxHovered && hoveredOnImage && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        if (m_selectionShape == SelectionShape::MagicWand)
        {
            if (m_magicWandTool.applyFromSeed(
                    frame,
                    canvasWidth,
                    canvasHeight,
                    mousePixelX,
                    mousePixelY,
                    context,
                    AppContext::PixelSelectionOp::Remove))
            {
                m_sourceCacheValid = false;
                m_sourceFramePixels.clear();
                m_sourceSelectionMask.clear();
            }
            m_state = {};
        }
        else if (m_selectionShape == SelectionShape::PolygonLasso)
        {
            if (m_state.mode != InteractionState::Mode::PolygonLassoSelecting)
            {
                m_state.mode = InteractionState::Mode::PolygonLassoSelecting;
                m_state.removeMode = true;
                m_state.previewOp = AppContext::PixelSelectionOp::Remove;
                m_state.previewBoundsValid = false;
                m_state.previewFlipX = false;
                m_state.previewFlipY = false;
                m_state.lassoPathPixels.clear();
                m_state.lassoPathPixels.emplace_back(static_cast<float>(mousePixelX), static_cast<float>(mousePixelY));
            }
            else if (m_state.removeMode)
            {
                if (m_state.lassoPathPixels.size() >= 3
                    && selection::isCloseToFirstVertex(m_state.lassoPathPixels, mousePixelX, mousePixelY, 1))
                {
                    std::vector<uint8_t> polygonMask;
                    if (selection::buildPolygonMaskFromVertices(m_state.lassoPathPixels, canvasWidth, canvasHeight, polygonMask))
                    {
                        context.applyMaskPixelSelection(polygonMask, canvasWidth, canvasHeight, m_state.previewOp);
                        m_sourceCacheValid = false;
                        m_sourceFramePixels.clear();
                        m_sourceSelectionMask.clear();
                    }
                    m_state = {};
                }
                else
                {
                    const int lastX = static_cast<int>(m_state.lassoPathPixels.back().x);
                    const int lastY = static_cast<int>(m_state.lassoPathPixels.back().y);
                    if (lastX != mousePixelX || lastY != mousePixelY)
                        m_state.lassoPathPixels.emplace_back(static_cast<float>(mousePixelX), static_cast<float>(mousePixelY));
                }
            }
            else
            {
                // 左键进行中的多边形套索遇到右键时直接取消，避免操作歧义。
                m_state = {};
            }
        }
        else if (m_selectionShape == SelectionShape::Lasso)
        {
            m_state.mode = InteractionState::Mode::LassoSelecting;
            m_state.removeMode = true;
            m_state.previewOp = AppContext::PixelSelectionOp::Remove;
            m_state.previewBoundsValid = false;
            m_state.previewFlipX = false;
            m_state.previewFlipY = false;
            m_state.lassoPathPixels.clear();
            m_state.lassoPathPixels.emplace_back(static_cast<float>(mousePixelX), static_cast<float>(mousePixelY));
        }
        else
        {
            m_state.mode = InteractionState::Mode::BoxSelecting;
            m_state.dragStartX = mousePixelX;
            m_state.dragStartY = mousePixelY;
            m_state.removeMode = true;
            m_state.previewOp = AppContext::PixelSelectionOp::Remove;
            m_state.previewBounds = rectFromDragPixels(mousePixelX, mousePixelY, mousePixelX, mousePixelY);
            m_state.previewBoundsValid = true;
            m_state.previewFlipX = false;
            m_state.previewFlipY = false;
        }
    }

    switch (m_state.mode)
    {
    case InteractionState::Mode::BoxSelecting:
    {
        const bool usingRight = m_state.removeMode;
        const bool stillDown = ImGui::IsMouseDown(usingRight ? ImGuiMouseButton_Right : ImGuiMouseButton_Left);
        m_state.previewBounds = rectFromDragPixels(m_state.dragStartX, m_state.dragStartY, mousePixelX, mousePixelY);
        m_state.previewBoundsValid = true;
        m_state.previewFlipX = false;
        m_state.previewFlipY = false;

        if (!stillDown)
        {
            const AppContext::PixelSelectionOp op = usingRight
                ? AppContext::PixelSelectionOp::Remove
                : m_state.previewOp;
            if (m_selectionShape == SelectionShape::Ellipse)
            {
                context.applyEllipsePixelSelection(
                    m_state.dragStartX,
                    m_state.dragStartY,
                    mousePixelX,
                    mousePixelY,
                    canvasWidth,
                    canvasHeight,
                    op);
            }
            else
            {
                context.applyRectPixelSelection(
                    m_state.dragStartX,
                    m_state.dragStartY,
                    mousePixelX,
                    mousePixelY,
                    canvasWidth,
                    canvasHeight,
                    op);
            }

            // 框选改动会改变选区定义，需要失效旧的变换缓存。
            m_sourceCacheValid = false;
            m_sourceFramePixels.clear();
            m_sourceSelectionMask.clear();
            m_state = {};
        }
        break;
    }
    case InteractionState::Mode::LassoSelecting:
    {
        const bool usingRight = m_state.removeMode;
        const bool stillDown = ImGui::IsMouseDown(usingRight ? ImGuiMouseButton_Right : ImGuiMouseButton_Left);
        if (!m_state.lassoPathPixels.empty())
        {
            const int lastX = static_cast<int>(m_state.lassoPathPixels.back().x);
            const int lastY = static_cast<int>(m_state.lassoPathPixels.back().y);
            selection::appendLineToPath(m_state.lassoPathPixels, lastX, lastY, mousePixelX, mousePixelY);
        }

        if (!stillDown)
        {
            std::vector<uint8_t> lassoMask;
            if (selection::buildLassoMaskFromPath(m_state.lassoPathPixels, canvasWidth, canvasHeight, lassoMask))
            {
                context.applyMaskPixelSelection(lassoMask, canvasWidth, canvasHeight, m_state.previewOp);
                m_sourceCacheValid = false;
                m_sourceFramePixels.clear();
                m_sourceSelectionMask.clear();
            }
            m_state = {};
        }
        break;
    }
    case InteractionState::Mode::PolygonLassoSelecting:
    {
        // 多边形套索由“点击事件”驱动：
        // - 点击加点；
        // - 点击起点闭合提交；
        // 因此这里不做按住拖拽处理。
        break;
    }
    case InteractionState::Mode::Moving:
    {
        const bool stillDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
        const int dx = mousePixelX - m_state.startMouseX;
        const int dy = mousePixelY - m_state.startMouseY;
        m_state.previewBounds = m_state.initialBounds;
        m_state.previewBounds.x += dx;
        m_state.previewBounds.y += dy;
        clampRectToCanvas(m_state.previewBounds, canvasWidth, canvasHeight);
        m_state.previewBoundsValid = true;

        // 实时预览：每帧都基于“拖拽开始快照”重算，避免累计误差。
        if (m_sourceCacheValid)
        {
            std::vector<uint32_t> previewPixels;
            selection::buildMovedPixelsFromSource(
                m_sourceFramePixels,
                previewPixels,
                m_sourceSelectionMask,
                canvasWidth,
                canvasHeight,
                dx,
                dy);
            if (frame.pixels != previewPixels)
            {
                frame.pixels.swap(previewPixels);
                outPixelsChanged = true;
            }
        }

        if (!stillDown)
        {
            context.movePixelSelection(dx, dy);
            m_lastCommittedBounds = m_state.previewBounds;
            m_state = {};
        }
        break;
    }
    case InteractionState::Mode::Resizing:
    {
        const bool stillDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
        const int dx = mousePixelX - m_state.startMouseX;
        const int dy = mousePixelY - m_state.startMouseY;
        const selection::ResizeResult resizeResult = selection::buildResizedRect(
            m_state.initialBounds,
            m_state.activeHandle,
            dx,
            dy,
            canvasWidth,
            canvasHeight,
            ImGui::GetIO().KeyCtrl);
        m_state.previewBounds = resizeResult.rect;
        m_state.previewBoundsValid = true;
        m_state.previewFlipX = resizeResult.flipX;
        m_state.previewFlipY = resizeResult.flipY;

        // 实时预览：每帧都基于“拖拽开始快照”做缩放变换，避免反复重采样劣化。
        if (m_sourceCacheValid)
        {
            std::vector<uint32_t> previewPixels;
            selection::buildScaledPixelsFromSource(
                m_sourceFramePixels,
                previewPixels,
                m_sourceSelectionMask,
                canvasWidth,
                canvasHeight,
                m_sourceBounds,
                m_state.previewBounds,
                m_state.previewFlipX,
                m_state.previewFlipY);
            if (frame.pixels != previewPixels)
            {
                frame.pixels.swap(previewPixels);
                outPixelsChanged = true;
            }
        }

        if (!stillDown)
        {
            context.transformPixelSelectionByRect(
                m_state.initialBounds,
                m_state.previewBounds,
                m_state.previewFlipX,
                m_state.previewFlipY);
            m_lastCommittedBounds = m_state.previewBounds;
            m_state = {};
        }
        break;
    }
    default:
        break;
    }

    // 光标反馈。
    if (m_state.mode == InteractionState::Mode::Moving)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    }
    else if (m_state.mode == InteractionState::Mode::LassoSelecting)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    else if (m_state.mode == InteractionState::Mode::PolygonLassoSelecting)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    else if (m_state.mode == InteractionState::Mode::Resizing || hoveredHandle >= 0)
    {
        switch (hoveredHandle >= 0 ? hoveredHandle : m_state.activeHandle)
        {
        case 0:
        case 4:
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
            break;
        case 2:
        case 6:
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNESW);
            break;
        case 1:
        case 5:
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
            break;
        case 3:
        case 7:
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            break;
        default:
            break;
        }
    }
    else if (hoveredOnImage && insideSelection)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    }
}

void RectSelectionTool::renderOverlay(const AppContext& context,
                                      ImDrawList* drawList,
                                      const ImVec2& imagePos,
                                      int zoom,
                                      bool anyPopupOpen) const
{
    if (!drawList) return;

    const float segmentLength = std::max(3.0f, std::min(8.0f, static_cast<float>(zoom) * 0.35f));
    const float antsSpeed = 70.0f;
    const float timePhase = static_cast<float>(ImGui::GetTime()) * antsSpeed;
    const Project* project = context.getProject();
    if (!project) return;

    const int canvasWidth = std::max(1, project->getWidth());
    const int canvasHeight = std::max(1, project->getHeight());

    std::vector<uint8_t> committedMask;
    captureSelectionMaskFromContext(context, canvasWidth, canvasHeight, committedMask);

    // displayMask 表示当前帧真正要显示的“选区轮廓依据”：
    // - 空闲：已提交选区；
    // - 框选：布尔运算后的临时结果（并可保留旧轮廓参考）；
    // - 平移/缩放：实时变换后的临时结果（不保留原始轮廓）。
    std::vector<uint8_t> displayMask = committedMask;
    ImU32 displayColor = IM_COL32(255, 210, 80, 255);

    if (m_state.mode == InteractionState::Mode::BoxSelecting && m_state.previewBoundsValid)
    {
        std::vector<uint8_t> previewMask = committedMask;
        applySelectionShapeOpToMask(
            previewMask,
            canvasWidth,
            canvasHeight,
            m_state.previewBounds,
            m_state.previewOp,
            m_selectionShape);
        displayMask.swap(previewMask);

        if (m_state.previewOp == AppContext::PixelSelectionOp::Add) displayColor = IM_COL32(90, 230, 140, 255);
        else if (m_state.previewOp == AppContext::PixelSelectionOp::Remove)
            displayColor = IM_COL32(255, 120, 120, 255);
        else
            displayColor = IM_COL32(80, 220, 255, 255);

        // 仅在框选阶段保留旧轮廓参考（用户此前明确希望此行为）。
        if (selection::maskHasAnySelected(committedMask)) render::drawMaskSolidOutline(drawList, committedMask, canvasWidth, canvasHeight, imagePos, zoom, IM_COL32(190, 170, 80, 180), 1.0f);
    }
    else if (m_state.mode == InteractionState::Mode::LassoSelecting && m_state.lassoPathPixels.size() >= 2)
    {
        std::vector<uint8_t> lassoMask;
        if (selection::buildLassoMaskFromPath(m_state.lassoPathPixels, canvasWidth, canvasHeight, lassoMask))
        {
            std::vector<uint8_t> previewMask = committedMask;
            selection::applyMaskOpToMask(previewMask, lassoMask, m_state.previewOp);
            displayMask.swap(previewMask);

            if (m_state.previewOp == AppContext::PixelSelectionOp::Add) displayColor = IM_COL32(90, 230, 140, 255);
            else if (m_state.previewOp == AppContext::PixelSelectionOp::Remove)
                displayColor = IM_COL32(255, 120, 120, 255);
            else
                displayColor = IM_COL32(80, 220, 255, 255);

            if (selection::maskHasAnySelected(committedMask)) render::drawMaskSolidOutline(drawList, committedMask, canvasWidth, canvasHeight, imagePos, zoom, IM_COL32(190, 170, 80, 180), 1.0f);
        }
    }
    else if (m_state.mode == InteractionState::Mode::PolygonLassoSelecting && !m_state.lassoPathPixels.empty())
    {
        std::vector<ImVec2> previewVertices = m_state.lassoPathPixels;
        const int hoverX = m_state.hoverMouseX;
        const int hoverY = m_state.hoverMouseY;
        const int lastX = static_cast<int>(previewVertices.back().x);
        const int lastY = static_cast<int>(previewVertices.back().y);
        if (lastX != hoverX || lastY != hoverY)
            previewVertices.emplace_back(static_cast<float>(hoverX), static_cast<float>(hoverY));

        if (previewVertices.size() >= 3)
        {
            std::vector<uint8_t> polygonMask;
            if (selection::buildPolygonMaskFromVertices(previewVertices, canvasWidth, canvasHeight, polygonMask))
            {
                std::vector<uint8_t> previewMask = committedMask;
                selection::applyMaskOpToMask(previewMask, polygonMask, m_state.previewOp);
                displayMask.swap(previewMask);

                if (m_state.previewOp == AppContext::PixelSelectionOp::Add) displayColor = IM_COL32(90, 230, 140, 255);
                else if (m_state.previewOp == AppContext::PixelSelectionOp::Remove)
                    displayColor = IM_COL32(255, 120, 120, 255);
                else
                    displayColor = IM_COL32(80, 220, 255, 255);

                if (selection::maskHasAnySelected(committedMask)) render::drawMaskSolidOutline(drawList, committedMask, canvasWidth, canvasHeight, imagePos, zoom, IM_COL32(190, 170, 80, 180), 1.0f);
            }
        }

        render::drawPolygonLassoGuide(drawList, previewVertices, m_state.lassoPathPixels, imagePos, zoom);
    }
    else if (m_state.mode == InteractionState::Mode::Moving && m_state.previewBoundsValid && m_sourceCacheValid)
    {
        const int dx = m_state.previewBounds.x - m_state.initialBounds.x;
        const int dy = m_state.previewBounds.y - m_state.initialBounds.y;
        selection::buildMovedMaskFromSource(m_sourceSelectionMask, displayMask, canvasWidth, canvasHeight, dx, dy);
        displayColor = IM_COL32(80, 220, 255, 255);
    }
    else if (m_state.mode == InteractionState::Mode::Resizing && m_state.previewBoundsValid && m_sourceCacheValid)
    {
        selection::buildScaledMaskFromSource(
            m_sourceSelectionMask,
            displayMask,
            canvasWidth,
            canvasHeight,
            m_sourceBounds,
            m_state.previewBounds,
            m_state.previewFlipX,
            m_state.previewFlipY);
        displayColor = IM_COL32(80, 220, 255, 255);
    }

    if (!selection::maskHasAnySelected(displayMask)) return;

    render::drawMaskSolidOutline(drawList, displayMask, canvasWidth, canvasHeight, imagePos, zoom, displayColor, 1.2f);
    render::drawMarchingAntsMask(drawList, displayMask, canvasWidth, canvasHeight, imagePos, zoom, segmentLength, timePhase);

    AppContext::PixelRect currentBounds;
    const bool hasCurrentBounds = context.getPixelSelectionBounds(currentBounds);

    /**
     * 8 手柄显示策略：
     * - 仅在“矩形框选工具被选中”时显示；
     * - 弹窗期间不显示；
     * - 交互拖拽进行中（框选/平移/缩放）不显示，避免遮挡编辑反馈。
     */
    const bool showHandles =
        !anyPopupOpen
        && context.getTool() == ToolType::RectSelection
        && m_state.mode == InteractionState::Mode::None;

    if (!showHandles) return;

    if (!hasCurrentBounds) return;

    const float handleHalf = std::max(4.0f, std::min(10.0f, static_cast<float>(zoom) * 0.45f));
    render::drawSelectionHandles(drawList, currentBounds, imagePos, zoom, handleHalf);
}

void RectSelectionTool::resetInteractionState()
{
    m_state = {};
    m_sourceCacheValid = false;
    m_sourceFramePixels.clear();
    m_sourceSelectionMask.clear();
}


