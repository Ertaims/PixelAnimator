#pragma once

#include <memory>
#include <cstdint>
#include <string>
#include <vector>

class Project;

/**
 * @brief 项目序列化器（.pxanim 二进制）。
 *
 * 设计目标：
 * 1) 结构简单：便于快速实现与调试。
 * 2) 可演进：通过 version 字段支持后续扩展。
 * 3) 无外部依赖：直接使用 C++ 文件流读写二进制。
 *
 * 当前支持版本：
 * - v2：基础头 + 项目名长度 + 项目名字节 + 单图层像素帧数据。
 * - v3：在 v2 基础上加入图层结构、活动图层与多图层像素帧数据。
 *
 * 注意：
 * - 该格式按“宿主机器字节序”直接写入 uint32_t，不是跨平台稳定格式。
 * - 目前面向同平台最小可用，后续若要跨平台，需要固定端序与字段编码。
 */
class ProjectSerializer
{
public:
    /**
     * @brief 随项目文件持久化的时间轴帧分组。
     *
     * Project 本身只保存像素/图层等核心数据，帧分组目前属于 AppContext 状态；
     * 二进制保存时用这个轻量结构在 App 与 Serializer 之间传递分组数据。
     */
    struct FrameGroupInfo
    {
        std::string name;
        std::vector<int> frameIndices;
        uint32_t colorRGBA = 0xFFFFFFFF;
    };

    /**
     * @brief 将 Project 写入指定路径。
     * @param project      待保存的项目对象。
     * @param path         输出文件路径。
     * @param errorMessage 失败时写入错误信息（可为 nullptr）。
     * @return true 保存成功；false 保存失败。
     */
    static bool save(const Project& project, const std::string& path, std::string* errorMessage);

    /**
     * @brief 将 Project 与时间轴帧分组一起写入指定路径。
     */
    static bool save(const Project& project,
                     const std::vector<FrameGroupInfo>& frameGroups,
                     const std::string& path,
                     std::string* errorMessage);

    /**
     * @brief 从指定路径加载 Project。
     * @param path         输入文件路径。
     * @param errorMessage 失败时写入错误信息（可为 nullptr）。
     * @return 成功返回 Project；失败返回 nullptr。
     */
    static std::unique_ptr<Project> load(const std::string& path, std::string* errorMessage);

    /**
     * @brief 从指定路径加载 Project，并可选返回时间轴帧分组。
     */
    static std::unique_ptr<Project> load(const std::string& path,
                                         std::vector<FrameGroupInfo>* outFrameGroups,
                                         std::string* errorMessage);
};
