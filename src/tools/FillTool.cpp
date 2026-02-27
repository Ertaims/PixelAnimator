#include "tools/FillTool.h"

#include <deque>
#include <utility>

bool FillTool::apply(Project::Frame& frame,
                     int canvasWidth,
                     int canvasHeight,
                     int x,
                     int y,
                     AppContext& context,
                     bool isMouseClicked) const
{
    // Fill 仅在“按下瞬间”触发一次，避免按住鼠标持续重复填充。
    if (!isMouseClicked)
        return false;

    if (x < 0 || y < 0 || x >= canvasWidth || y >= canvasHeight)
        return false;

    const size_t startIndex =
        static_cast<size_t>(y) * static_cast<size_t>(canvasWidth) + static_cast<size_t>(x);

    const uint32_t oldColor = frame.pixels[startIndex];
    const uint32_t newColor = context.getColorRGBA();
    if (oldColor == newColor)
        return false;

    std::deque<std::pair<int, int>> queue;
    queue.emplace_back(x, y);
    frame.pixels[startIndex] = newColor;

    const int dx[4] = {1, -1, 0, 0};
    const int dy[4] = {0, 0, 1, -1};

    while (!queue.empty())
    {
        const auto [cx, cy] = queue.front();
        queue.pop_front();

        for (int i = 0; i < 4; ++i)
        {
            const int nx = cx + dx[i];
            const int ny = cy + dy[i];
            if (nx < 0 || ny < 0 || nx >= canvasWidth || ny >= canvasHeight)
                continue;

            const size_t index =
                static_cast<size_t>(ny) * static_cast<size_t>(canvasWidth) + static_cast<size_t>(nx);
            if (frame.pixels[index] != oldColor)
                continue;

            frame.pixels[index] = newColor;
            queue.emplace_back(nx, ny);
        }
    }

    return true;
}
