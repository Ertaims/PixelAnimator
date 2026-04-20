#pragma once

class AppContext;

#include <string>


namespace commands
{
    /**
     * @brief Delete 命令：删除选区内的像素或当前帧。
     *
     * 行为规则：
     * - 如果有像素选区，将选区内的像素清空为透明；
     * - 如果没有选区，删除当前帧；
     * - 如果是多选状态，删除所有选中的帧。
     */
    class DeleteCommand
    {
    public:
        static bool execute(AppContext& context, std::string* outError = nullptr);
    };
}