#include "DeleteCommand.h"
#include "core/AppContext.h"
#include "core/Project.h"
#include <algorithm>

namespace commands
{
    bool DeleteCommand::execute(AppContext& context, std::string* outError)
    {
        // 检查是否有打开的项目
        if (!context.hasProject())
        {
            if (outError) *outError = "No project open.";
            return false;
        }

        Project* project = context.getProject();

        // 检查是否有像素选区
        if (context.hasPixelSelection())
        {
            // 有选区：清空选区内的像素为透明
            int canvasWidth = project->getWidth();
            int canvasHeight = project->getHeight();
            int frameIndex = context.getCurrentFrameIndex();
            Project::Frame& frame = project->getFrame(frameIndex);

            bool changed = false;

            // 遍历画布上的每个像素
            for (int y = 0; y < canvasHeight; ++y)
            {
                for (int x = 0; x < canvasWidth; ++x)
                {
                    // 检查像素是否在选区内
                    if (context.isPixelSelected(x, y, canvasWidth, canvasHeight))
                    {
                        // 清空为透明
                        size_t index = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth) + static_cast<size_t>(x);
                        if (frame.pixels[index] != 0)
                        {
                            frame.pixels[index] = 0;
                            changed = true;
                        }
                    }
                }
            }

            if (changed)
            {
                // 清除像素选择，使选框消失
                context.clearPixelSelection();
                
                // 重置矩形选择工具的缓存，避免缩放时恢复已删除的像素
                // 方法：临时切换工具再切回，强制工具状态重置
                const auto currentTool = context.getTool();
                if (currentTool == ToolType::RectSelection)
                {
                    context.setTool(ToolType::Brush);
                    context.setTool(ToolType::RectSelection);
                }
                
                context.setProjectDirty(true, "Delete Selection");
                return true;
            }
            else
            {
                if (outError) *outError = "No pixels to delete in selection.";
                return false;
            }
        }
        else
        {
            // 没有选区：删除当前帧或选中的帧
            int frameCount = project->getFrameCount();
            if (frameCount <= 1)
            {
                if (outError) *outError = "Cannot delete the last frame.";
                return false;
            }

            // 检查是否处于多选状态
            if (context.hasMultiFrameSelection())
            {
                // 多选状态：删除所有选中的帧
                const std::vector<int>& selectedFrames = context.getSelectedFrameIndices();
                
                // 创建一个副本并排序（从后往前删除，避免索引变化）
                std::vector<int> framesToDelete = selectedFrames;
                std::sort(framesToDelete.begin(), framesToDelete.end(), std::greater<int>());

                for (int frameIndex : framesToDelete)
                {
                    project->removeFrame(frameIndex);
                    // 更新上下文，处理帧删除后的索引变化
                    context.onFrameRemoved(frameIndex, project->getFrameCount());
                }

                context.setProjectDirty(true, "Delete Frames");
                return true;
            }
            else
            {
                // 单选状态：删除当前帧
                int currentFrameIndex = context.getCurrentFrameIndex();
                project->removeFrame(currentFrameIndex);
                // 更新上下文，处理帧删除后的索引变化
                context.onFrameRemoved(currentFrameIndex, project->getFrameCount());
                context.setProjectDirty(true, "Delete Frame");
                return true;
            }
        }
    }
}