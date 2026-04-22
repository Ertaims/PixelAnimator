#pragma once

class AppContext;

#include <string>

namespace commands
{
    /**
     * @brief 旋转角度/方向。
     *
     * 说明：
     * - Clockwise90：顺时针旋转 90 度；
     * - CounterClockwise90：逆时针旋转 90 度；
     * - Rotate180：旋转 180 度。
     */
    enum class RotationAngle
    {
        Clockwise90,
        CounterClockwise90,
        Rotate180
    };

    /**
     * @brief Rotate 命令：旋转当前帧或时间轴多选帧的像素。
     *
     * 行为规则：
     * - 若时间轴存在多选帧，则旋转所有选中的帧；
     * - 否则只旋转当前帧；
     * - 90 度旋转目前要求画布为正方形，因为 Project 的所有帧共享同一画布尺寸；
     * - 180 度旋转不改变宽高，因此支持任意画布尺寸。
     */
    class RotateCommand
    {
    public:
        static bool execute(AppContext& context,
                            RotationAngle angle,
                            std::string* outError = nullptr);
    };
}
