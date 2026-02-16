#include "Project.h"

#include <algorithm>
#include <stdexcept>

namespace
{
// 保证宽高/帧数为正值，避免 0 或负数导致空画布
int clampPositive(int value)
{
    return std::max(1, value);
}
} // namespace

Project::Project() : Project(128, 128, 1, 0x00000000) {}

Project::Project(int width, int height, int frameCount, uint32_t fillColor)
{
    // 规范化参数，保证画布有效
    width_ = clampPositive(width);
    height_ = clampPositive(height);
    // 创建并填充帧数据
    createFrames(std::max(1, frameCount), fillColor);
}

Project::Frame& Project::getFrame(int index)
{
    // 边界检查，防止越界访问
    if (index < 0 || index >= static_cast<int>(frames_.size()))
        throw std::out_of_range("Project::getFrame index out of range");
    return frames_[static_cast<size_t>(index)];
}

const Project::Frame& Project::getFrame(int index) const
{
    // 边界检查，防止越界访问
    if (index < 0 || index >= static_cast<int>(frames_.size()))
        throw std::out_of_range("Project::getFrame index out of range");
    return frames_[static_cast<size_t>(index)];
}

void Project::resizeCanvas(int width, int height, uint32_t fillColor)
{
    const int newWidth = clampPositive(width);
    const int newHeight = clampPositive(height);

    // 尺寸不变则直接返回
    if (newWidth == width_ && newHeight == height_)
        return;

    for (Frame& frame : frames_)
    {
        // 新画布先用填充色初始化
        std::vector<uint32_t> newPixels(
            static_cast<size_t>(newWidth) * static_cast<size_t>(newHeight),
            fillColor);

        // 仅拷贝重叠区域（左上角对齐）
        const int copyWidth = std::min(width_, newWidth);
        const int copyHeight = std::min(height_, newHeight);

        for (int y = 0; y < copyHeight; ++y)
        {
            const size_t oldRow = static_cast<size_t>(y) * static_cast<size_t>(width_);
            const size_t newRow = static_cast<size_t>(y) * static_cast<size_t>(newWidth);
            std::copy_n(frame.pixels.begin() + static_cast<long long>(oldRow),
                        copyWidth,
                        newPixels.begin() + static_cast<long long>(newRow));
        }

        // 交换内存，避免额外拷贝
        frame.pixels.swap(newPixels);
    }

    // 更新尺寸
    width_ = newWidth;
    height_ = newHeight;
}

void Project::setFrameCount(int count, uint32_t fillColor)
{
    const int newCount = std::max(1, count);
    // 帧数不变则直接返回
    if (newCount == static_cast<int>(frames_.size()))
        return;

    if (newCount < static_cast<int>(frames_.size()))
    {
        // 缩小帧数：直接截断
        frames_.resize(static_cast<size_t>(newCount));
        return;
    }

    // 扩展帧数：新增帧用填充色初始化
    const size_t pixelCount =
        static_cast<size_t>(width_) * static_cast<size_t>(height_);
    const size_t oldCount = frames_.size();
    frames_.resize(static_cast<size_t>(newCount));
    for (size_t i = oldCount; i < frames_.size(); ++i)
    {
        frames_[i].pixels.assign(pixelCount, fillColor);
    }
}

void Project::createFrames(int count, uint32_t fillColor)
{
    frames_.clear();
    frames_.resize(static_cast<size_t>(count));

    // 初始化每一帧的像素数据
    const size_t pixelCount =
        static_cast<size_t>(width_) * static_cast<size_t>(height_);
    for (Frame& frame : frames_)
    {
        frame.pixels.assign(pixelCount, fillColor);
    }
}
