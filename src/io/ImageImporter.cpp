#include "io/ImageImporter.h"

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
    // 统一写错误信息，减少重复 if(errorMessage)。
    void assignError(std::string* errorMessage, const std::string& message)
    {
        if (errorMessage) *errorMessage = message;
    }

    /**
     * @brief 从文件加载 RGBA32 Surface。
     *
     * 说明：
     * - 返回的 surface 始终是 SDL_PIXELFORMAT_RGBA32；
     * - 调用方负责 SDL_DestroySurface。
     */
    SDL_Surface* loadRgbaSurface(const std::string& path, std::string* errorMessage)
    {
        SDL_Surface* source = IMG_Load(path.c_str());
        if (!source)
        {
            assignError(errorMessage, std::string("Failed to load image: ") + SDL_GetError());
            return nullptr;
        }

        if (source->format == SDL_PIXELFORMAT_RGBA32) return source;

        SDL_Surface* rgba = SDL_ConvertSurface(source, SDL_PIXELFORMAT_RGBA32);
        SDL_DestroySurface(source);
        if (!rgba)
        {
            assignError(errorMessage, std::string("Failed to convert image format: ") + SDL_GetError());
            return nullptr;
        }
        return rgba;
    }

    /**
     * @brief 从 RGBA32 surface 中提取指定矩形区域像素。
     *
     * 提取结果为 RGBA8888 的 uint32_t 数组，按行优先。
     */
    bool extractPixelsFromRect(const SDL_Surface* surface,
                               int x,
                               int y,
                               int width,
                               int height,
                               std::vector<uint32_t>& outPixels,
                               std::string* errorMessage)
    {
        if (!surface || !surface->pixels)
        {
            assignError(errorMessage, "Invalid image surface.");
            return false;
        }
        if (x < 0 || y < 0 || width <= 0 || height <= 0
            || x + width > surface->w || y + height > surface->h)
        {
            assignError(errorMessage, "Requested image rect is out of bounds.");
            return false;
        }

        outPixels.assign(static_cast<size_t>(width) * static_cast<size_t>(height), 0);
        const uint8_t* base = static_cast<const uint8_t*>(surface->pixels);
        for (int row = 0; row < height; ++row)
        {
            // SDL RGBA32 每像素 4 字节，按 surface->pitch 跨行。
            const uint8_t* srcRow = base + static_cast<long long>(y + row) * surface->pitch
                + static_cast<long long>(x) * 4;
            uint32_t* dstRow = outPixels.data() + static_cast<long long>(row) * width;
            std::memcpy(dstRow, srcRow, static_cast<size_t>(width) * sizeof(uint32_t));
        }
        return true;
    }
} // namespace

bool ImageImporter::importSingleFramePng(Project& project,
                                         int frameIndex,
                                         const std::string& path,
                                         std::string* errorMessage)
{
    if (path.empty())
    {
        assignError(errorMessage, "Import path is empty.");
        return false;
    }

    const int projectWidth = project.getWidth();
    const int projectHeight = project.getHeight();
    if (projectWidth <= 0 || projectHeight <= 0)
    {
        assignError(errorMessage, "Invalid project canvas size.");
        return false;
    }

    if (frameIndex < 0 || frameIndex >= project.getFrameCount())
    {
        assignError(errorMessage, "Frame index out of range.");
        return false;
    }

    SDL_Surface* surface = loadRgbaSurface(path, errorMessage);
    if (!surface) return false;

    // MVP 规则：单帧导入要求尺寸严格一致，避免隐式缩放造成像素失真。
    if (surface->w != projectWidth || surface->h != projectHeight)
    {
        assignError(errorMessage,
                    "Image size does not match canvas size. "
                    "Expected " + std::to_string(projectWidth) + "x" + std::to_string(projectHeight)
                    + ", got " + std::to_string(surface->w) + "x" + std::to_string(surface->h) + ".");
        SDL_DestroySurface(surface);
        return false;
    }

    std::vector<uint32_t> pixels;
    const bool extracted = extractPixelsFromRect(surface, 0, 0, surface->w, surface->h, pixels, errorMessage);
    SDL_DestroySurface(surface);
    if (!extracted) return false;

    project.getFrame(frameIndex).pixels = std::move(pixels);
    return true;
}

