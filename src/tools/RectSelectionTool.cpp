#include "tools/RectSelectionTool.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace
{
// 将屏幕坐标映射为画布像素坐标，并夹到合法范围。
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
    if (rect.width <= 0 || rect.height <= 0)
        return false;
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

// 画布边界裁剪（AppContext::PixelRect 版本）。
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

// 像素矩形转屏幕矩形。
void convertPixelRectToScreen(const AppContext::PixelRect& rect,
                              const ImVec2& imagePos,
                              int zoom,
                              ImVec2& outMin,
                              ImVec2& outMax)
{
    outMin = ImVec2(imagePos.x + static_cast<float>(rect.x * zoom),
                    imagePos.y + static_cast<float>(rect.y * zoom));
    outMax = ImVec2(imagePos.x + static_cast<float>((rect.x + rect.width) * zoom),
                    imagePos.y + static_cast<float>((rect.y + rect.height) * zoom));
}

// 计算 8 个手柄中心点（NW/N/NE/E/SE/S/SW/W）。
std::array<ImVec2, 8> getSelectionHandleCenters(const AppContext::PixelRect& rect,
                                                const ImVec2& imagePos,
                                                int zoom)
{
    ImVec2 minP(0.0f, 0.0f);
    ImVec2 maxP(0.0f, 0.0f);
    convertPixelRectToScreen(rect, imagePos, zoom, minP, maxP);
    const float midX = (minP.x + maxP.x) * 0.5f;
    const float midY = (minP.y + maxP.y) * 0.5f;
    return {{
        ImVec2(minP.x, minP.y),
        ImVec2(midX, minP.y),
        ImVec2(maxP.x, minP.y),
        ImVec2(maxP.x, midY),
        ImVec2(maxP.x, maxP.y),
        ImVec2(midX, maxP.y),
        ImVec2(minP.x, maxP.y),
        ImVec2(minP.x, midY)
    }};
}

// 手柄命中测试：命中返回 0~7，否则 -1。
int hitTestSelectionHandle(const AppContext::PixelRect& rect,
                           const ImVec2& imagePos,
                           int zoom,
                           const ImVec2& mousePos,
                           float handleHalfSize)
{
    const auto centers = getSelectionHandleCenters(rect, imagePos, zoom);
    for (int i = 0; i < static_cast<int>(centers.size()); ++i)
    {
        const ImVec2 c = centers[static_cast<size_t>(i)];
        if (mousePos.x >= c.x - handleHalfSize && mousePos.x <= c.x + handleHalfSize
            && mousePos.y >= c.y - handleHalfSize && mousePos.y <= c.y + handleHalfSize)
        {
            return i;
        }
    }
    return -1;
}

// 缩放结果：包含标准化后的目标矩形，以及是否发生轴向翻转。
struct ResizeResult
{
    AppContext::PixelRect rect;
    bool flipX = false;
    bool flipY = false;
};

// 根据手柄拖拽构造目标矩形；支持越过对边后的 X/Y 反转，Ctrl 时保持等比。
ResizeResult buildResizedRect(const AppContext::PixelRect& initial,
                              int handle,
                              int deltaX,
                              int deltaY,
                              int canvasWidth,
                              int canvasHeight,
                              bool keepAspect)
{
    int left = initial.x;
    int top = initial.y;
    int right = initial.x + initial.width - 1;
    int bottom = initial.y + initial.height - 1;

    const bool moveLeft = (handle == 0 || handle == 6 || handle == 7);
    const bool moveRight = (handle == 2 || handle == 3 || handle == 4);
    const bool moveTop = (handle == 0 || handle == 1 || handle == 2);
    const bool moveBottom = (handle == 4 || handle == 5 || handle == 6);

    if (moveLeft)
        left += deltaX;
    if (moveRight)
        right += deltaX;
    if (moveTop)
        top += deltaY;
    if (moveBottom)
        bottom += deltaY;

    if (keepAspect && initial.width > 0 && initial.height > 0)
    {
        const float aspect = static_cast<float>(initial.width) / static_cast<float>(initial.height);
        const int curW = std::abs(right - left) + 1;
        const int curH = std::abs(bottom - top) + 1;
        int newW = curW;
        int newH = curH;

        if ((moveLeft || moveRight) && (moveTop || moveBottom))
        {
            const float sx = static_cast<float>(std::max(1, curW)) / static_cast<float>(std::max(1, initial.width));
            const float sy = static_cast<float>(std::max(1, curH)) / static_cast<float>(std::max(1, initial.height));
            const float s = std::max(sx, sy);
            newW = std::max(1, static_cast<int>(std::lround(static_cast<float>(initial.width) * s)));
            newH = std::max(1, static_cast<int>(std::lround(static_cast<float>(initial.height) * s)));
        }
        else if (moveLeft || moveRight)
        {
            newW = std::max(1, curW);
            newH = std::max(1, static_cast<int>(std::lround(static_cast<float>(newW) / aspect)));
        }
        else if (moveTop || moveBottom)
        {
            newH = std::max(1, curH);
            newW = std::max(1, static_cast<int>(std::lround(static_cast<float>(newH) * aspect)));
        }

        if (moveLeft && !moveRight)
            left = right - newW + 1;
        else
            right = left + ((right >= left) ? (newW - 1) : -(newW - 1));

        if (moveTop && !moveBottom)
            top = bottom - newH + 1;
        else if (moveTop || moveBottom)
            bottom = top + ((bottom >= top) ? (newH - 1) : -(newH - 1));
        else
        {
            const float centerY = static_cast<float>(initial.y) + (static_cast<float>(initial.height - 1) * 0.5f);
            top = static_cast<int>(std::lround(centerY - static_cast<float>(newH - 1) * 0.5f));
            bottom = top + newH - 1;
        }

        if ((moveTop || moveBottom) && !(moveLeft || moveRight))
        {
            const float centerX = static_cast<float>(initial.x) + (static_cast<float>(initial.width - 1) * 0.5f);
            left = static_cast<int>(std::lround(centerX - static_cast<float>(newW - 1) * 0.5f));
            right = left + newW - 1;
        }
    }

    ResizeResult out;
    out.flipX = right < left;
    out.flipY = bottom < top;

    AppContext::PixelRect normalized;
    normalized.x = std::min(left, right);
    normalized.y = std::min(top, bottom);
    normalized.width = std::abs(right - left) + 1;
    normalized.height = std::abs(bottom - top) + 1;
    clampRectToCanvas(normalized, canvasWidth, canvasHeight);
    out.rect = normalized;
    return out;
}

// 绘制蚂蚁线边框。
void drawMarchingAntsRect(ImDrawList* drawList,
                          const ImVec2& minP,
                          const ImVec2& maxP,
                          float segmentLength,
                          float timePhase)
{
    if (!drawList)
        return;
    const float width = maxP.x - minP.x;
    const float height = maxP.y - minP.y;
    if (width <= 0.0f || height <= 0.0f || segmentLength <= 0.0f)
        return;

    const ImU32 whiteColor = IM_COL32(255, 255, 255, 255);
    const ImU32 blackColor = IM_COL32(20, 20, 20, 255);
    const float patternLength = segmentLength * 2.0f;
    const float phase = std::fmod(timePhase, patternLength);

    auto drawEdge = [&](const ImVec2& p0, const ImVec2& p1, float edgeLen, float offsetBase) {
        if (edgeLen <= 0.0f)
            return;

        const ImVec2 dir((p1.x - p0.x) / edgeLen, (p1.y - p0.y) / edgeLen);
        for (float s = -phase; s < edgeLen; s += segmentLength)
        {
            const float start = std::max(0.0f, s);
            const float end = std::min(edgeLen, s + segmentLength);
            if (end <= start)
                continue;

            const int stripeIndex = static_cast<int>(std::floor((s + phase + offsetBase) / segmentLength));
            const ImU32 col = ((stripeIndex & 1) == 0) ? whiteColor : blackColor;
            const ImVec2 a(p0.x + dir.x * start, p0.y + dir.y * start);
            const ImVec2 b(p0.x + dir.x * end, p0.y + dir.y * end);
            drawList->AddLine(a, b, col, 2.0f);
        }
    };

    drawEdge(ImVec2(minP.x, minP.y), ImVec2(maxP.x, minP.y), width, 0.0f);
    drawEdge(ImVec2(maxP.x, minP.y), ImVec2(maxP.x, maxP.y), height, width);
    drawEdge(ImVec2(maxP.x, maxP.y), ImVec2(minP.x, maxP.y), width, width + height);
    drawEdge(ImVec2(minP.x, maxP.y), ImVec2(minP.x, minP.y), height, width + height + width);
}

/**
 * @brief 将当前选区像素整体平移到新位置，并执行“剪切式移动”。
 *
 * 行为说明：
 * - 先把源选区像素从结果图中清空（透明）；
 * - 再把源选区像素搬运到 (x+dx, y+dy)；
 * - 越界目标像素自动裁掉；
 * - 仅处理“当前选区中的像素”，其余像素保持不变。
 */
bool buildMovedPixelsFromSource(const std::vector<uint32_t>& sourcePixels,
                                std::vector<uint32_t>& outPixels,
                                const std::vector<uint8_t>& sourceMask,
                                int canvasWidth,
                                int canvasHeight,
                                int dx,
                                int dy)
{
    outPixels = sourcePixels;

    // 1) 先清空源选区像素。
    for (int y = 0; y < canvasHeight; ++y)
    {
        const size_t rowOffset = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth);
        for (int x = 0; x < canvasWidth; ++x)
        {
            const size_t idx = rowOffset + static_cast<size_t>(x);
            if (idx >= sourceMask.size() || sourceMask[idx] == 0)
                continue;
            outPixels[idx] = 0x00000000u;
        }
    }

    // 2) 再把源选区像素搬运到目标位置。
    for (int y = 0; y < canvasHeight; ++y)
    {
        const size_t rowOffset = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth);
        for (int x = 0; x < canvasWidth; ++x)
        {
            const size_t srcIdx = rowOffset + static_cast<size_t>(x);
            if (srcIdx >= sourceMask.size() || sourceMask[srcIdx] == 0)
                continue;

            const int nx = x + dx;
            const int ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= canvasWidth || ny >= canvasHeight)
                continue;

            const size_t dstIdx = static_cast<size_t>(ny) * static_cast<size_t>(canvasWidth) + static_cast<size_t>(nx);
            outPixels[dstIdx] = sourcePixels[srcIdx];
        }
    }

    return outPixels != sourcePixels;
}

