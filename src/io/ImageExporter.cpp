#include "io/ImageExporter.h"

#include "core/Project.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    // 统一错误信息写回，避免每个分支重复 if(errorMessage)。
    void assignError(std::string* errorMessage, const std::string& message)
    {
        if (errorMessage) *errorMessage = message;
    }

    // 把 RGBA8888 像素缓冲复制到 SDL_Surface。
    // 这里按 surface->pitch 处理行跨度，兼容非紧密内存布局。
    bool copyPixelsToSurface(SDL_Surface* surface,
                            const std::vector<uint32_t>& pixels,
                            int width,
                            int height,
                            std::string* errorMessage)
    {
        if (!surface)
        {
            assignError(errorMessage, "Invalid surface.");
            return false;
        }

        const size_t expectedPixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
        if (pixels.size() != expectedPixelCount)
        {
            assignError(errorMessage, "Pixel buffer size mismatch.");
            return false;
        }

        const int rowBytes = width * static_cast<int>(sizeof(uint32_t));
        const uint8_t* src = reinterpret_cast<const uint8_t*>(pixels.data());
        uint8_t* dst = static_cast<uint8_t*>(surface->pixels);

        if (surface->pitch == rowBytes)
        {
            std::memcpy(dst, src, expectedPixelCount * sizeof(uint32_t));
            return true;
        }

        for (int y = 0; y < height; ++y)
        {
            std::memcpy(dst + y * surface->pitch, src + y * rowBytes, static_cast<size_t>(rowBytes));
        }
        return true;
    }

    // 归一化帧列表：
    // - 若调用方给了 frameIndices，则按调用方选择导出。
    // - 若 frameIndices 为空，则自动扩展为“全部帧”。
    std::vector<int> buildFrameList(const Project& project, const std::vector<int>& frameIndices)
    {
        if (!frameIndices.empty()) return frameIndices;

        std::vector<int> all;
        const int frameCount = project.getFrameCount();
        all.reserve(static_cast<size_t>(frameCount));
        for (int i = 0; i < frameCount; ++i)
            all.push_back(i);
        return all;
    }

    // 校验帧索引范围，防止越界访问 project.getFrame()。
    bool validateFrameList(const Project& project, const std::vector<int>& frameIndices, std::string* errorMessage)
    {
        const int frameCount = project.getFrameCount();
        if (frameCount <= 0)
        {
            assignError(errorMessage, "Project has no frames.");
            return false;
        }

        for (int idx : frameIndices)
        {
            if (idx < 0 || idx >= frameCount)
            {
                assignError(errorMessage, "Frame index out of range: " + std::to_string(idx));
                return false;
            }
        }
        return true;
    }

    // 把一张 RGBA8888 像素图保存为 PNG。
    // 流程：创建 surface -> 复制像素 -> IMG_SavePNG。
    bool savePixelsAsPng(const std::vector<uint32_t>& pixels,
                        int width,
                        int height,
                        const std::string& path,
                        std::string* errorMessage)
    {
        SDL_Surface* surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32);
        if (!surface)
        {
            assignError(errorMessage, std::string("Failed to create surface: ") + SDL_GetError());
            return false;
        }

        const bool copied = copyPixelsToSurface(surface, pixels, width, height, errorMessage);
        if (!copied)
        {
            SDL_DestroySurface(surface);
            return false;
        }

        const bool ok = IMG_SavePNG(surface, path.c_str());
        SDL_DestroySurface(surface);
        if (!ok)
        {
            assignError(errorMessage, std::string("Failed to save PNG: ") + SDL_GetError());
            return false;
        }
        return true;
    }

    /**
     * @brief 计算一组帧在 Row/Column 布局下的块尺寸。
     *
     * 说明：
     * - 分组导出当前只支持每组内部 Row/Column；
     * - RowColumn 属于“整表网格”概念，不适用于单组局部块，若传入会按 Row 兜底。
     */
    void computeGroupBlockSize(ImageExporter::SpriteSheetLayout layout,
                               int frameWidth,
                               int frameHeight,
                               int frameCount,
                               int& outWidth,
                               int& outHeight)
    {
        outWidth = 0;
        outHeight = 0;
        if (frameCount <= 0) return;

        if (layout == ImageExporter::SpriteSheetLayout::Column)
        {
            outWidth = frameWidth;
            outHeight = frameHeight * frameCount;
            return;
        }

        // 默认按 Row 处理（包含显式 Row 与非法/未支持布局）。
        outWidth = frameWidth * frameCount;
        outHeight = frameHeight;
    }

    /**
     * @brief 把一帧像素拷贝到目标图指定偏移位置。
     */
    void blitFramePixels(const std::vector<uint32_t>& srcPixels,
                         int frameWidth,
                         int frameHeight,
                         std::vector<uint32_t>& dstPixels,
                         int dstWidth,
                         int offsetX,
                         int offsetY)
    {
        for (int y = 0; y < frameHeight; ++y)
        {
            const size_t srcRow = static_cast<size_t>(y) * static_cast<size_t>(frameWidth);
            const size_t dstRow = static_cast<size_t>(offsetY + y) * static_cast<size_t>(dstWidth)
                + static_cast<size_t>(offsetX);
            std::copy_n(srcPixels.begin() + static_cast<long long>(srcRow),
                        frameWidth,
                        dstPixels.begin() + static_cast<long long>(dstRow));
        }
    }
} // namespace

