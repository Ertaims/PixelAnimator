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
     * RowColumn: 指定“每行列数”后自动换行（多行多列网格）
     */
    enum class SpriteSheetLayout : int
    {
        Row = 0,
        Column,
        RowColumn
    };

    /**
     * @brief 自定义分组导出项。
     *
     * 用途：
     * - 把一组帧作为一个“组块”导出；
     * - 每组可独立选择行排或列排；
     * - name 当前主要用于报错定位与后续扩展（例如导出元数据）。
     */
    struct SpriteGroup
    {
        std::string name;              // 组名（用于识别和错误信息）
        std::vector<int> frameIndices; // 该组包含的帧索引列表
        SpriteSheetLayout layout = SpriteSheetLayout::Row; // 该组内部布局（行/列）
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
     * @param columnsPerRow 当 layout=RowColumn 时生效，表示每行帧数。
     *                      传入 <=0 时将被修正为 1。
     * @param path         输出路径（建议 .png）。
     * @param errorMessage 失败时写入错误文本（可为 nullptr）。
     * @return true 成功，false 失败。
     */
    static bool exportSpriteSheetPng(const Project& project,
                                     const std::vector<int>& frameIndices,
                                     SpriteSheetLayout layout,
                                     int columnsPerRow,
                                     const std::string& path,
                                     std::string* errorMessage);

    /**
     * @brief 按“自定义分组”导出精灵图 PNG。
     *
     * 布局规则（当前实现）：
     * - 每个组先按组内 layout（Row/Column）生成一个局部块；
     * - 然后把多个组按“纵向堆叠”拼接到同一张图；
     * - 组与组之间使用 groupSpacing 像素间距。
     *
     * @param project      项目对象。
     * @param groups       分组配置列表。
     * @param groupSpacing 组间距（像素，<0 时按 0 处理）。
     * @param path         输出路径（建议 .png）。
     * @param errorMessage 失败时写入错误文本（可为 nullptr）。
     * @return true 成功，false 失败。
     */
    static bool exportGroupedSpriteSheetPng(const Project& project,
                                            const std::vector<SpriteGroup>& groups,
                                            int groupSpacing,
                                            const std::string& path,
                                            std::string* errorMessage);
};
