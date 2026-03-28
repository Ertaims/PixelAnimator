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
        if (errorMessage)
            *errorMessage = message;
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
        if (!frameIndices.empty())
            return frameIndices;

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
} // namespace

bool ImageExporter::exportSingleFramePng(const Project& project,
                                         int frameIndex,
                                         const std::string& path,
                                         std::string* errorMessage)
{
    // 1) 基本入参校验
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

    // 2) 导出目标帧
    const Project::Frame& frame = project.getFrame(frameIndex);
    return savePixelsAsPng(frame.pixels, width, height, path, errorMessage);
}

bool ImageExporter::exportSpriteSheetPng(const Project& project,
                                         const std::vector<int>& frameIndices,
                                         SpriteSheetLayout layout,
                                         const std::string& path,
                                         std::string* errorMessage)
{
    // 1) 基本入参校验
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
    if (!validateFrameList(project, frames, errorMessage))
        return false;

    // 2) 计算精灵图尺寸
    // Row:    宽 = frameWidth * N, 高 = frameHeight
    // Column: 宽 = frameWidth,     高 = frameHeight * N
    const int n = static_cast<int>(frames.size());
    const int sheetWidth = (layout == SpriteSheetLayout::Row) ? frameWidth * n : frameWidth;
    const int sheetHeight = (layout == SpriteSheetLayout::Row) ? frameHeight : frameHeight * n;
    if (sheetWidth <= 0 || sheetHeight <= 0)
    {
        assignError(errorMessage, "Invalid sprite sheet size.");
        return false;
    }

    // 3) 先创建目标像素缓冲，默认透明填充。
    std::vector<uint32_t> sheetPixels(static_cast<size_t>(sheetWidth) * static_cast<size_t>(sheetHeight), 0x00000000);

    // 4) 把每一帧拷贝到目标区域。
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

        const int offsetX = (layout == SpriteSheetLayout::Row) ? i * frameWidth : 0;
        const int offsetY = (layout == SpriteSheetLayout::Row) ? 0 : i * frameHeight;

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

    // 5) 输出 PNG
    return savePixelsAsPng(sheetPixels, sheetWidth, sheetHeight, path, errorMessage);
}
