#pragma once

#include "Tool.h"

class BrushTool final : public Tool
{
public:
    ToolType type() const override { return ToolType::Brush; }

    bool apply(Project::Frame& frame,
               int canvasWidth,
               int canvasHeight,
               int x,
               int y,
               AppContext& context,
               bool isMouseClicked) const override;
};
