#pragma once

#include "core/AppContext.h"

#include <cstdint>
#include <vector>

namespace selection
{
    /**
     * @brief 缩放手柄拖拽后的标准化结果。
     *
     * rect 是裁剪到画布内的目标矩形；flipX/flipY 表示拖拽跨过对边后发生了镜像翻转。
     */
    struct ResizeResult
    {
        AppContext::PixelRect rect;
        bool flipX = false;
        bool flipY = false;
    };

    /**
     * @brief 根据八手柄拖拽位移计算新的选区外接矩形。
     *
     * handle 取值 0~7，顺序为 NW/N/NE/E/SE/S/SW/W；keepAspect 为 true 时保持原比例缩放。
     */
    ResizeResult buildResizedRect(const AppContext::PixelRect& initial,
                                  int handle,
                                  int deltaX,
                                  int deltaY,
                                  int canvasWidth,
                                  int canvasHeight,
                                  bool keepAspect);

    /**
     * @brief 按 dx/dy 移动源选区像素，生成新的像素缓冲。
     *
     * 这是“剪切式移动”：先清空源选区，再把源像素写到目标位置；越界目标会被丢弃。
     */
    bool buildMovedPixelsFromSource(const std::vector<uint32_t>& sourcePixels,
                                    std::vector<uint32_t>& outPixels,
                                    const std::vector<uint8_t>& sourceMask,
                                    int canvasWidth,
                                    int canvasHeight,
                                    int dx,
                                    int dy);

    /**
     * @brief 将源选区像素从 fromRect 缩放到 toRect。
     *
     * 使用最近邻反向采样，避免缩放后出现空洞；flipX/flipY 用于处理拖拽跨边后的镜像。
     */
    bool buildScaledPixelsFromSource(const std::vector<uint32_t>& sourcePixels,
                                     std::vector<uint32_t>& outPixels,
                                     const std::vector<uint8_t>& sourceMask,
                                     int canvasWidth,
                                     int canvasHeight,
                                     const AppContext::PixelRect& fromRect,
                                     const AppContext::PixelRect& toRect,
                                     bool flipX,
                                     bool flipY);

    /**
     * @brief 按 dx/dy 移动源选区 mask，生成预览用 mask。
     */
    bool buildMovedMaskFromSource(const std::vector<uint8_t>& sourceMask,
                                  std::vector<uint8_t>& outMask,
                                  int canvasWidth,
                                  int canvasHeight,
                                  int dx,
                                  int dy);

    /**
     * @brief 将源选区 mask 从 fromRect 缩放到 toRect。
     *
     * 与像素缩放使用同一套最近邻反向采样规则，保证预览轮廓和提交结果一致。
     */
    bool buildScaledMaskFromSource(const std::vector<uint8_t>& sourceMask,
                                   std::vector<uint8_t>& outMask,
                                   int canvasWidth,
                                   int canvasHeight,
                                   const AppContext::PixelRect& fromRect,
                                   const AppContext::PixelRect& toRect,
                                   bool flipX,
                                   bool flipY);
}
