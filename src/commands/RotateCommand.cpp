#include "commands/RotateCommand.h"

#include "core/AppContext.h"
#include "core/Project.h"

#include <algorithm>
#include <vector>

namespace commands
{
    namespace
    {
        const char* getActionLabel(RotationAngle angle)
        {
            switch (angle)
            {
            case RotationAngle::Clockwise90:
                return "Rotate 90 CW";
            case RotationAngle::CounterClockwise90:
                return "Rotate 90 CCW";
            case RotationAngle::Rotate180:
                return "Rotate 180";
            default:
                return "Rotate";
            }
        }

        /**
         * @brief 收集本次需要旋转的帧索引。
         *
         * 说明：
         * - 多选帧优先，便于一次性旋转多个动画帧；
         * - 无多选时回退到当前帧；
         * - 最后统一排序、去重并过滤越界索引，避免重复处理同一帧。
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
         * @brief 对单帧像素执行旋转，保持画布尺寸不变。
         *
         * 坐标映射：
         * - 90 CW：  (x, y) -> (height - 1 - y, x)，正方形画布下等价于顺时针旋转；
         * - 90 CCW： (x, y) -> (y, width - 1 - x)，正方形画布下等价于逆时针旋转；
         * - 180：    (x, y) -> (width - 1 - x, height - 1 - y)。
         */
        bool rotateFramePixels(Project::Frame& frame,
                               int width,
                               int height,
                               RotationAngle angle)
        {
            const std::vector<uint32_t> source = frame.pixels;
            std::vector<uint32_t> rotated(source.size(), 0x00000000);

            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    int dstX = x;
                    int dstY = y;
                    switch (angle)
                    {
                    case RotationAngle::Clockwise90:
                        dstX = height - 1 - y;
                        dstY = x;
                        break;
                    case RotationAngle::CounterClockwise90:
                        dstX = y;
                        dstY = width - 1 - x;
                        break;
                    case RotationAngle::Rotate180:
                        dstX = width - 1 - x;
                        dstY = height - 1 - y;
                        break;
                    }

                    const size_t srcIndex = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
                    const size_t dstIndex = static_cast<size_t>(dstY) * static_cast<size_t>(width) + static_cast<size_t>(dstX);
                    rotated[dstIndex] = source[srcIndex];
                }
            }

            if (rotated == source) return false;
            frame.pixels.swap(rotated);
            return true;
        }
    } // namespace

    bool RotateCommand::execute(AppContext& context,
                                RotationAngle angle,
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

        // Project 当前所有帧共享同一画布尺寸；90 度旋转非正方形画布会需要交换宽高，
        // 这会影响整个项目尺寸，因此先明确限制，避免静默裁剪像素。
        if ((angle == RotationAngle::Clockwise90 || angle == RotationAngle::CounterClockwise90)
            && width != height)
        {
            if (outError) *outError = "90-degree rotation currently requires a square canvas.";
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
            if (rotateFramePixels(frame, width, height, angle)) changed = true;
        }

        if (!changed)
        {
            if (outError) *outError = "Rotate did not change any pixels.";
            return false;
        }

        context.setProjectDirty(true, getActionLabel(angle));
        return true;
    }
}
