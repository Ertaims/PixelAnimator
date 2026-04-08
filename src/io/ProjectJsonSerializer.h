#pragma once

#include <memory>
#include <string>

class Project;

/**
 * @brief 项目 JSON 序列化器（可读格式）。
 *
 * 设计目标：
 * - 可读性优先：文件可直接打开查看和手工调试。
 * - 与二进制序列化并存：不替换旧能力，便于平滑迁移。
 * - 最小可用：先覆盖 Project 核心数据（尺寸、帧、像素、项目名）。
 *
 * 当前格式约定（JSON v1）：
 * - root.format  = "PixelAnimatorProject"
 * - root.version = 1
 * - root.canvas.width / root.canvas.height
 * - root.timeline.frameCount
 * - root.frames[i].pixels: "#RRGGBBAA" 字符串数组
 *
 * 注意：
 * - JSON 里像素按字符串存储，可读性好但体积较大。
 * - 后续如果需要更小体积，可扩展为 base64 或压缩编码（版本递增）。
 */
class ProjectJsonSerializer
{
public:
    /**
     * @brief 将 Project 保存为 JSON 文件。
     * @param project      待保存项目。
     * @param path         输出路径（/*.pxanim.json）。
     * @param errorMessage 失败时写入错误信息（可为 nullptr）。
     * @return true 成功，false 失败。
     */
    static bool save(const Project& project, const std::string& path, std::string* errorMessage);

    /**
     * @brief 从 JSON 文件加载 Project。
     * @param path         输入路径。
     * @param errorMessage 失败时写入错误信息（可为 nullptr）。
     * @return 成功返回项目对象；失败返回 nullptr。
     */
    static std::unique_ptr<Project> load(const std::string& path, std::string* errorMessage);
};
