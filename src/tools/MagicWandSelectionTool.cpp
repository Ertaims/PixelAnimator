#include "tools/MagicWandSelectionTool.h"

#include <cstddef>
#include <vector>

namespace
{
    // 将二维坐标转为线性索引。
    inline size_t toIndex(int x, int y, int width)
    {
        return static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
    }
} // namespace

bool MagicWandSelectionTool::applyFromSeed(const Project::Frame& frame,
                                           int canvasWidth,
                                           int canvasHeight,
                                           int seedX,
                                           int seedY,
                                           AppContext& context,
                                           AppContext::PixelSelectionOp op) const
{
    if (canvasWidth <= 0 || canvasHeight <= 0) return false;
    if (seedX < 0 || seedY < 0 || seedX >= canvasWidth || seedY >= canvasHeight) return false;

    const size_t pixelCount = static_cast<size_t>(canvasWidth) * static_cast<size_t>(canvasHeight);
    if (frame.pixels.size() != pixelCount) return false;

    // 本版本按“像素值完全相等”判断命中（RGBA8888 全通道匹配）。
    const uint32_t targetColor = frame.pixels[toIndex(seedX, seedY, canvasWidth)];
    std::vector<uint8_t> selectionMask(pixelCount, static_cast<uint8_t>(0));
    std::vector<uint8_t> visited(pixelCount, static_cast<uint8_t>(0));

    // 用向量模拟队列，避免引入额外容器依赖。
    std::vector<int> queueX;
    std::vector<int> queueY;
    queueX.reserve(pixelCount);
    queueY.reserve(pixelCount);

    queueX.push_back(seedX);
    queueY.push_back(seedY);
    visited[toIndex(seedX, seedY, canvasWidth)] = 1;

    size_t head = 0;
    while (head < queueX.size())
    {
        const int x = queueX[head];
        const int y = queueY[head];
        ++head;

        const size_t idx = toIndex(x, y, canvasWidth);
        if (frame.pixels[idx] != targetColor) continue;
        selectionMask[idx] = 1;

        // 4 邻域：左、右、上、下。
        const int nx[4] = {x - 1, x + 1, x, x};
        const int ny[4] = {y, y, y - 1, y + 1};
        for (int i = 0; i < 4; ++i)
        {
            const int px = nx[i];
            const int py = ny[i];
            if (px < 0 || py < 0 || px >= canvasWidth || py >= canvasHeight) continue;

            const size_t nextIdx = toIndex(px, py, canvasWidth);
            if (visited[nextIdx] != 0) continue;
            visited[nextIdx] = 1;

            // 只扩展“同色像素”，这样队列规模受控且语义明确。
            if (frame.pixels[nextIdx] != targetColor) continue;
            queueX.push_back(px);
            queueY.push_back(py);
        }
    }

    return context.applyMaskPixelSelection(selectionMask, canvasWidth, canvasHeight, op);
}