/**
 * @brief 将当前选区像素按 fromRect -> toRect 做缩放变换（最近邻，反向采样）。
 *
 * 行为说明：
 * - 与选区掩码一致，采用“目标反查源”的方式避免空洞；
 * - 只会搬运“源选区中的像素”；
 * - 同样是剪切式：源选区先清空，再写入目标；
 * - 非选区像素保持不变。
 */
bool buildScaledPixelsFromSource(const std::vector<uint32_t>& sourcePixels,
                                 std::vector<uint32_t>& outPixels,
                                 const std::vector<uint8_t>& sourceMask,
                                 int canvasWidth,
                                 int canvasHeight,
                                 const AppContext::PixelRect& fromRect,
                                 const AppContext::PixelRect& toRect,
                                 bool flipX,
                                 bool flipY)
{
    if (fromRect.width <= 0 || fromRect.height <= 0 || toRect.width <= 0 || toRect.height <= 0)
        return false;
    outPixels = sourcePixels;

    // 1) 清空源选区像素。
    for (int y = 0; y < canvasHeight; ++y)
    {
        const size_t rowOffset = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth);
        for (int x = 0; x < canvasWidth; ++x)
        {
            const size_t idx = rowOffset + static_cast<size_t>(x);
            if (idx >= sourceMask.size() || sourceMask[idx] == 0)
                continue;
            outPixels[idx] = 0x00000000u;
        }
    }

    // 2) 目标像素反向映射到源矩形。
    for (int dy = toRect.y; dy < toRect.y + toRect.height; ++dy)
    {
        for (int dx = toRect.x; dx < toRect.x + toRect.width; ++dx)
        {
            if (dx < 0 || dy < 0 || dx >= canvasWidth || dy >= canvasHeight)
                continue;

            const float u = (toRect.width <= 1)
                ? 0.0f
                : static_cast<float>(dx - toRect.x) / static_cast<float>(toRect.width - 1);
            const float v = (toRect.height <= 1)
                ? 0.0f
                : static_cast<float>(dy - toRect.y) / static_cast<float>(toRect.height - 1);

            const float sampleU = flipX ? (1.0f - u) : u;
            const float sampleV = flipY ? (1.0f - v) : v;
            const int sx = fromRect.x + static_cast<int>(std::lround(sampleU * static_cast<float>(fromRect.width - 1)));
            const int sy = fromRect.y + static_cast<int>(std::lround(sampleV * static_cast<float>(fromRect.height - 1)));
            if (sx < 0 || sy < 0 || sx >= canvasWidth || sy >= canvasHeight)
                continue;

            const size_t srcIdx = static_cast<size_t>(sy) * static_cast<size_t>(canvasWidth) + static_cast<size_t>(sx);
            if (srcIdx >= sourceMask.size() || sourceMask[srcIdx] == 0)
                continue;
            const size_t dstIdx = static_cast<size_t>(dy) * static_cast<size_t>(canvasWidth) + static_cast<size_t>(dx);
            outPixels[dstIdx] = sourcePixels[srcIdx];
        }
    }

    return outPixels != sourcePixels;
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
            if (context.isPixelSelected(x, y, canvasWidth, canvasHeight))
                outMask[rowOffset + static_cast<size_t>(x)] = 1;
        }
    }
}