bool ImageExporter::exportSingleFramePng(const Project& project,
                                         int frameIndex,
                                         const std::string& path,
                                         std::string* errorMessage)
{
    // 基本入参校验
    if (path.empty())
    {
        assignError(errorMessage, "Export path is empty.");
        return false;
    }

    const int width = project.getWidth();
    const int height = project.getHeight();
    if (width <= 0 || height <= 0)
    {
        assignError(errorMessage, "Invalid project canvas size.");
        return false;
    }

    const int frameCount = project.getFrameCount();
    if (frameIndex < 0 || frameIndex >= frameCount)
    {
        assignError(errorMessage, "Frame index out of range.");
        return false;
    }

    // 导出目标帧
    const Project::Frame& frame = project.getFrame(frameIndex);
    return savePixelsAsPng(frame.pixels, width, height, path, errorMessage);
}

bool ImageExporter::exportSpriteSheetPng(const Project& project,
                                         const std::vector<int>& frameIndices,
                                         SpriteSheetLayout layout,
                                         int columnsPerRow,
                                         const std::string& path,
                                         std::string* errorMessage)
{
    // 参数校验
    if (path.empty())
    {
        assignError(errorMessage, "Export path is empty.");
        return false;
    }

    const int frameWidth = project.getWidth();
    const int frameHeight = project.getHeight();
    if (frameWidth <= 0 || frameHeight <= 0)
    {
        assignError(errorMessage, "Invalid project canvas size.");
        return false;
    }

    const std::vector<int> frames = buildFrameList(project, frameIndices);
    if (frames.empty())
    {
        assignError(errorMessage, "No frames to export.");
        return false;
    }
    if (!validateFrameList(project, frames, errorMessage)) return false;

    // 计算精灵图尺寸
    // Row:
    //   宽 = frameWidth * N, 高 = frameHeight
    // Column:
    //   宽 = frameWidth,     高 = frameHeight * N
    // RowColumn:
    //   每行列数 = columnsPerRow（至少为 1）
    //   行数 = ceil(N / 列数)
    //   宽 = frameWidth * 列数，高 = frameHeight * 行数
    const int n = static_cast<int>(frames.size());
    int sheetWidth = 0;
    int sheetHeight = 0;
    int columns = 1;
    if (layout == SpriteSheetLayout::Row)
    {
        sheetWidth = frameWidth * n;
        sheetHeight = frameHeight;
    }
    else if (layout == SpriteSheetLayout::Column)
    {
        sheetWidth = frameWidth;
        sheetHeight = frameHeight * n;
    }
    else
    {
        columns = std::max(1, columnsPerRow);
        columns = std::min(columns, n);
        const int rows = (n + columns - 1) / columns;
        sheetWidth = frameWidth * columns;
        sheetHeight = frameHeight * rows;
    }
    if (sheetWidth <= 0 || sheetHeight <= 0)
    {
        assignError(errorMessage, "Invalid sprite sheet size.");
        return false;
    }

    // 先创建目标像素缓冲，默认透明填充。
    std::vector<uint32_t> sheetPixels(static_cast<size_t>(sheetWidth) * static_cast<size_t>(sheetHeight), 0x00000000);

    // 把每一帧拷贝到目标区域。
    for (int i = 0; i < n; ++i)
    {
        const int frameIndex = frames[static_cast<size_t>(i)];
        const Project::Frame& frame = project.getFrame(frameIndex);
        const size_t expectedPixelCount = static_cast<size_t>(frameWidth) * static_cast<size_t>(frameHeight);
        if (frame.pixels.size() != expectedPixelCount)
        {
            assignError(errorMessage, "Frame pixel count mismatch: " + std::to_string(frameIndex));
            return false;
        }

        // 根据排布模式计算该帧在输出图中的左上角偏移。
        int offsetX = 0;
        int offsetY = 0;
        if (layout == SpriteSheetLayout::Row)
        {
            offsetX = i * frameWidth;
            offsetY = 0;
        }
        else if (layout == SpriteSheetLayout::Column)
        {
            offsetX = 0;
            offsetY = i * frameHeight;
        }
        else
        {
            const int col = i % columns;
            const int row = i / columns;
            offsetX = col * frameWidth;
            offsetY = row * frameHeight;
        }

        // 逐行拷贝，避免按像素循环造成额外开销。
        for (int y = 0; y < frameHeight; ++y)
        {
            const size_t srcRow = static_cast<size_t>(y) * static_cast<size_t>(frameWidth);
            const size_t dstRow = static_cast<size_t>(offsetY + y) * static_cast<size_t>(sheetWidth)
                + static_cast<size_t>(offsetX);
            std::copy_n(frame.pixels.begin() + static_cast<long long>(srcRow),
                        frameWidth,
                        sheetPixels.begin() + static_cast<long long>(dstRow));
        }
    }

    // 输出 PNG
    return savePixelsAsPng(sheetPixels, sheetWidth, sheetHeight, path, errorMessage);
}

