#pragma once

#include <memory>
#include <string>

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
 * - v2：基础头 + 项目名长度 + 项目名字节 + 像素帧数据。
 *
 * 注意：
 * - 该格式按“宿主机器字节序”直接写入 uint32_t，不是跨平台稳定格式。
 * - 目前面向同平台最小可用，后续若要跨平台，需要固定端序与字段编码。
 */
class ProjectSerializer
{
public:
    /**
     * @brief 将 Project 写入指定路径。
     * @param project      待保存的项目对象。
     * @param path         输出文件路径。
     * @param errorMessage 失败时写入错误信息（可为 nullptr）。
     * @return true 保存成功；false 保存失败。
     */
    static bool save(const Project& project, const std::string& path, std::string* errorMessage);

    /**
     * @brief 从指定路径加载 Project。
     * @param path         输入文件路径。
     * @param errorMessage 失败时写入错误信息（可为 nullptr）。
     * @return 成功返回 Project；失败返回 nullptr。
     */
    static std::unique_ptr<Project> load(const std::string& path, std::string* errorMessage);
};
