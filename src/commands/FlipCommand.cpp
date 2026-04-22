#include "commands/FlipCommand.h"

#include "core/AppContext.h"
#include "core/Project.h"

#include <algorithm>
#include <vector>

namespace commands
{
    namespace
    {
        const char* getActionLabel(FlipDirection direction)
        {
            switch (direction)
            {
            case FlipDirection::Horizontal:
                return "Flip Horizontal";
            case FlipDirection::Vertical:
                return "Flip Vertical";
            default:
                return "Flip";
            }
        }

        /**
         * @brief 收集需要翻转的帧索引。
         *
         * 说明：
         * - 时间轴多选时优先处理多选帧，方便批量整理动画帧；
         * - 无多选时只处理当前帧；
         * - 排序、去重、过滤越界索引，避免重复处理或访问非法帧。
         */
        std::vector<int> collectTargetFrameIndices(AppContext& context, int frameCount)
        {
            std::vector<int> indices = context.hasMultiFrameSelection()
                ? context.getSelectedFrameIndices()
                : std::vector<int>{context.getCurrentFrameIndex()};

            indices.erase(
                std::remove_if(
                    indices.begin(),
                    indices.end(),
                    [frameCount](int index) { return index < 0 || index >= frameCount; }),
                indices.end());
            std::sort(indices.begin(), indices.end());
            indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
            return indices;
        }

        /**
         * @brief 对单帧像素执行镜像翻转。
         *
         * 坐标映射：
         * - 水平翻转：(x, y) -> (width - 1 - x, y)
         * - 垂直翻转：(x, y) -> (x, height - 1 - y)
         */
        bool flipFramePixels(Project::Frame& frame,
                             int width,
                             int height,
                             FlipDirection direction)
        {
            const std::vector<uint32_t> source = frame.pixels;
            std::vector<uint32_t> flipped(source.size(), 0x00000000);

            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    int dstX = x;
                    int dstY = y;
                    if (direction == FlipDirection::Horizontal)
                    {
                        dstX = width - 1 - x;
                    }
                    else
                    {
                        dstY = height - 1 - y;
                    }

                    const size_t srcIndex = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
                    const size_t dstIndex = static_cast<size_t>(dstY) * static_cast<size_t>(width) + static_cast<size_t>(dstX);
                    flipped[dstIndex] = source[srcIndex];
                }
            }

            if (flipped == source) return false;
            frame.pixels.swap(flipped);
            return true;
        }
    } // namespace

    bool FlipCommand::execute(AppContext& context,
                              FlipDirection direction,
                              std::string* outError)
    {
        if (!context.hasProject())
        {
            if (outError) *outError = "No active project.";
            return false;
        }

        Project* project = context.getProject();
        if (!project || project->getFrameCount() <= 0)
        {
            if (outError) *outError = "Project has no frames.";
            return false;
        }

        const int width = project->getWidth();
        const int height = project->getHeight();
        if (width <= 0 || height <= 0)
        {
            if (outError) *outError = "Canvas size is invalid.";
            return false;
        }

        std::vector<int> targetFrames = collectTargetFrameIndices(context, project->getFrameCount());
        if (targetFrames.empty())
        {
            if (outError) *outError = "No frame selected.";
            return false;
        }

        bool changed = false;
        for (int frameIndex : targetFrames)
        {
            Project::Frame& frame = project->getFrame(frameIndex);
            if (flipFramePixels(frame, width, height, direction)) changed = true;
        }

        if (!changed)
        {
            if (outError) *outError = "Flip did not change any pixels.";
            return false;
        }

        context.setProjectDirty(true, getActionLabel(direction));
        return true;
    }
}
