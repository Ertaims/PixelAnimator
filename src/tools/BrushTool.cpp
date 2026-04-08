#include "tools/BrushTool.h"

#include <algorithm>
#include <cstdint>

bool BrushTool::apply(Project::Frame& frame,
                      int canvasWidth,
                      int canvasHeight,
                      int x,
                      int y,
                      AppContext& context,
                      bool isMouseClicked) const
{
    (void)isMouseClicked;

    const uint32_t color = context.getColorRGBA();
    const int radius = std::max(0, context.getBrushSize() - 1);
    const int minX = std::max(0, x - radius);
    const int maxX = std::min(canvasWidth - 1, x + radius);
    const int minY = std::max(0, y - radius);
    const int maxY = std::min(canvasHeight - 1, y + radius);

    bool changed = false;
    for (int py = minY; py <= maxY; ++py)
    {
        const size_t row = static_cast<size_t>(py) * static_cast<size_t>(canvasWidth);
        for (int px = minX; px <= maxX; ++px)
        {
            // 若当前存在选区，则只允许修改选区内像素；
            // 若不存在选区，canEditPixel(...) 会自动放行全部像素。
            if (!context.canEditPixel(px, py, canvasWidth, canvasHeight)) continue;

            uint32_t& pixel = frame.pixels[row + static_cast<size_t>(px)];
            if (pixel != color)
            {
                pixel = color;
                changed = true;
            }
        }
    }
    return changed;
}
