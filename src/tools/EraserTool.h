#pragma once

#include "Tool.h"

class EraserTool final : public Tool
{
public:
    ToolType type() const override { return ToolType::Eraser; }

    bool apply(Project::Frame& frame,
               int canvasWidth,
               int canvasHeight,
               int x,
               int y,
               AppContext& context,
               bool isMouseClicked) const override;
};
