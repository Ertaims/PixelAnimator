#pragma once

#include "Tool.h"

class FillTool final : public Tool
{
public:
    ToolType type() const override { return ToolType::Fill; }

    bool apply(Project::Frame& frame,
               int canvasWidth,
               int canvasHeight,
               int x,
               int y,
               AppContext& context,
               bool isMouseClicked) const override;
};
