#pragma once

#include <memory>
#include <string>

class Project;

/**
 * @brief 最小可用项目序列化器（.pxanim 二进制）。
 */
class ProjectSerializer
{
public:
    static bool save(const Project& project, const std::string& path, std::string* errorMessage);
    static std::unique_ptr<Project> load(const std::string& path, std::string* errorMessage);
};
