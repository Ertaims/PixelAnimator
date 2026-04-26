#include "tools/RectSelectionTool.h"

#include "core/Project.h"

#include <algorithm>
#include <array>
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

    int roundHalfUp(double value)
    {
        return static_cast<int>(std::floor(value + 0.5));
    }

    /**
     * @brief 将画布像素矩形转换为屏幕坐标矩形。
     *
     * @param rect 像素坐标系矩形
     * @param imagePos 画布图像左上角屏幕坐标
     * @param zoom 当前缩放倍率
     * @param outMin 输出屏幕矩形左上角
     * @param outMax 输出屏幕矩形右下角
     */
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
    // 为了保证贴边选区也容易抓到手柄，中心点会按手柄尺寸向选区内部轻微内收，
    // 避免手柄有一半落在画布外而导致命中困难。
    std::array<ImVec2, 8> getSelectionHandleCenters(const AppContext::PixelRect& rect,
                                                    const ImVec2& imagePos,
                                                    int zoom,
                                                    float handleHalfSize)
    {
        ImVec2 minP(0.0f, 0.0f);
        ImVec2 maxP(0.0f, 0.0f);
        convertPixelRectToScreen(rect, imagePos, zoom, minP, maxP);

        const float screenWidth = std::max(0.0f, maxP.x - minP.x);
        const float screenHeight = std::max(0.0f, maxP.y - minP.y);
        const float insetX = std::min(handleHalfSize, screenWidth * 0.5f);
        const float insetY = std::min(handleHalfSize, screenHeight * 0.5f);

        const float leftX = minP.x + insetX;
        const float rightX = maxP.x - insetX;
        const float topY = minP.y + insetY;
        const float bottomY = maxP.y - insetY;
        const float midX = (leftX + rightX) * 0.5f;
        const float midY = (topY + bottomY) * 0.5f;
        return {{
            ImVec2(leftX, topY),
            ImVec2(midX, topY),
            ImVec2(rightX, topY),
            ImVec2(rightX, midY),
            ImVec2(rightX, bottomY),
            ImVec2(midX, bottomY),
            ImVec2(leftX, bottomY),
            ImVec2(leftX, midY)
        }};
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
        const auto centers = getSelectionHandleCenters(rect, imagePos, zoom, handleHalfSize);
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

    // 缩放结果：包含标准化后的目标矩形，以及是否发生轴向翻转。
    struct ResizeResult
    {
        AppContext::PixelRect rect;
        bool flipX = false;
        bool flipY = false;
    };

    /**
     * @brief 根据手柄拖拽计算缩放后的目标矩形。
     *
     * @param initial 拖拽开始时的初始选区矩形
     * @param handle 当前激活手柄编号（0~7）
     * @param deltaX 鼠标在像素坐标中的水平位移
     * @param deltaY 鼠标在像素坐标中的垂直位移
     * @param canvasWidth 画布像素宽度
     * @param canvasHeight 画布像素高度
     * @param keepAspect 是否保持等比例缩放（Ctrl）
     * @return ResizeResult 目标矩形以及 X/Y 轴翻转标记
     */
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

        if (moveLeft) left += deltaX;
        if (moveRight) right += deltaX;
        if (moveTop) top += deltaY;
        if (moveBottom) bottom += deltaY;

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

            if (moveLeft && !moveRight) left = right - newW + 1;
            else
                right = left + ((right >= left) ? (newW - 1) : -(newW - 1));

            if (moveTop && !moveBottom) top = bottom - newH + 1;
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
        if (!drawList) return;
        const float width = maxP.x - minP.x;
        const float height = maxP.y - minP.y;
        if (width <= 0.0f || height <= 0.0f || segmentLength <= 0.0f) return;

        const ImU32 whiteColor = IM_COL32(255, 255, 255, 255);
        const ImU32 blackColor = IM_COL32(20, 20, 20, 255);
        const float patternLength = segmentLength * 2.0f;
        const float phase = std::fmod(timePhase, patternLength);

        auto drawEdge = [&](const ImVec2& p0, const ImVec2& p1, float edgeLen, float offsetBase) {
            if (edgeLen <= 0.0f) return;

            const ImVec2 dir((p1.x - p0.x) / edgeLen, (p1.y - p0.y) / edgeLen);
            for (float s = -phase; s < edgeLen; s += segmentLength)
            {
                const float start = std::max(0.0f, s);
                const float end = std::min(edgeLen, s + segmentLength);
                if (end <= start) continue;

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

        // 先清空源选区像素。
        for (int y = 0; y < canvasHeight; ++y)
        {
            const size_t rowOffset = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth);
            for (int x = 0; x < canvasWidth; ++x)
            {
                const size_t idx = rowOffset + static_cast<size_t>(x);
                if (idx >= sourceMask.size() || sourceMask[idx] == 0) continue;
                outPixels[idx] = 0x00000000u;
            }
        }

        // 再把源选区像素搬运到目标位置。
        for (int y = 0; y < canvasHeight; ++y)
        {
            const size_t rowOffset = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth);
            for (int x = 0; x < canvasWidth; ++x)
            {
                const size_t srcIdx = rowOffset + static_cast<size_t>(x);
                if (srcIdx >= sourceMask.size() || sourceMask[srcIdx] == 0) continue;

                const int nx = x + dx;
                const int ny = y + dy;
                if (nx < 0 || ny < 0 || nx >= canvasWidth || ny >= canvasHeight) continue;

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
        if (fromRect.width <= 0 || fromRect.height <= 0 || toRect.width <= 0 || toRect.height <= 0) return false;
        outPixels = sourcePixels;

        // 清空源选区像素。
        for (int y = 0; y < canvasHeight; ++y)
        {
            const size_t rowOffset = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth);
            for (int x = 0; x < canvasWidth; ++x)
            {
                const size_t idx = rowOffset + static_cast<size_t>(x);
                if (idx >= sourceMask.size() || sourceMask[idx] == 0) continue;
                outPixels[idx] = 0x00000000u;
            }
        }

        // 目标像素反向映射到源矩形。
        for (int dy = toRect.y; dy < toRect.y + toRect.height; ++dy)
        {
            for (int dx = toRect.x; dx < toRect.x + toRect.width; ++dx)
            {
                if (dx < 0 || dy < 0 || dx >= canvasWidth || dy >= canvasHeight) continue;

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
                if (sx < 0 || sy < 0 || sx >= canvasWidth || sy >= canvasHeight) continue;

                const size_t srcIdx = static_cast<size_t>(sy) * static_cast<size_t>(canvasWidth) + static_cast<size_t>(sx);
                if (srcIdx >= sourceMask.size() || sourceMask[srcIdx] == 0) continue;
                const size_t dstIdx = static_cast<size_t>(dy) * static_cast<size_t>(canvasWidth) + static_cast<size_t>(dx);
                outPixels[dstIdx] = sourcePixels[srcIdx];
            }
        }

        return outPixels != sourcePixels;
    }

    bool buildMovedMaskFromSource(const std::vector<uint8_t>& sourceMask,
                                  std::vector<uint8_t>& outMask,
                                  int canvasWidth,
                                  int canvasHeight,
                                  int dx,
                                  int dy)
    {
        outMask.assign(static_cast<size_t>(canvasWidth) * static_cast<size_t>(canvasHeight), static_cast<uint8_t>(0));
        if (sourceMask.size() != outMask.size()) return false;

        for (int y = 0; y < canvasHeight; ++y)
        {
            const size_t rowOffset = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth);
            for (int x = 0; x < canvasWidth; ++x)
            {
                const size_t srcIdx = rowOffset + static_cast<size_t>(x);
                if (sourceMask[srcIdx] == 0) continue;

                const int nx = x + dx;
                const int ny = y + dy;
                if (nx < 0 || ny < 0 || nx >= canvasWidth || ny >= canvasHeight) continue;

                const size_t dstIdx = static_cast<size_t>(ny) * static_cast<size_t>(canvasWidth) + static_cast<size_t>(nx);
                outMask[dstIdx] = 1;
            }
        }
        return true;
    }

    bool buildScaledMaskFromSource(const std::vector<uint8_t>& sourceMask,
                                   std::vector<uint8_t>& outMask,
                                   int canvasWidth,
                                   int canvasHeight,
                                   const AppContext::PixelRect& fromRect,
                                   const AppContext::PixelRect& toRect,
                                   bool flipX,
                                   bool flipY)
    {
        outMask.assign(static_cast<size_t>(canvasWidth) * static_cast<size_t>(canvasHeight), static_cast<uint8_t>(0));
        if (sourceMask.size() != outMask.size()) return false;
        if (fromRect.width <= 0 || fromRect.height <= 0 || toRect.width <= 0 || toRect.height <= 0) return false;

        for (int dy = toRect.y; dy < toRect.y + toRect.height; ++dy)
        {
            for (int dx = toRect.x; dx < toRect.x + toRect.width; ++dx)
            {
                if (dx < 0 || dy < 0 || dx >= canvasWidth || dy >= canvasHeight) continue;

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
                if (sx < 0 || sy < 0 || sx >= canvasWidth || sy >= canvasHeight) continue;

                const size_t srcIdx = static_cast<size_t>(sy) * static_cast<size_t>(canvasWidth) + static_cast<size_t>(sx);
                if (sourceMask[srcIdx] == 0) continue;

                const size_t dstIdx = static_cast<size_t>(dy) * static_cast<size_t>(canvasWidth) + static_cast<size_t>(dx);
                outMask[dstIdx] = 1;
            }
        }
        return true;
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

    bool maskHasAnySelected(const std::vector<uint8_t>& mask)
    {
        for (const uint8_t v : mask)
        {
            if (v != 0) return true;
        }
        return false;
    }

    void applyRectSelectionOpToMask(std::vector<uint8_t>& ioMask,
                                    int canvasWidth,
                                    int canvasHeight,
                                    const AppContext::PixelRect& rect,
                                    AppContext::PixelSelectionOp op)
    {
        if (ioMask.size() != static_cast<size_t>(canvasWidth) * static_cast<size_t>(canvasHeight)) return;
        if (canvasWidth <= 0 || canvasHeight <= 0 || rect.width <= 0 || rect.height <= 0) return;

        AppContext::PixelRect clamped = rect;
        clampRectToCanvas(clamped, canvasWidth, canvasHeight);

        if (op == AppContext::PixelSelectionOp::Replace) std::fill(ioMask.begin(), ioMask.end(), static_cast<uint8_t>(0));

        for (int y = clamped.y; y < clamped.y + clamped.height; ++y)
        {
            const size_t rowOffset = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth);
            for (int x = clamped.x; x < clamped.x + clamped.width; ++x)
            {
                const size_t idx = rowOffset + static_cast<size_t>(x);
                if (op == AppContext::PixelSelectionOp::Remove) ioMask[idx] = 0;
                else
                    ioMask[idx] = 1;
            }
        }
    }

    // 将椭圆区域应用到选区掩码。
    void applyEllipseSelectionOpToMask(std::vector<uint8_t>& ioMask,
                                       int canvasWidth,
                                       int canvasHeight,
                                       const AppContext::PixelRect& rect,
                                       AppContext::PixelSelectionOp op)
    {
        if (ioMask.size() != static_cast<size_t>(canvasWidth) * static_cast<size_t>(canvasHeight)) return;
        if (canvasWidth <= 0 || canvasHeight <= 0 || rect.width <= 0 || rect.height <= 0) return;

        AppContext::PixelRect clamped = rect;
        clampRectToCanvas(clamped, canvasWidth, canvasHeight);

        if (op == AppContext::PixelSelectionOp::Replace)
        {
            std::fill(ioMask.begin(), ioMask.end(), static_cast<uint8_t>(0));
        }

        const int minX = rect.x;
        const int minY = rect.y;
        const int maxX = rect.x + rect.width - 1;
        const int maxY = rect.y + rect.height - 1;

        if (rect.width == 1)
        {
            for (int y = clamped.y; y < clamped.y + clamped.height; ++y)
            {
                const size_t idx = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth)
                    + static_cast<size_t>(clamped.x);
                if (op == AppContext::PixelSelectionOp::Remove) ioMask[idx] = 0;
                else ioMask[idx] = 1;
            }
            return;
        }

        if (rect.height == 1)
        {
            const size_t rowOffset = static_cast<size_t>(clamped.y) * static_cast<size_t>(canvasWidth);
            for (int x = clamped.x; x < clamped.x + clamped.width; ++x)
            {
                const size_t idx = rowOffset + static_cast<size_t>(x);
                if (op == AppContext::PixelSelectionOp::Remove) ioMask[idx] = 0;
                else ioMask[idx] = 1;
            }
            return;
        }

        const double centerX = (static_cast<double>(minX) + static_cast<double>(maxX)) * 0.5;
        const double centerY = (static_cast<double>(minY) + static_cast<double>(maxY)) * 0.5;
        const double radiusInset = 0.25;
        const double radiusX = std::max(0.5, static_cast<double>(rect.width - 1) * 0.5 - radiusInset);
        const double radiusY = std::max(0.5, static_cast<double>(rect.height - 1) * 0.5 - radiusInset);

        std::vector<int> rowLeft(static_cast<size_t>(clamped.height), canvasWidth);
        std::vector<int> rowRight(static_cast<size_t>(clamped.height), -1);

        auto markBoundaryPoint = [&](int x, int y)
        {
            if (y < clamped.y || y >= clamped.y + clamped.height) return;

            const size_t rowIndex = static_cast<size_t>(y - clamped.y);
            rowLeft[rowIndex] = std::min(rowLeft[rowIndex], x);
            rowRight[rowIndex] = std::max(rowRight[rowIndex], x);
        };

        for (int x = minX; x <= maxX; ++x)
        {
            const double normalizedX = (static_cast<double>(x) - centerX) / radiusX;
            if (std::abs(normalizedX) > 1.0) continue;

            const double yOffset = radiusY * std::sqrt(std::max(0.0, 1.0 - normalizedX * normalizedX));
            const int topY = roundHalfUp(centerY - yOffset);
            const int bottomY = minY + maxY - topY;
            markBoundaryPoint(x, topY);
            markBoundaryPoint(x, bottomY);
        }

        for (int y = minY; y <= maxY; ++y)
        {
            const double normalizedY = (static_cast<double>(y) - centerY) / radiusY;
            if (std::abs(normalizedY) > 1.0) continue;

            const double xOffset = radiusX * std::sqrt(std::max(0.0, 1.0 - normalizedY * normalizedY));
            const int leftX = roundHalfUp(centerX - xOffset);
            const int rightX = minX + maxX - leftX;
            markBoundaryPoint(leftX, y);
            markBoundaryPoint(rightX, y);
        }

        for (int y = clamped.y; y < clamped.y + clamped.height; ++y)
        {
            const size_t rowIndex = static_cast<size_t>(y - clamped.y);
            if (rowRight[rowIndex] < rowLeft[rowIndex]) continue;

            const int fillStartX = std::max(clamped.x, rowLeft[rowIndex]);
            const int fillEndX = std::min(clamped.x + clamped.width - 1, rowRight[rowIndex]);
            if (fillEndX < fillStartX) continue;

            const size_t rowOffset = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth);
            for (int x = fillStartX; x <= fillEndX; ++x)
            {
                const size_t idx = rowOffset + static_cast<size_t>(x);
                if (op == AppContext::PixelSelectionOp::Remove) ioMask[idx] = 0;
                else ioMask[idx] = 1;
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
            applyEllipseSelectionOpToMask(ioMask, canvasWidth, canvasHeight, rect, op);
            return;
        }
        if (shape == RectSelectionTool::SelectionShape::MagicWand)
        {
            // 魔棒是“点击连通域”模式，不走拖拽矩形预览。
            // 这里保持原掩码不变，避免在框选预览分支误绘制矩形。
            return;
        }
        applyRectSelectionOpToMask(ioMask, canvasWidth, canvasHeight, rect, op);
    }

    // 将“任意掩码”按 Replace/Add/Remove 应用到目标掩码。
    void applyArbitraryMaskOpToMask(std::vector<uint8_t>& ioMask,
                                    const std::vector<uint8_t>& applyMask,
                                    AppContext::PixelSelectionOp op)
    {
        if (ioMask.size() != applyMask.size()) return;
        if (op == AppContext::PixelSelectionOp::Replace) std::fill(ioMask.begin(), ioMask.end(), static_cast<uint8_t>(0));

        for (size_t i = 0; i < ioMask.size(); ++i)
        {
            if (applyMask[i] == 0) continue;
            if (op == AppContext::PixelSelectionOp::Remove) ioMask[i] = 0;
            else
                ioMask[i] = 1;
        }
    }

    // 给套索路径追加一段整数像素线，避免快速拖拽出现路径断裂。
    void appendLineToLassoPath(std::vector<ImVec2>& path, int x0, int y0, int x1, int y1)
    {
        int dx = std::abs(x1 - x0);
        int sx = (x0 < x1) ? 1 : -1;
        int dy = -std::abs(y1 - y0);
        int sy = (y0 < y1) ? 1 : -1;
        int err = dx + dy;
        int x = x0;
        int y = y0;

        while (true)
        {
            if (path.empty()
                || static_cast<int>(path.back().x) != x
                || static_cast<int>(path.back().y) != y)
            {
                path.emplace_back(static_cast<float>(x), static_cast<float>(y));
            }
            if (x == x1 && y == y1) break;
            const int e2 = err * 2;
            if (e2 >= dy)
            {
                err += dy;
                x += sx;
            }
            if (e2 <= dx)
            {
                err += dx;
                y += sy;
            }
        }
    }

    // 判断点是否在多边形内部（奇偶规则）。
    bool isPointInsidePolygonEvenOdd(const std::vector<ImVec2>& polygon, float px, float py)
    {
        if (polygon.size() < 3) return false;
        bool inside = false;
        size_t j = polygon.size() - 1;
        for (size_t i = 0; i < polygon.size(); ++i)
        {
            const float xi = polygon[i].x + 0.5f;
            const float yi = polygon[i].y + 0.5f;
            const float xj = polygon[j].x + 0.5f;
            const float yj = polygon[j].y + 0.5f;

            const bool intersect = ((yi > py) != (yj > py))
                && (px < (xj - xi) * (py - yi) / ((yj - yi) + 1e-6f) + xi);
            if (intersect) inside = !inside;
            j = i;
        }
        return inside;
    }

    // 根据套索路径生成闭合区域掩码。
    bool buildLassoMaskFromPath(const std::vector<ImVec2>& inputPath,
                                int canvasWidth,
                                int canvasHeight,
                                std::vector<uint8_t>& outMask)
    {
        outMask.assign(static_cast<size_t>(canvasWidth) * static_cast<size_t>(canvasHeight), static_cast<uint8_t>(0));
        if (canvasWidth <= 0 || canvasHeight <= 0 || inputPath.size() < 2) return false;

        // 拷贝一份路径，并保证首尾闭合。
        std::vector<ImVec2> polygon = inputPath;
        const int firstX = static_cast<int>(polygon.front().x);
        const int firstY = static_cast<int>(polygon.front().y);
        const int lastX = static_cast<int>(polygon.back().x);
        const int lastY = static_cast<int>(polygon.back().y);
        if (firstX != lastX || firstY != lastY) appendLineToLassoPath(polygon, lastX, lastY, firstX, firstY);
        if (polygon.size() < 3) return false;

        int minX = canvasWidth - 1;
        int minY = canvasHeight - 1;
        int maxX = 0;
        int maxY = 0;
        bool hasValid = false;
        for (const ImVec2& p : polygon)
        {
            const int x = std::clamp(static_cast<int>(p.x), 0, canvasWidth - 1);
            const int y = std::clamp(static_cast<int>(p.y), 0, canvasHeight - 1);
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
            hasValid = true;
        }
        if (!hasValid) return false;

        for (int y = minY; y <= maxY; ++y)
        {
            const size_t rowOffset = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth);
            for (int x = minX; x <= maxX; ++x)
            {
                const float cx = static_cast<float>(x) + 0.5f;
                const float cy = static_cast<float>(y) + 0.5f;
                if (isPointInsidePolygonEvenOdd(polygon, cx, cy)) outMask[rowOffset + static_cast<size_t>(x)] = 1;
            }
        }

        // 边界像素也标记为选中，保证轮廓不会出现断裂。
        for (const ImVec2& p : polygon)
        {
            const int x = std::clamp(static_cast<int>(p.x), 0, canvasWidth - 1);
            const int y = std::clamp(static_cast<int>(p.y), 0, canvasHeight - 1);
            outMask[static_cast<size_t>(y) * static_cast<size_t>(canvasWidth) + static_cast<size_t>(x)] = 1;
        }

        return maskHasAnySelected(outMask);
    }

    // 判断当前点击是否足够接近首点，用于“点击起点闭合”。
    bool isCloseToFirstVertex(const std::vector<ImVec2>& vertices, int x, int y, int thresholdPixels)
    {
        if (vertices.empty()) return false;
        const int fx = static_cast<int>(vertices.front().x);
        const int fy = static_cast<int>(vertices.front().y);
        return std::abs(fx - x) <= thresholdPixels && std::abs(fy - y) <= thresholdPixels;
    }

    // 根据多边形顶点构建闭合掩码：先把边离散成像素路径，再复用套索填充逻辑。
    bool buildPolygonMaskFromVertices(const std::vector<ImVec2>& vertices,
                                      int canvasWidth,
                                      int canvasHeight,
                                      std::vector<uint8_t>& outMask)
    {
        if (vertices.size() < 3) return false;

        std::vector<ImVec2> pathPixels;
        pathPixels.reserve(vertices.size() * 4);
        pathPixels.push_back(vertices.front());

        for (size_t i = 1; i < vertices.size(); ++i)
        {
            const int x0 = static_cast<int>(vertices[i - 1].x);
            const int y0 = static_cast<int>(vertices[i - 1].y);
            const int x1 = static_cast<int>(vertices[i].x);
            const int y1 = static_cast<int>(vertices[i].y);
            appendLineToLassoPath(pathPixels, x0, y0, x1, y1);
        }

        const int lx = static_cast<int>(vertices.back().x);
        const int ly = static_cast<int>(vertices.back().y);
        const int fx = static_cast<int>(vertices.front().x);
        const int fy = static_cast<int>(vertices.front().y);
        appendLineToLassoPath(pathPixels, lx, ly, fx, fy);

        return buildLassoMaskFromPath(pathPixels, canvasWidth, canvasHeight, outMask);
    }

    void drawMaskSolidOutline(ImDrawList* drawList,
                              const std::vector<uint8_t>& mask,
                              int canvasWidth,
                              int canvasHeight,
                              const ImVec2& imagePos,
                              int zoom,
                              ImU32 color,
                              float thickness)
    {
        if (!drawList || zoom <= 0) return;
        if (mask.size() != static_cast<size_t>(canvasWidth) * static_cast<size_t>(canvasHeight)) return;

        const auto isSel = [&](int x, int y) -> bool {
            if (x < 0 || y < 0 || x >= canvasWidth || y >= canvasHeight) return false;
            const size_t idx = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth) + static_cast<size_t>(x);
            return mask[idx] != 0;
        };

        for (int y = 0; y < canvasHeight; ++y)
        {
            for (int x = 0; x < canvasWidth; ++x)
            {
                if (!isSel(x, y)) continue;

                const float x0 = imagePos.x + static_cast<float>(x * zoom);
                const float y0 = imagePos.y + static_cast<float>(y * zoom);
                const float x1 = x0 + static_cast<float>(zoom);
                const float y1 = y0 + static_cast<float>(zoom);

                if (!isSel(x, y - 1)) drawList->AddLine(ImVec2(x0, y0), ImVec2(x1, y0), color, thickness);
                if (!isSel(x + 1, y)) drawList->AddLine(ImVec2(x1, y0), ImVec2(x1, y1), color, thickness);
                if (!isSel(x, y + 1)) drawList->AddLine(ImVec2(x0, y1), ImVec2(x1, y1), color, thickness);
                if (!isSel(x - 1, y)) drawList->AddLine(ImVec2(x0, y0), ImVec2(x0, y1), color, thickness);
            }
        }
    }

    void drawMarchingAntsEdge(ImDrawList* drawList,
                              const ImVec2& p0,
                              const ImVec2& p1,
                              float edgeLen,
                              float segmentLength,
                              float timePhase,
                              float offsetBase)
    {
        if (!drawList || edgeLen <= 0.0f || segmentLength <= 0.0f) return;

        const ImU32 whiteColor = IM_COL32(255, 255, 255, 255);
        const ImU32 blackColor = IM_COL32(20, 20, 20, 255);
        const float patternLength = segmentLength * 2.0f;
        const float phase = std::fmod(timePhase, patternLength);
        const ImVec2 dir((p1.x - p0.x) / edgeLen, (p1.y - p0.y) / edgeLen);

        for (float s = -phase; s < edgeLen; s += segmentLength)
        {
            const float start = std::max(0.0f, s);
            const float end = std::min(edgeLen, s + segmentLength);
            if (end <= start) continue;

            const int stripeIndex = static_cast<int>(std::floor((s + phase + offsetBase) / segmentLength));
            const ImU32 col = ((stripeIndex & 1) == 0) ? whiteColor : blackColor;
            const ImVec2 a(p0.x + dir.x * start, p0.y + dir.y * start);
            const ImVec2 b(p0.x + dir.x * end, p0.y + dir.y * end);
            drawList->AddLine(a, b, col, 2.0f);
        }
    }

    void drawMarchingAntsMask(ImDrawList* drawList,
                              const std::vector<uint8_t>& mask,
                              int canvasWidth,
                              int canvasHeight,
                              const ImVec2& imagePos,
                              int zoom,
                              float segmentLength,
                              float timePhase)
    {
        if (!drawList || zoom <= 0) return;
        if (mask.size() != static_cast<size_t>(canvasWidth) * static_cast<size_t>(canvasHeight)) return;

        const auto isSel = [&](int x, int y) -> bool {
            if (x < 0 || y < 0 || x >= canvasWidth || y >= canvasHeight) return false;
            const size_t idx = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth) + static_cast<size_t>(x);
            return mask[idx] != 0;
        };

        const float edgeLen = static_cast<float>(zoom);
        for (int y = 0; y < canvasHeight; ++y)
        {
            for (int x = 0; x < canvasWidth; ++x)
            {
                if (!isSel(x, y)) continue;

                const float x0 = imagePos.x + static_cast<float>(x * zoom);
                const float y0 = imagePos.y + static_cast<float>(y * zoom);
                const float x1 = x0 + static_cast<float>(zoom);
                const float y1 = y0 + static_cast<float>(zoom);
                const float base = static_cast<float>((x + y) * zoom);

                if (!isSel(x, y - 1)) drawMarchingAntsEdge(drawList, ImVec2(x0, y0), ImVec2(x1, y0), edgeLen, segmentLength, timePhase, base);
                if (!isSel(x + 1, y)) drawMarchingAntsEdge(drawList, ImVec2(x1, y0), ImVec2(x1, y1), edgeLen, segmentLength, timePhase, base + edgeLen);
                if (!isSel(x, y + 1)) drawMarchingAntsEdge(drawList, ImVec2(x0, y1), ImVec2(x1, y1), edgeLen, segmentLength, timePhase, base + edgeLen * 2.0f);
                if (!isSel(x - 1, y)) drawMarchingAntsEdge(drawList, ImVec2(x0, y0), ImVec2(x0, y1), edgeLen, segmentLength, timePhase, base + edgeLen * 3.0f);
            }
        }
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
    state_.hoverMouseX = mousePixelX;
    state_.hoverMouseY = mousePixelY;

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
            if (selectionShape_ == SelectionShape::MagicWand)
            {
                const AppContext::PixelSelectionOp op = ImGui::GetIO().KeyCtrl
                    ? AppContext::PixelSelectionOp::Add
                    : AppContext::PixelSelectionOp::Replace;
                if (magicWandTool_.applyFromSeed(
                        frame,
                        canvasWidth,
                        canvasHeight,
                        mousePixelX,
                        mousePixelY,
                        context,
                        op))
                {
                    // 选区定义发生变化，失效变换缓存。
                    sourceCacheValid_ = false;
                    sourceFramePixels_.clear();
                    sourceSelectionMask_.clear();
                }
                state_ = {};
            }
            else if (selectionShape_ == SelectionShape::PolygonLasso)
            {
                const AppContext::PixelSelectionOp clickOp = ImGui::GetIO().KeyCtrl
                    ? AppContext::PixelSelectionOp::Add
                    : AppContext::PixelSelectionOp::Replace;

                if (state_.mode != InteractionState::Mode::PolygonLassoSelecting)
                {
                    state_.mode = InteractionState::Mode::PolygonLassoSelecting;
                    state_.removeMode = false;
                    state_.previewOp = clickOp;
                    state_.previewBoundsValid = false;
                    state_.previewFlipX = false;
                    state_.previewFlipY = false;
                    state_.lassoPathPixels.clear();
                    state_.lassoPathPixels.emplace_back(static_cast<float>(mousePixelX), static_cast<float>(mousePixelY));
                }
                else if (!state_.removeMode)
                {
                    // 点击起点闭合：至少 3 个顶点时生效。
                    if (state_.lassoPathPixels.size() >= 3
                        && isCloseToFirstVertex(state_.lassoPathPixels, mousePixelX, mousePixelY, 1))
                    {
                        std::vector<uint8_t> polygonMask;
                        if (buildPolygonMaskFromVertices(state_.lassoPathPixels, canvasWidth, canvasHeight, polygonMask))
                        {
                            context.applyMaskPixelSelection(polygonMask, canvasWidth, canvasHeight, state_.previewOp);
                            sourceCacheValid_ = false;
                            sourceFramePixels_.clear();
                            sourceSelectionMask_.clear();
                        }
                        state_ = {};
                    }
                    else
                    {
                        const int lastX = static_cast<int>(state_.lassoPathPixels.back().x);
                        const int lastY = static_cast<int>(state_.lassoPathPixels.back().y);
                        if (lastX != mousePixelX || lastY != mousePixelY)
                            state_.lassoPathPixels.emplace_back(static_cast<float>(mousePixelX), static_cast<float>(mousePixelY));
                    }
                }
            }
            else if (selectionShape_ == SelectionShape::Lasso)
            {
                state_.mode = InteractionState::Mode::LassoSelecting;
                state_.removeMode = false;
                state_.previewOp = ImGui::GetIO().KeyCtrl
                    ? AppContext::PixelSelectionOp::Add
                    : AppContext::PixelSelectionOp::Replace;
                state_.previewBoundsValid = false;
                state_.previewFlipX = false;
                state_.previewFlipY = false;
                state_.lassoPathPixels.clear();
                state_.lassoPathPixels.emplace_back(static_cast<float>(mousePixelX), static_cast<float>(mousePixelY));
            }
            else
            {
            state_.mode = InteractionState::Mode::BoxSelecting;
            state_.dragStartX = mousePixelX;
            state_.dragStartY = mousePixelY;
            state_.removeMode = false;
            // 左键框选：按下瞬间决定是 Replace 还是 Add，拖拽过程不再抖动。
            state_.previewOp = ImGui::GetIO().KeyCtrl
                ? AppContext::PixelSelectionOp::Add
                : AppContext::PixelSelectionOp::Replace;
            state_.previewBounds = rectFromDragPixels(mousePixelX, mousePixelY, mousePixelX, mousePixelY);
            state_.previewBoundsValid = true;
            state_.previewFlipX = false;
            state_.previewFlipY = false;
            }
        }
    }

    if (canvasHitboxHovered && hoveredOnImage && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        if (selectionShape_ == SelectionShape::MagicWand)
        {
            if (magicWandTool_.applyFromSeed(
                    frame,
                    canvasWidth,
                    canvasHeight,
                    mousePixelX,
                    mousePixelY,
                    context,
                    AppContext::PixelSelectionOp::Remove))
            {
                sourceCacheValid_ = false;
                sourceFramePixels_.clear();
                sourceSelectionMask_.clear();
            }
            state_ = {};
        }
        else if (selectionShape_ == SelectionShape::PolygonLasso)
        {
            if (state_.mode != InteractionState::Mode::PolygonLassoSelecting)
            {
                state_.mode = InteractionState::Mode::PolygonLassoSelecting;
                state_.removeMode = true;
                state_.previewOp = AppContext::PixelSelectionOp::Remove;
                state_.previewBoundsValid = false;
                state_.previewFlipX = false;
                state_.previewFlipY = false;
                state_.lassoPathPixels.clear();
                state_.lassoPathPixels.emplace_back(static_cast<float>(mousePixelX), static_cast<float>(mousePixelY));
            }
            else if (state_.removeMode)
            {
                if (state_.lassoPathPixels.size() >= 3
                    && isCloseToFirstVertex(state_.lassoPathPixels, mousePixelX, mousePixelY, 1))
                {
                    std::vector<uint8_t> polygonMask;
                    if (buildPolygonMaskFromVertices(state_.lassoPathPixels, canvasWidth, canvasHeight, polygonMask))
                    {
                        context.applyMaskPixelSelection(polygonMask, canvasWidth, canvasHeight, state_.previewOp);
                        sourceCacheValid_ = false;
                        sourceFramePixels_.clear();
                        sourceSelectionMask_.clear();
                    }
                    state_ = {};
                }
                else
                {
                    const int lastX = static_cast<int>(state_.lassoPathPixels.back().x);
                    const int lastY = static_cast<int>(state_.lassoPathPixels.back().y);
                    if (lastX != mousePixelX || lastY != mousePixelY)
                        state_.lassoPathPixels.emplace_back(static_cast<float>(mousePixelX), static_cast<float>(mousePixelY));
                }
            }
            else
            {
                // 左键进行中的多边形套索遇到右键时直接取消，避免操作歧义。
                state_ = {};
            }
        }
        else if (selectionShape_ == SelectionShape::Lasso)
        {
            state_.mode = InteractionState::Mode::LassoSelecting;
            state_.removeMode = true;
            state_.previewOp = AppContext::PixelSelectionOp::Remove;
            state_.previewBoundsValid = false;
            state_.previewFlipX = false;
            state_.previewFlipY = false;
            state_.lassoPathPixels.clear();
            state_.lassoPathPixels.emplace_back(static_cast<float>(mousePixelX), static_cast<float>(mousePixelY));
        }
        else
        {
            state_.mode = InteractionState::Mode::BoxSelecting;
            state_.dragStartX = mousePixelX;
            state_.dragStartY = mousePixelY;
            state_.removeMode = true;
            state_.previewOp = AppContext::PixelSelectionOp::Remove;
            state_.previewBounds = rectFromDragPixels(mousePixelX, mousePixelY, mousePixelX, mousePixelY);
            state_.previewBoundsValid = true;
            state_.previewFlipX = false;
            state_.previewFlipY = false;
        }
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
                : state_.previewOp;
            if (selectionShape_ == SelectionShape::Ellipse)
            {
                context.applyEllipsePixelSelection(
                    state_.dragStartX,
                    state_.dragStartY,
                    mousePixelX,
                    mousePixelY,
                    canvasWidth,
                    canvasHeight,
                    op);
            }
            else
            {
                context.applyRectPixelSelection(
                    state_.dragStartX,
                    state_.dragStartY,
                    mousePixelX,
                    mousePixelY,
                    canvasWidth,
                    canvasHeight,
                    op);
            }

            // 框选改动会改变选区定义，需要失效旧的变换缓存。
            sourceCacheValid_ = false;
            sourceFramePixels_.clear();
            sourceSelectionMask_.clear();
            state_ = {};
        }
        break;
    }
    case InteractionState::Mode::LassoSelecting:
    {
        const bool usingRight = state_.removeMode;
        const bool stillDown = ImGui::IsMouseDown(usingRight ? ImGuiMouseButton_Right : ImGuiMouseButton_Left);
        if (!state_.lassoPathPixels.empty())
        {
            const int lastX = static_cast<int>(state_.lassoPathPixels.back().x);
            const int lastY = static_cast<int>(state_.lassoPathPixels.back().y);
            appendLineToLassoPath(state_.lassoPathPixels, lastX, lastY, mousePixelX, mousePixelY);
        }

        if (!stillDown)
        {
            std::vector<uint8_t> lassoMask;
            if (buildLassoMaskFromPath(state_.lassoPathPixels, canvasWidth, canvasHeight, lassoMask))
            {
                context.applyMaskPixelSelection(lassoMask, canvasWidth, canvasHeight, state_.previewOp);
                sourceCacheValid_ = false;
                sourceFramePixels_.clear();
                sourceSelectionMask_.clear();
            }
            state_ = {};
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
    else if (state_.mode == InteractionState::Mode::LassoSelecting)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    else if (state_.mode == InteractionState::Mode::PolygonLassoSelecting)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
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

    if (state_.mode == InteractionState::Mode::BoxSelecting && state_.previewBoundsValid)
    {
        std::vector<uint8_t> previewMask = committedMask;
        applySelectionShapeOpToMask(
            previewMask,
            canvasWidth,
            canvasHeight,
            state_.previewBounds,
            state_.previewOp,
            selectionShape_);
        displayMask.swap(previewMask);

        if (state_.previewOp == AppContext::PixelSelectionOp::Add) displayColor = IM_COL32(90, 230, 140, 255);
        else if (state_.previewOp == AppContext::PixelSelectionOp::Remove)
            displayColor = IM_COL32(255, 120, 120, 255);
        else
            displayColor = IM_COL32(80, 220, 255, 255);

        // 仅在框选阶段保留旧轮廓参考（用户此前明确希望此行为）。
        if (maskHasAnySelected(committedMask)) drawMaskSolidOutline(drawList, committedMask, canvasWidth, canvasHeight, imagePos, zoom, IM_COL32(190, 170, 80, 180), 1.0f);
    }
    else if (state_.mode == InteractionState::Mode::LassoSelecting && state_.lassoPathPixels.size() >= 2)
    {
        std::vector<uint8_t> lassoMask;
        if (buildLassoMaskFromPath(state_.lassoPathPixels, canvasWidth, canvasHeight, lassoMask))
        {
            std::vector<uint8_t> previewMask = committedMask;
            applyArbitraryMaskOpToMask(previewMask, lassoMask, state_.previewOp);
            displayMask.swap(previewMask);

            if (state_.previewOp == AppContext::PixelSelectionOp::Add) displayColor = IM_COL32(90, 230, 140, 255);
            else if (state_.previewOp == AppContext::PixelSelectionOp::Remove)
                displayColor = IM_COL32(255, 120, 120, 255);
            else
                displayColor = IM_COL32(80, 220, 255, 255);

            if (maskHasAnySelected(committedMask)) drawMaskSolidOutline(drawList, committedMask, canvasWidth, canvasHeight, imagePos, zoom, IM_COL32(190, 170, 80, 180), 1.0f);
        }
    }
    else if (state_.mode == InteractionState::Mode::PolygonLassoSelecting && !state_.lassoPathPixels.empty())
    {
        std::vector<ImVec2> previewVertices = state_.lassoPathPixels;
        const int hoverX = state_.hoverMouseX;
        const int hoverY = state_.hoverMouseY;
        const int lastX = static_cast<int>(previewVertices.back().x);
        const int lastY = static_cast<int>(previewVertices.back().y);
        if (lastX != hoverX || lastY != hoverY)
            previewVertices.emplace_back(static_cast<float>(hoverX), static_cast<float>(hoverY));

        if (previewVertices.size() >= 3)
        {
            std::vector<uint8_t> polygonMask;
            if (buildPolygonMaskFromVertices(previewVertices, canvasWidth, canvasHeight, polygonMask))
            {
                std::vector<uint8_t> previewMask = committedMask;
                applyArbitraryMaskOpToMask(previewMask, polygonMask, state_.previewOp);
                displayMask.swap(previewMask);

                if (state_.previewOp == AppContext::PixelSelectionOp::Add) displayColor = IM_COL32(90, 230, 140, 255);
                else if (state_.previewOp == AppContext::PixelSelectionOp::Remove)
                    displayColor = IM_COL32(255, 120, 120, 255);
                else
                    displayColor = IM_COL32(80, 220, 255, 255);

                if (maskHasAnySelected(committedMask)) drawMaskSolidOutline(drawList, committedMask, canvasWidth, canvasHeight, imagePos, zoom, IM_COL32(190, 170, 80, 180), 1.0f);
            }
        }

        // 多边形路径辅助显示：统一使用黑色像素风（线段 + 顶点方点）。
        const ImU32 guideColor = IM_COL32(0, 0, 0, 255);
        for (size_t i = 1; i < previewVertices.size(); ++i)
        {
            const ImVec2 p0(
                imagePos.x + (previewVertices[i - 1].x + 0.5f) * static_cast<float>(zoom),
                imagePos.y + (previewVertices[i - 1].y + 0.5f) * static_cast<float>(zoom));
            const ImVec2 p1(
                imagePos.x + (previewVertices[i].x + 0.5f) * static_cast<float>(zoom),
                imagePos.y + (previewVertices[i].y + 0.5f) * static_cast<float>(zoom));
            drawList->AddLine(p0, p1, guideColor, 1.0f);
        }

        const float vertexHalf = std::max(1.0f, static_cast<float>(zoom) * 0.12f);
        for (const ImVec2& v : state_.lassoPathPixels)
        {
            const ImVec2 c(
                imagePos.x + (v.x + 0.5f) * static_cast<float>(zoom),
                imagePos.y + (v.y + 0.5f) * static_cast<float>(zoom));
            drawList->AddRectFilled(
                ImVec2(c.x - vertexHalf, c.y - vertexHalf),
                ImVec2(c.x + vertexHalf, c.y + vertexHalf),
                IM_COL32(0, 0, 0, 255));
        }
    }
    else if (state_.mode == InteractionState::Mode::Moving && state_.previewBoundsValid && sourceCacheValid_)
    {
        const int dx = state_.previewBounds.x - state_.initialBounds.x;
        const int dy = state_.previewBounds.y - state_.initialBounds.y;
        buildMovedMaskFromSource(sourceSelectionMask_, displayMask, canvasWidth, canvasHeight, dx, dy);
        displayColor = IM_COL32(80, 220, 255, 255);
    }
    else if (state_.mode == InteractionState::Mode::Resizing && state_.previewBoundsValid && sourceCacheValid_)
    {
        buildScaledMaskFromSource(
            sourceSelectionMask_,
            displayMask,
            canvasWidth,
            canvasHeight,
            sourceBounds_,
            state_.previewBounds,
            state_.previewFlipX,
            state_.previewFlipY);
        displayColor = IM_COL32(80, 220, 255, 255);
    }

    if (!maskHasAnySelected(displayMask)) return;

    drawMaskSolidOutline(drawList, displayMask, canvasWidth, canvasHeight, imagePos, zoom, displayColor, 1.2f);
    drawMarchingAntsMask(drawList, displayMask, canvasWidth, canvasHeight, imagePos, zoom, segmentLength, timePhase);

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
        && state_.mode == InteractionState::Mode::None;

    if (!showHandles) return;

    if (!hasCurrentBounds) return;

    const float handleHalf = std::max(4.0f, std::min(10.0f, static_cast<float>(zoom) * 0.45f));
    const auto handleCenters = getSelectionHandleCenters(currentBounds, imagePos, zoom, handleHalf);
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
