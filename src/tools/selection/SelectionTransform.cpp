#include "tools/selection/SelectionTransform.h"

#include <algorithm>
#include <cmath>

namespace selection
{
    namespace
    {
        // 将变换后的目标矩形裁剪到画布内，避免预览或提交时越界。
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

        // 剪切式移动/缩放都需要先清空原选区像素，再写入变换后的像素。
        void clearSourceSelectionPixels(std::vector<uint32_t>& ioPixels,
                                        const std::vector<uint8_t>& sourceMask,
                                        int canvasWidth,
                                        int canvasHeight)
        {
            for (int y = 0; y < canvasHeight; ++y)
            {
                const size_t rowOffset = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth);
                for (int x = 0; x < canvasWidth; ++x)
                {
                    const size_t idx = rowOffset + static_cast<size_t>(x);
                    if (idx >= sourceMask.size() || sourceMask[idx] == 0) continue;
                    ioPixels[idx] = 0x00000000u;
                }
            }
        }
    }

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
            else right = left + ((right >= left) ? (newW - 1) : -(newW - 1));

            if (moveTop && !moveBottom) top = bottom - newH + 1;
            else if (moveTop || moveBottom)
            {
                bottom = top + ((bottom >= top) ? (newH - 1) : -(newH - 1));
            }
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

    bool buildMovedPixelsFromSource(const std::vector<uint32_t>& sourcePixels,
                                    std::vector<uint32_t>& outPixels,
                                    const std::vector<uint8_t>& sourceMask,
                                    int canvasWidth,
                                    int canvasHeight,
                                    int dx,
                                    int dy)
    {
        outPixels = sourcePixels;
        clearSourceSelectionPixels(outPixels, sourceMask, canvasWidth, canvasHeight);

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
        clearSourceSelectionPixels(outPixels, sourceMask, canvasWidth, canvasHeight);

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
}
