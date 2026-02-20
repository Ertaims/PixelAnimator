#pragma once

#include <cstdint>
#include <string>
#include <vector>

/**
 * @brief 像素动画项目的数据模型
 *
 * 主要职责：
 * - 记录画布尺寸（宽高）
 * - 维护帧列表（每帧是 RGBA8888 像素数组）
 * - 提供简单的画布调整与帧数量管理
 *
 * 注意：
 * - 本类只管理内存中的数据，不负责文件 IO
 * - 像素格式为 RGBA8888（uint32_t），与 ImGui/渲染层便于对接
 */
class Project
{
public:
    struct Frame
    {
        // RGBA8888 像素数组，长度始终等于 width * height
        std::vector<uint32_t> pixels;
    };

    // 默认构造：128x128、1 帧、透明填充
    Project();

    // 自定义构造：指定宽高/帧数/填充色
    Project(int width, int height, int frameCount = 1, uint32_t fillColor = 0x00000000);

    // 项目名（UI 展示用）
    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }

    // 画布尺寸
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

    // 帧数量与帧访问
    int getFrameCount() const { return static_cast<int>(frames_.size()); }
    Frame& getFrame(int index);
    const Frame& getFrame(int index) const;

    // 调整画布尺寸（保留左上角旧像素，其余用 fillColor 填充）
    void resizeCanvas(int width, int height, uint32_t fillColor = 0x00000000);

    // 调整帧数量（新增帧用 fillColor 填充）
    void setFrameCount(int count, uint32_t fillColor = 0x00000000);

    // 在指定帧之后插入一帧（新增帧用 fillColor 填充）
    void insertFrameAfter(int index, uint32_t fillColor = 0x00000000);

    // 删除指定帧（至少保留 1 帧）
    void removeFrame(int index);

private:
    // 按当前 width_/height_ 创建指定数量的帧并填充像素
    void createFrames(int count, uint32_t fillColor);

    // 项目信息
    std::string name_ = "Untitled";
    int width_ = 0;
    int height_ = 0;

    // 帧列表
    std::vector<Frame> frames_;
};
