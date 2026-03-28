#pragma once

#include <cstdint>
#include <string>
#include <vector>

class Project;

/**
 * @brief 图片导出器（PNG）。
 *
 * 设计目标：
 * 1. 仅负责“把 Project 像素数据导出为 PNG”，不处理 UI。
 * 2. 提供稳定的两类导出入口：单帧导出、精灵图导出。
 * 3. 错误信息统一通过 errorMessage 输出，便于上层弹窗展示。
 */
class ImageExporter
{
public:
    /**
     * @brief 精灵图帧排布方式。
     * Row:    所有帧横向拼接（1 行 N 列）
     * Column: 所有帧纵向拼接（N 行 1 列）
     */
    enum class SpriteSheetLayout : int
    {
        Row = 0,
        Column
    };

    /**
     * @brief 导出单帧 PNG。
     * @param project      项目对象（提供尺寸与像素帧）。
     * @param frameIndex   导出的帧索引。
     * @param path         输出路径（建议 .png）。
     * @param errorMessage 失败时写入错误文本（可为 nullptr）。
     * @return true 成功，false 失败。
     */
    static bool exportSingleFramePng(const Project& project,
                                     int frameIndex,
                                     const std::string& path,
                                     std::string* errorMessage);

    /**
     * @brief 导出精灵图 PNG。
     * @param project      项目对象。
     * @param frameIndices 要导出的帧索引列表。
     *                     传空数组时默认导出“全部帧”。
     * @param layout       行排或列排。
     * @param path         输出路径（建议 .png）。
     * @param errorMessage 失败时写入错误文本（可为 nullptr）。
     * @return true 成功，false 失败。
     */
    static bool exportSpriteSheetPng(const Project& project,
                                     const std::vector<int>& frameIndices,
                                     SpriteSheetLayout layout,
                                     const std::string& path,
                                     std::string* errorMessage);
};
