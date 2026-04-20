#pragma once

#include "core/AppContext.h"
#include "core/Project.h"

/**
 * @brief 魔棒选区工具（独立模块）。
 *
 * 设计目标：
 * - 从“种子像素”出发做连通域拾取；
 * - 生成命中掩码后交给 AppContext 执行 Replace/Add/Remove；
 * - 与 RectSelectionTool 解耦，便于后续扩展容差、8 邻域等参数。
 */
class MagicWandSelectionTool
{
public:
    /**
     * @brief 以种子点为起点执行一次魔棒选区。
     *
     * @param frame 当前帧像素数据
     * @param canvasWidth 画布宽
     * @param canvasHeight 画布高
     * @param seedX 种子像素 X
     * @param seedY 种子像素 Y
     * @param context 编辑器上下文（用于提交选区）
     * @param op 选区布尔运算模式
     * @return true 选区发生变化
     */
    bool applyFromSeed(const Project::Frame& frame,
                       int canvasWidth,
                       int canvasHeight,
                       int seedX,
                       int seedY,
                       AppContext& context,
                       AppContext::PixelSelectionOp op) const;
};
