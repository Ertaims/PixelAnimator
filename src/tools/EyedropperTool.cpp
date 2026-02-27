#include "tools/EyedropperTool.h"

bool EyedropperTool::apply(Project::Frame& frame,
                           int canvasWidth,
                           int canvasHeight,
                           int x,
                           int y,
                           AppContext& context,
                           bool isMouseClicked) const
{
    (void)canvasHeight;
    (void)isMouseClicked;

    const size_t index = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth) + static_cast<size_t>(x);
    context.setColorRGBA(frame.pixels[index]);
    return false;
}
