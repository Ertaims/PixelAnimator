#pragma once

#include "core/AppContext.h"
#include "imgui.h"

#include <cstdint>
#include <vector>

/*
 * mask其实就是一张选区地图，用于记录选区。
 * 0：这个像素没有被选中
 * 1：这个像素被选中了
*/
namespace selection
{
    /**
     * @brief 检查当前选区里有没有格子被选中
     */
    bool maskHasAnySelected(const std::vector<uint8_t>& mask);

    /**
     * @brief 将矩形选区按 Replace/Add/Remove 写入目标 mask。
     * 它支持三种操作：
     * Replace：用这个矩形替换原来的选区
     * Add：把这个矩形加到原选区里
     * Remove：从原选区里减掉这个矩形
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
     * @brief 将一张选区 mask 合并到另一张选区 mask。
     *
     * targetMask 是被修改的目标选区；
     * sourceMask 是本次要合并进来的新选区；
     * operation 决定是替换、添加，还是从目标选区里扣除。
     */
    void mergeMaskIntoSelection(std::vector<uint8_t>& targetMask,
                                const std::vector<uint8_t>& sourceMask,
                                AppContext::PixelSelectionOp operation);

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