bool ImageExporter::exportGroupedSpriteSheetPng(const Project& project,
                                                const std::vector<SpriteGroup>& groups,
                                                int groupSpacing,
                                                const std::string& path,
                                                std::string* errorMessage)
{
    // ---------------- 1) 基础参数校验 ----------------
    if (path.empty())
    {
        assignError(errorMessage, "Export path is empty.");
        return false;
    }

    const int frameWidth = project.getWidth();
    const int frameHeight = project.getHeight();
    if (frameWidth <= 0 || frameHeight <= 0)
    {
        assignError(errorMessage, "Invalid project canvas size.");
        return false;
    }

    if (groups.empty())
    {
        assignError(errorMessage, "No groups to export.");
        return false;
    }

    // 组间距最小为 0，避免负数导致尺寸计算异常。
    const int spacing = std::max(0, groupSpacing);

    // ---------------- 2) 预校验并计算整图尺寸 ----------------
    // 为了降低内存重分配，我们先计算每组块尺寸，再一次性分配整图像素缓冲。
    std::vector<int> groupBlockWidths;
    std::vector<int> groupBlockHeights;
    groupBlockWidths.reserve(groups.size());
    groupBlockHeights.reserve(groups.size());

    int sheetWidth = 0;
    int sheetHeight = 0;
    for (size_t gi = 0; gi < groups.size(); ++gi)
    {
        const SpriteGroup& group = groups[gi];
        if (group.frameIndices.empty())
        {
            const std::string groupName = group.name.empty()
                ? ("Group#" + std::to_string(static_cast<int>(gi + 1)))
                : group.name;
            assignError(errorMessage, "Group has no frames: " + groupName);
            return false;
        }

        if (!validateFrameList(project, group.frameIndices, errorMessage)) return false;

        int blockW = 0;
        int blockH = 0;
        computeGroupBlockSize(group.layout,
                              frameWidth,
                              frameHeight,
                              static_cast<int>(group.frameIndices.size()),
                              blockW,
                              blockH);

        if (blockW <= 0 || blockH <= 0)
        {
            assignError(errorMessage, "Invalid group block size.");
            return false;
        }

        groupBlockWidths.push_back(blockW);
        groupBlockHeights.push_back(blockH);

        sheetWidth = std::max(sheetWidth, blockW);
        sheetHeight += blockH;
        if (gi + 1 < groups.size()) sheetHeight += spacing;
    }

    if (sheetWidth <= 0 || sheetHeight <= 0)
    {
        assignError(errorMessage, "Invalid grouped sprite sheet size.");
        return false;
    }

    // ---------------- 3) 创建目标像素缓冲 ----------------
    // 先用透明像素填充，未覆盖区域保持透明，方便后续在引擎里直接叠加使用。
    std::vector<uint32_t> sheetPixels(static_cast<size_t>(sheetWidth) * static_cast<size_t>(sheetHeight), 0x00000000);

    // ---------------- 4) 逐组绘制 ----------------
    int groupTopY = 0;
    for (size_t gi = 0; gi < groups.size(); ++gi)
    {
        const SpriteGroup& group = groups[gi];
        const int blockW = groupBlockWidths[gi];
        (void)blockW; // 当前实现默认左对齐，保留变量便于后续支持居中/右对齐策略。

        // 每组内部从 (0, groupTopY) 开始排布。
        // Row:    x 递增，y 固定
        // Column: y 递增，x 固定
        for (size_t i = 0; i < group.frameIndices.size(); ++i)
        {
            const int frameIndex = group.frameIndices[i];
            const Project::Frame& frame = project.getFrame(frameIndex);
            const size_t expectedPixelCount = static_cast<size_t>(frameWidth) * static_cast<size_t>(frameHeight);
            if (frame.pixels.size() != expectedPixelCount)
            {
                assignError(errorMessage, "Frame pixel count mismatch: " + std::to_string(frameIndex));
                return false;
            }

            int offsetX = 0;
            int offsetY = groupTopY;
            if (group.layout == SpriteSheetLayout::Column)
            {
                offsetX = 0;
                offsetY = groupTopY + static_cast<int>(i) * frameHeight;
            }
            else
            {
                // Row 以及当前未支持布局统一按 Row 处理。
                offsetX = static_cast<int>(i) * frameWidth;
                offsetY = groupTopY;
            }

            blitFramePixels(frame.pixels,
                            frameWidth,
                            frameHeight,
                            sheetPixels,
                            sheetWidth,
                            offsetX,
                            offsetY);
        }

        groupTopY += groupBlockHeights[gi];
        if (gi + 1 < groups.size()) groupTopY += spacing;
    }

    // ---------------- 5) 输出 PNG ----------------
    return savePixelsAsPng(sheetPixels, sheetWidth, sheetHeight, path, errorMessage);
}

