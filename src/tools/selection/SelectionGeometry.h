#pragma once

#include "core/AppContext.h"
#include "imgui.h"

#include <cstdint>
#include <vector>

namespace selection
{
    /**
     * @brief 判断 mask 中是否至少存在一个选中像素。
     */
    bool maskHasAnySelected(const std::vector<uint8_t>& mask);

    /**
     * @brief 将矩形选区按 Replace/Add/Remove 写入目标 mask。
     *
     * rect 会先裁剪到画布范围，避免越界访问。
     */
    void applyRectOpToMask(std::vector<uint8_t>& ioMask,
                           int canvasWidth,
                           int canvasHeight,
                           const AppContext::PixelRect& rect,
                           AppContext::PixelSelectionOp op);

    /**
     * @brief 将椭圆选区按 Replace/Add/Remove 写入目标 mask。
     *
     * 椭圆基于原始外接矩形计算，再裁剪到画布内，避免贴边时形状被重新拉伸。
     */
    void applyEllipseOpToMask(std::vector<uint8_t>& ioMask,
                              int canvasWidth,
                              int canvasHeight,
                              const AppContext::PixelRect& rect,
                              AppContext::PixelSelectionOp op);

    /**
     * @brief 将 applyMask 按 Replace/Add/Remove 合并到目标 mask。
     */
    void applyMaskOpToMask(std::vector<uint8_t>& ioMask,
                           const std::vector<uint8_t>& applyMask,
                           AppContext::PixelSelectionOp op);

    /**
     * @brief 给路径补上一段整数像素线。
     *
     * 用于快速拖拽套索时补齐两帧鼠标采样之间的空隙，避免路径断裂。
     */
    void appendLineToPath(std::vector<ImVec2>& path, int x0, int y0, int x1, int y1);

    /**
     * @brief 根据自由套索路径生成闭合选区 mask。
     *
     * 路径会自动首尾闭合，并额外保留边界像素，保证显示轮廓连续。
     */
    bool buildLassoMaskFromPath(const std::vector<ImVec2>& inputPath,
                                int canvasWidth,
                                int canvasHeight,
                                std::vector<uint8_t>& outMask);

    /**
     * @brief 判断当前点是否足够接近第一个顶点。
     *
     * 多边形套索用它判断“点击起点闭合”。
     */
    bool isCloseToFirstVertex(const std::vector<ImVec2>& vertices,
                              int x,
                              int y,
                              int thresholdPixels);

    /**
     * @brief 根据多边形顶点生成闭合选区 mask。
     *
     * 会先把多边形边离散成像素路径，再复用套索填充逻辑。
     */
    bool buildPolygonMaskFromVertices(const std::vector<ImVec2>& vertices,
                                      int canvasWidth,
                                      int canvasHeight,
                                      std::vector<uint8_t>& outMask);
}
