#pragma once

class AppContext;

#include <string>

namespace commands
{
    /**
     * @brief 翻转方向。
     *
     * 说明：
     * - Horizontal：水平翻转，左右镜像；
     * - Vertical：垂直翻转，上下镜像。
     */
    enum class FlipDirection
    {
        Horizontal,
        Vertical
    };

    /**
     * @brief Flip 命令：翻转当前帧或时间轴多选帧的像素。
     *
     * 行为规则：
     * - 若时间轴存在多选帧，则翻转所有选中的帧；
     * - 否则只翻转当前帧；
     * - 翻转不改变画布尺寸，因此支持任意宽高画布。
     */
    class FlipCommand
    {
    public:
        static bool execute(AppContext& context,
                            FlipDirection direction,
                            std::string* outError = nullptr);
    };
}