bool isSameRect(const AppContext::PixelRect& a, const AppContext::PixelRect& b)
{
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
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
        state_.mode = InteractionState::Mode::None;
        state_.previewBoundsValid = false;
        state_.previewFlipX = false;
        state_.previewFlipY = false;
        return;
    }

    int mousePixelX = 0;
    int mousePixelY = 0;
    getClampedPixelFromMouse(mousePos, imagePos, zoom, canvasWidth, canvasHeight, mousePixelX, mousePixelY);

    AppContext::PixelRect currentBounds;
    const bool hasSelectionBounds = context.getPixelSelectionBounds(currentBounds);
    const float handleHalf = std::max(4.0f, std::min(10.0f, static_cast<float>(zoom) * 0.45f));
    const int hoveredHandle =
        hasSelectionBounds ? hitTestSelectionHandle(currentBounds, imagePos, zoom, mousePos, handleHalf) : -1;
    const bool insideSelection = hasSelectionBounds && isPixelInRect(mousePixelX, mousePixelY, currentBounds);

    if (canvasHitboxHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        if (hoveredOnImage && hoveredHandle >= 0)
        {
            // 进入缩放拖拽前，准备“非破坏性变换缓存”。
            // 若当前选区与上次提交的选区不一致，则重建缓存基准。
            if (!sourceCacheValid_ || !isSameRect(currentBounds, lastCommittedBounds_))
            {
                sourceFramePixels_ = frame.pixels;
                captureSelectionMask(context, canvasWidth, canvasHeight, sourceSelectionMask_);
                sourceBounds_ = currentBounds;
                lastCommittedBounds_ = currentBounds;
                sourceCacheValid_ = true;
            }

            state_.mode = InteractionState::Mode::Resizing;
            state_.activeHandle = hoveredHandle;
            state_.startMouseX = mousePixelX;
            state_.startMouseY = mousePixelY;
            state_.initialBounds = currentBounds;
            state_.previewBounds = currentBounds;
            state_.previewBoundsValid = true;
            state_.previewFlipX = false;
            state_.previewFlipY = false;
        }
        else if (hoveredOnImage && insideSelection)
        {
            /**
             * 关键修复：
             * - 平移必须严格基于“当前帧 + 当前选区”重建缓存；
             * - 不能复用缩放阶段留下的历史缓存，否则会把旧选区像素投影到新位置，
             *   导致你看到的“像素莫名移动/跑出框选区域”。
             */
            sourceFramePixels_ = frame.pixels;
            captureSelectionMask(context, canvasWidth, canvasHeight, sourceSelectionMask_);
            sourceBounds_ = currentBounds;
            lastCommittedBounds_ = currentBounds;
            sourceCacheValid_ = true;

            state_.mode = InteractionState::Mode::Moving;
            state_.startMouseX = mousePixelX;
            state_.startMouseY = mousePixelY;
            state_.initialBounds = currentBounds;
            state_.previewBounds = currentBounds;
            state_.previewBoundsValid = true;
            state_.previewFlipX = false;
            state_.previewFlipY = false;
        }
        else if (hoveredOnImage)
        {
            state_.mode = InteractionState::Mode::BoxSelecting;
            state_.dragStartX = mousePixelX;
            state_.dragStartY = mousePixelY;
            state_.removeMode = false;
            state_.previewBounds = rectFromDragPixels(mousePixelX, mousePixelY, mousePixelX, mousePixelY);
            state_.previewBoundsValid = true;
            state_.previewFlipX = false;
            state_.previewFlipY = false;
        }
    }

    if (canvasHitboxHovered && hoveredOnImage && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        state_.mode = InteractionState::Mode::BoxSelecting;
        state_.dragStartX = mousePixelX;
        state_.dragStartY = mousePixelY;
        state_.removeMode = true;
        state_.previewBounds = rectFromDragPixels(mousePixelX, mousePixelY, mousePixelX, mousePixelY);
        state_.previewBoundsValid = true;
        state_.previewFlipX = false;
        state_.previewFlipY = false;
    }

    switch (state_.mode)
    {
    case InteractionState::Mode::BoxSelecting:
    {
        const bool usingRight = state_.removeMode;
        const bool stillDown = ImGui::IsMouseDown(usingRight ? ImGuiMouseButton_Right : ImGuiMouseButton_Left);
        state_.previewBounds = rectFromDragPixels(state_.dragStartX, state_.dragStartY, mousePixelX, mousePixelY);
        state_.previewBoundsValid = true;
        state_.previewFlipX = false;
        state_.previewFlipY = false;

        if (!stillDown)
        {
            const AppContext::PixelSelectionOp op = usingRight
                ? AppContext::PixelSelectionOp::Remove
                : (ImGui::GetIO().KeyCtrl ? AppContext::PixelSelectionOp::Add : AppContext::PixelSelectionOp::Replace);
            context.applyRectPixelSelection(
                state_.dragStartX,
                state_.dragStartY,
                mousePixelX,
                mousePixelY,
                canvasWidth,
                canvasHeight,
                op);

            // 框选改动会改变选区定义，需要失效旧的变换缓存。
            sourceCacheValid_ = false;
            sourceFramePixels_.clear();
            sourceSelectionMask_.clear();
            state_ = {};
        }
        break;
    }
    case InteractionState::Mode::Moving:
    {
        const bool stillDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
        const int dx = mousePixelX - state_.startMouseX;
        const int dy = mousePixelY - state_.startMouseY;
        state_.previewBounds = state_.initialBounds;
        state_.previewBounds.x += dx;
        state_.previewBounds.y += dy;
        clampRectToCanvas(state_.previewBounds, canvasWidth, canvasHeight);
        state_.previewBoundsValid = true;

        // 实时预览：每帧都基于“拖拽开始快照”重算，避免累计误差。
        if (sourceCacheValid_)
        {
            std::vector<uint32_t> previewPixels;
            buildMovedPixelsFromSource(
                sourceFramePixels_,
                previewPixels,
                sourceSelectionMask_,
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
            lastCommittedBounds_ = state_.previewBounds;
            state_ = {};
        }
        break;
    }
    case InteractionState::Mode::Resizing:
    {
        const bool stillDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
        const int dx = mousePixelX - state_.startMouseX;
        const int dy = mousePixelY - state_.startMouseY;
        const ResizeResult resizeResult = buildResizedRect(
            state_.initialBounds,
            state_.activeHandle,
            dx,
            dy,
            canvasWidth,
            canvasHeight,
            ImGui::GetIO().KeyCtrl);
        state_.previewBounds = resizeResult.rect;
        state_.previewBoundsValid = true;
        state_.previewFlipX = resizeResult.flipX;
        state_.previewFlipY = resizeResult.flipY;

        // 实时预览：每帧都基于“拖拽开始快照”做缩放变换，避免反复重采样劣化。
        if (sourceCacheValid_)
        {
            std::vector<uint32_t> previewPixels;
            buildScaledPixelsFromSource(
                sourceFramePixels_,
                previewPixels,
                sourceSelectionMask_,
                canvasWidth,
                canvasHeight,
                sourceBounds_,
                state_.previewBounds,
                state_.previewFlipX,
                state_.previewFlipY);
            if (frame.pixels != previewPixels)
            {
                frame.pixels.swap(previewPixels);
                outPixelsChanged = true;
            }
        }

        if (!stillDown)
        {
            context.transformPixelSelectionByRect(
                state_.initialBounds,
                state_.previewBounds,
                state_.previewFlipX,
                state_.previewFlipY);
            lastCommittedBounds_ = state_.previewBounds;
            state_ = {};
        }
        break;
    }
    default:
        break;
    }

    // 光标反馈。
    if (state_.mode == InteractionState::Mode::Moving)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    }
    else if (state_.mode == InteractionState::Mode::Resizing || hoveredHandle >= 0)
    {
        switch (hoveredHandle >= 0 ? hoveredHandle : state_.activeHandle)
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
    if (!drawList)
        return;

    AppContext::PixelRect drawBounds;
    bool drawBoundsValid = context.getPixelSelectionBounds(drawBounds);
    ImU32 boundsColor = IM_COL32(255, 210, 80, 255);
    if (state_.previewBoundsValid)
    {
        drawBounds = state_.previewBounds;
        drawBoundsValid = true;
        boundsColor = IM_COL32(80, 220, 255, 255);
    }
    if (!drawBoundsValid)
        return;

    ImVec2 selMin(0.0f, 0.0f);
    ImVec2 selMax(0.0f, 0.0f);
    convertPixelRectToScreen(drawBounds, imagePos, zoom, selMin, selMax);

    drawList->AddRect(selMin, selMax, boundsColor, 0.0f, 0, 1.0f);
    const float segmentLength = std::max(3.0f, std::min(8.0f, static_cast<float>(zoom) * 0.35f));
    const float antsSpeed = 70.0f;
    const float timePhase = static_cast<float>(ImGui::GetTime()) * antsSpeed;
    drawMarchingAntsRect(drawList, selMin, selMax, segmentLength, timePhase);

    /**
     * 8 手柄显示策略：
     * - 仅在“矩形框选工具被选中”时显示；
     * - 弹窗期间不显示；
     * - 交互拖拽进行中（框选/平移/缩放）不显示，避免遮挡编辑反馈。
     */
    const bool showHandles =
        !anyPopupOpen
        && context.getTool() == ToolType::RectSelection
        && state_.mode == InteractionState::Mode::None;

    if (!showHandles)
        return;

    const float handleHalf = std::max(4.0f, std::min(10.0f, static_cast<float>(zoom) * 0.45f));
    const auto handleCenters = getSelectionHandleCenters(drawBounds, imagePos, zoom);
    for (const ImVec2& c : handleCenters)
    {
        drawList->AddRectFilled(ImVec2(c.x - handleHalf, c.y - handleHalf),
                                ImVec2(c.x + handleHalf, c.y + handleHalf),
                                IM_COL32(40, 120, 200, 255));
        drawList->AddRect(ImVec2(c.x - handleHalf, c.y - handleHalf),
                          ImVec2(c.x + handleHalf, c.y + handleHalf),
                          IM_COL32(255, 255, 255, 255));
    }
}

void RectSelectionTool::resetInteractionState()
{
    state_ = {};
    sourceCacheValid_ = false;
    sourceFramePixels_.clear();
    sourceSelectionMask_.clear();
}