bool ImageImporter::importSpriteSheetPng(Project& project,
                                         int insertAfterFrameIndex,
                                         const std::string& path,
                                         bool rowMajorTraversal,
                                         int* outImportedFrameCount,
                                         std::string* errorMessage)
{
    if (outImportedFrameCount) *outImportedFrameCount = 0;

    if (path.empty())
    {
        assignError(errorMessage, "Import path is empty.");
        return false;
    }

    const int frameWidth = project.getWidth();
    const int frameHeight = project.getHeight();
    if (frameWidth <= 0 || frameHeight <= 0)
    {
        assignError(errorMessage, "Invalid project canvas size.");
        return false;
    }

    SpriteSheetSliceResult sliceResult;
    if (!sliceSpriteSheetPng(path,
                             frameWidth,
                             frameHeight,
                             rowMajorTraversal,
                             sliceResult,
                             errorMessage))
    {
        return false;
    }
    const int totalFrames = static_cast<int>(sliceResult.frames.size());

    // 统一规范插入起点，后续每导入一帧都在“上一帧之后”继续追加。
    int anchorIndex = std::clamp(insertAfterFrameIndex, -1, std::max(0, project.getFrameCount() - 1));
    for (const std::vector<uint32_t>& framePixels : sliceResult.frames)
    {
        // 插入空帧后覆写像素，复用 Project 现有插帧接口。
        project.insertFrameAfter(anchorIndex, 0x00000000);
        ++anchorIndex;
        project.getFrame(anchorIndex).pixels = framePixels;
    }

    if (outImportedFrameCount) *outImportedFrameCount = totalFrames;
    return true;
}

bool ImageImporter::sliceSpriteSheetPng(const std::string& path,
                                        int frameWidth,
                                        int frameHeight,
                                        bool rowMajorTraversal,
                                        SpriteSheetSliceResult& outResult,
                                        std::string* errorMessage)
{
    outResult = SpriteSheetSliceResult{};
    if (path.empty())
    {
        assignError(errorMessage, "Import path is empty.");
        return false;
    }
    if (frameWidth <= 0 || frameHeight <= 0)
    {
        assignError(errorMessage, "Invalid slice size.");
        return false;
    }

    SDL_Surface* surface = loadRgbaSurface(path, errorMessage);
    if (!surface) return false;

    outResult.sheetWidth = surface->w;
    outResult.sheetHeight = surface->h;

    if (surface->w % frameWidth != 0 || surface->h % frameHeight != 0)
    {
        assignError(errorMessage,
                    "Sprite sheet size must be divisible by slice size. "
                    "Sheet=" + std::to_string(surface->w) + "x" + std::to_string(surface->h)
                    + ", Slice=" + std::to_string(frameWidth) + "x" + std::to_string(frameHeight) + ".");
        SDL_DestroySurface(surface);
        return false;
    }

    outResult.columns = surface->w / frameWidth;
    outResult.rows = surface->h / frameHeight;
    const int totalFrames = outResult.columns * outResult.rows;
    if (totalFrames <= 0)
    {
        assignError(errorMessage, "No frames found in sprite sheet.");
        SDL_DestroySurface(surface);
        return false;
    }

    outResult.frames.reserve(static_cast<size_t>(totalFrames));
    outResult.tileRows.reserve(static_cast<size_t>(totalFrames));
    outResult.tileCols.reserve(static_cast<size_t>(totalFrames));

    auto readTile = [&](int tileCol, int tileRow) -> bool {
        std::vector<uint32_t> pixels;
        if (!extractPixelsFromRect(surface,
                                   tileCol * frameWidth,
                                   tileRow * frameHeight,
                                   frameWidth,
                                   frameHeight,
                                   pixels,
                                   errorMessage))
        {
            return false;
        }
        outResult.frames.push_back(std::move(pixels));
        outResult.tileRows.push_back(tileRow);
        outResult.tileCols.push_back(tileCol);
        return true;
    };

    bool ok = true;
    if (rowMajorTraversal)
    {
        for (int row = 0; row < outResult.rows && ok; ++row)
        {
            for (int col = 0; col < outResult.columns && ok; ++col)
                ok = readTile(col, row);
        }
    }
    else
    {
        for (int col = 0; col < outResult.columns && ok; ++col)
        {
            for (int row = 0; row < outResult.rows && ok; ++row)
                ok = readTile(col, row);
        }
    }

    SDL_DestroySurface(surface);
    if (!ok) return false;
    return true;
}
