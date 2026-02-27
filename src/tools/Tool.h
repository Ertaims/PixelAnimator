#pragma once

#include "core/AppContext.h"
#include "core/Project.h"

/**
 * @brief 绘图工具统一接口。
 *
 * 各工具接收同一套输入参数：
 * - frame/canvasWidth/canvasHeight：当前要修改的画布帧
 * - x/y：命中的像素坐标
 * - context：编辑器上下文（颜色、笔刷大小、当前工具等）
 * - isMouseClicked：本帧是否是“按下瞬间”，用于只触发一次的工具（如 Fill）
 *
 * @return true 表示画布像素发生了修改，需要标记项目 dirty。
 */
class Tool
{
public:
    virtual ~Tool() = default;

    virtual ToolType type() const = 0;

    virtual bool apply(Project::Frame& frame,
                       int canvasWidth,
                       int canvasHeight,
                       int x,
                       int y,
                       AppContext& context,
                       bool isMouseClicked) const = 0;
};
