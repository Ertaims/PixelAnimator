#pragma once

#include <cstdint>
#include <string>
#include <vector>

class AppContext;

namespace commands
{
    /**
     * @brief 像素剪贴板数据（应用内共享）。
     *
     * 说明：
     * - width/height：剪贴板区域尺寸（来自源选区外接矩形）；
     * - pixels：RGBA8888 像素数据；
     * - mask：1 表示该局部像素属于选区并有效，0 表示无效（透明占位）。
     */
    struct PixelClipboardData
    {
        int width = 0;
        int height = 0;
        std::vector<uint32_t> pixels;
        std::vector<uint8_t> mask;

        bool isValid() const;
        void clear();
    };

    /**
     * @brief Copy 命令：把当前帧选区内容复制到像素剪贴板。
     */
    class CopySelectionCommand
    {
    public:
        static bool execute(AppContext& context, PixelClipboardData& clipboard, std::string* outError = nullptr);
    };

    /**
     * @brief Cut 命令：先复制选区，再将选区像素清空为透明。
     */
    class CutSelectionCommand
    {
    public:
        static bool execute(AppContext& context, PixelClipboardData& clipboard, std::string* outError = nullptr);
    };

    /**
     * @brief Paste 命令：把剪贴板内容粘贴到“当前选区”范围内。
     *
     * 粘贴定位规则：
     * - 以当前选区外接矩形左上角作为目标原点；
     * - 仅对当前选区内像素写入，选区外不会被修改。
     */
    class PasteSelectionCommand
    {
    public:
        static bool execute(AppContext& context,
                            const PixelClipboardData& clipboard,
                            std::string* outError = nullptr,
                            int customOriginX = -1,
                            int customOriginY = -1);
    };
} // namespace commands
