#include "commands/PixelClipboardCommands.h"

#include "core/AppContext.h"
#include "core/Project.h"

#include <algorithm>

namespace commands
{
    namespace
    {
        // 仅检查“有项目 + 有有效当前帧”，不要求存在像素选区。
        bool validateProjectFrameContext(AppContext& context,
                                         Project*& outProject,
                                         Project::Frame*& outFrame,
                                         std::string* outError)
        {
            outProject = nullptr;
            outFrame = nullptr;

            if (!context.hasProject())
            {
                if (outError) *outError = "No active project.";
                return false;
            }

            Project* project = context.getProject();
            if (!project)
            {
                if (outError) *outError = "No project data.";
                return false;
            }
            if (project->getFrameCount() <= 0)
            {
                if (outError) *outError = "Project has no frames.";
                return false;
            }

            const int frameIndex = std::clamp(context.getCurrentFrameIndex(), 0, project->getFrameCount() - 1);
            Project::Frame& frame = project->getFrame(frameIndex);

            outProject = project;
            outFrame = &frame;
            return true;
        }

        // 统一做上下文与选区合法性检查（用于 Copy/Cut）。
        bool validateSelectionContext(AppContext& context,
                                      Project*& outProject,
                                      Project::Frame*& outFrame,
                                      AppContext::PixelRect& outBounds,
                                      std::string* outError)
        {
            outProject = nullptr;
            outFrame = nullptr;
            outBounds = {};

            if (!context.hasPixelSelection())
            {
                if (outError) *outError = "No pixel selection.";
                return false;
            }
            if (!validateProjectFrameContext(context, outProject, outFrame, outError)) return false;

            AppContext::PixelRect bounds;
            if (!context.getPixelSelectionBounds(bounds) || bounds.width <= 0 || bounds.height <= 0)
            {
                if (outError) *outError = "Selection bounds are invalid.";
                return false;
            }

            outBounds = bounds;
            return true;
        }
    } // namespace

    bool PixelClipboardData::isValid() const
    {
        if (width <= 0 || height <= 0) return false;
        const size_t total = static_cast<size_t>(width) * static_cast<size_t>(height);
        if (pixels.size() != total) return false;
        if (mask.size() != total) return false;
        return true;
    }

    void PixelClipboardData::clear()
    {
        width = 0;
        height = 0;
        pixels.clear();
        mask.clear();
    }

    bool CopySelectionCommand::execute(AppContext& context, PixelClipboardData& clipboard, std::string* outError)
    {
        Project* project = nullptr;
        Project::Frame* frame = nullptr;
        AppContext::PixelRect bounds;
        if (!validateSelectionContext(context, project, frame, bounds, outError)) return false;

        const int canvasWidth = project->getWidth();
        const int canvasHeight = project->getHeight();
        const size_t total = static_cast<size_t>(bounds.width) * static_cast<size_t>(bounds.height);

        clipboard.width = bounds.width;
        clipboard.height = bounds.height;
        clipboard.pixels.assign(total, 0x00000000);
        clipboard.mask.assign(total, static_cast<uint8_t>(0));

        bool hasAnyPixel = false;
        for (int ly = 0; ly < bounds.height; ++ly)
        {
            const int sy = bounds.y + ly;
            for (int lx = 0; lx < bounds.width; ++lx)
            {
                const int sx = bounds.x + lx;
                if (!context.isPixelSelected(sx, sy, canvasWidth, canvasHeight)) continue;

                const size_t srcIndex = static_cast<size_t>(sy) * static_cast<size_t>(canvasWidth) + static_cast<size_t>(sx);
                const size_t dstIndex = static_cast<size_t>(ly) * static_cast<size_t>(bounds.width) + static_cast<size_t>(lx);
                clipboard.pixels[dstIndex] = frame->pixels[srcIndex];
                clipboard.mask[dstIndex] = 1;
                hasAnyPixel = true;
            }
        }

        if (!hasAnyPixel)
        {
            clipboard.clear();
            if (outError) *outError = "Selection is empty.";
            return false;
        }
        return true;
    }

    bool CutSelectionCommand::execute(AppContext& context, PixelClipboardData& clipboard, std::string* outError)
    {
        if (!CopySelectionCommand::execute(context, clipboard, outError)) return false;

        Project* project = nullptr;
        Project::Frame* frame = nullptr;
        AppContext::PixelRect bounds;
        if (!validateSelectionContext(context, project, frame, bounds, outError)) return false;

        const int canvasWidth = project->getWidth();
        const int canvasHeight = project->getHeight();

        bool changed = false;
        for (int y = bounds.y; y < bounds.y + bounds.height; ++y)
        {
            const size_t rowOffset = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth);
            for (int x = bounds.x; x < bounds.x + bounds.width; ++x)
            {
                if (!context.isPixelSelected(x, y, canvasWidth, canvasHeight)) continue;

                const size_t index = rowOffset + static_cast<size_t>(x);
                if (frame->pixels[index] == 0x00000000) continue;
                frame->pixels[index] = 0x00000000;
                changed = true;
            }
        }

        if (!changed)
        {
            if (outError) *outError = "Cut did not change any pixels.";
            return false;
        }
        return true;
    }

    bool PasteSelectionCommand::execute(AppContext& context,
                                        const PixelClipboardData& clipboard,
                                        std::string* outError,
                                        int customOriginX,
                                        int customOriginY)
    {
        if (!clipboard.isValid())
        {
            if (outError) *outError = "Clipboard is empty.";
            return false;
        }

        Project* project = nullptr;
        Project::Frame* frame = nullptr;
        if (!validateProjectFrameContext(context, project, frame, outError)) return false;

        const int canvasWidth = project->getWidth();
        const int canvasHeight = project->getHeight();

        // 粘贴锚点：
        // - 预览模式下由 customOrigin 指定；
        // - 非预览调用时默认贴到画布左上角。
        const bool useCustomOrigin = customOriginX >= 0 && customOriginY >= 0;
        const int originX = useCustomOrigin ? customOriginX : 0;
        const int originY = useCustomOrigin ? customOriginY : 0;

        bool changed = false;
        for (int ly = 0; ly < clipboard.height; ++ly)
        {
            const int dy = originY + ly;
            if (dy < 0 || dy >= canvasHeight) continue;

            for (int lx = 0; lx < clipboard.width; ++lx)
            {
                const size_t srcIndex = static_cast<size_t>(ly) * static_cast<size_t>(clipboard.width) + static_cast<size_t>(lx);
                if (clipboard.mask[srcIndex] == 0) continue;

                const int dx = originX + lx;
                if (dx < 0 || dx >= canvasWidth) continue;

                const size_t dstIndex = static_cast<size_t>(dy) * static_cast<size_t>(canvasWidth) + static_cast<size_t>(dx);
                const uint32_t srcPixel = clipboard.pixels[srcIndex];
                if (frame->pixels[dstIndex] == srcPixel) continue;
                frame->pixels[dstIndex] = srcPixel;
                changed = true;
            }
        }

        if (!changed)
        {
            if (outError) *outError = "Paste did not change any pixels.";
            return false;
        }
        return true;
    }
} // namespace commands

