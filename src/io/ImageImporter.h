#pragma once

#include <cstdint>
#include <string>
#include <vector>

class Project;

/**
 * @brief 图片导入器（PNG）。
 *
 * 设计目标：
 * 1. 仅负责“从 PNG 读取像素并写入 Project”，不处理 UI；
 * 2. 提供单帧导入与精灵图导入两类入口；
 * 3. 错误信息通过 errorMessage 向上层返回，便于 App 统一弹窗。
 */
class ImageImporter
{
public:
    /**
     * @brief 精灵图切片结果。
     */
    struct SpriteSheetSliceResult
    {
        int sheetWidth = 0;   // 原图宽度
        int sheetHeight = 0;  // 原图高度
        int columns = 0;      // 切片列数
        int rows = 0;         // 切片行数
        std::vector<std::vector<uint32_t>> frames; // 切片后的帧像素（顺序受遍历方式影响）
        std::vector<int> tileRows; // 与 frames 等长：每帧来自第几行切片
        std::vector<int> tileCols; // 与 frames 等长：每帧来自第几列切片
    };

    /**
     * @brief 导入单帧 PNG 到指定帧。
     *
     * 约束：
     * - 图片尺寸必须与项目画布尺寸一致；
     * - frameIndex 必须是合法帧索引。
     */
    static bool importSingleFramePng(Project& project,
                                     int frameIndex,
                                     const std::string& path,
                                     std::string* errorMessage);

    /**
     * @brief 从精灵图 PNG 切片并导入为多帧。
     *
     * 切片规则：
     * - 每个切片大小固定为项目画布尺寸（project.getWidth()/getHeight()）；
     * - 源图宽高必须分别能被切片宽高整除；
     * - 导入帧插入到 insertAfterFrameIndex 之后（按遍历顺序逐帧插入）。
     *
     * @param project 项目对象（会被修改）。
     * @param insertAfterFrameIndex 新帧插入位置（插入到该索引之后）。
     * @param path 图片路径（建议 .png）。
     * @param rowMajorTraversal true=按行遍历切片，false=按列遍历切片。
     * @param outImportedFrameCount 成功时返回实际导入帧数量（可为 nullptr）。
     * @param errorMessage 失败时返回错误文本（可为 nullptr）。
     */
    static bool importSpriteSheetPng(Project& project,
                                     int insertAfterFrameIndex,
                                     const std::string& path,
                                     bool rowMajorTraversal,
                                     int* outImportedFrameCount,
                                     std::string* errorMessage);

    /**
     * @brief 只执行“切片解析”，不直接写入 Project。
     *
     * @param path 图片路径。
     * @param frameWidth 切片宽度。
     * @param frameHeight 切片高度。
     * @param rowMajorTraversal true=按行遍历切片，false=按列遍历切片。
     * @param outResult 输出切片结果。
     * @param errorMessage 失败时错误信息。
     */
    static bool sliceSpriteSheetPng(const std::string& path,
                                    int frameWidth,
                                    int frameHeight,
                                    bool rowMajorTraversal,
                                    SpriteSheetSliceResult& outResult,
                                    std::string* errorMessage);
};
