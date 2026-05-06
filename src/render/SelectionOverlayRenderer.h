#pragma once

#include "core/AppContext.h"
#include "imgui.h"

#include <array>
#include <vector>

namespace render
{
    /**
     * @brief 计算选区八个缩放手柄的屏幕中心点。
     *
     * 手柄会按自身尺寸向选区内部轻微内收，避免选区贴边时手柄一半落到画布外，
     * 从而提高边缘拖拽命中率。
     */
    std::array<ImVec2, 8> getSelectionHandleCenters(const AppContext::PixelRect& rect,
                                                    const ImVec2& imagePos,
                                                    int zoom,
                                                    float handleHalfSize);

    /**
     * @brief 按选区 mask 绘制一层连续实线轮廓。
     *
     * 只绘制 mask 与非选中区域相邻的边，用于预览 Add/Remove/Move/Resize
     * 等操作时的彩色提示轮廓。
     */
    void drawMaskSolidOutline(ImDrawList* drawList,
                              const std::vector<uint8_t>& mask,
                              int canvasWidth,
                              int canvasHeight,
                              const ImVec2& imagePos,
                              int zoom,
                              ImU32 color,
                              float thickness);

    /**
     * @brief 直接按像素矩形绘制实线轮廓。
     *
     * 矩形框选拖拽预览不需要先生成整张 mask；直接画外框可以避免大画布下
     * 每帧分配/扫描整张选区掩码造成卡顿。
     */
    void drawPixelRectSolidOutline(ImDrawList* drawList,
                                   const AppContext::PixelRect& rect,
                                   const ImVec2& imagePos,
                                   int zoom,
                                   ImU32 color,
                                   float thickness);

    /**
     * @brief 按选区 mask 绘制黑白交替的蚂蚁线轮廓。
     *
     * timePhase 控制动画偏移；segmentLength 控制条纹长度。
     */
    void drawMarchingAntsMask(ImDrawList* drawList,
                              const std::vector<uint8_t>& mask,
                              int canvasWidth,
                              int canvasHeight,
                              const ImVec2& imagePos,
                              int zoom,
                              float segmentLength,
                              float timePhase);

    /**
     * @brief 直接按像素矩形绘制黑白交替蚂蚁线。
     *
     * 用于矩形选区的实时预览/已提交矩形选区显示，复杂度接近矩形周长，
     * 避免对整张 mask 做逐像素边界扫描。
     */
    void drawMarchingAntsPixelRect(ImDrawList* drawList,
                                   const AppContext::PixelRect& rect,
                                   const ImVec2& imagePos,
                                   int zoom,
                                   float segmentLength,
                                   float timePhase);

    /**
     * @brief 绘制选区八个缩放手柄。
     *
     * 该函数只负责视觉绘制；命中测试复用 getSelectionHandleCenters。
     */
    void drawSelectionHandles(ImDrawList* drawList,
                              const AppContext::PixelRect& rect,
                              const ImVec2& imagePos,
                              int zoom,
                              float handleHalfSize);

    /**
     * @brief 绘制自由套索的像素化路径辅助线。
     *
     * 自由套索路径已经按像素补点；这里遇到斜向相邻像素时会拆成横线+竖线，
     * 避免预览中出现不符合像素工具观感的斜线。
     */
    void drawPixelLassoGuide(ImDrawList* drawList,
                             const std::vector<ImVec2>& pathPixels,
                             const ImVec2& imagePos,
                             int zoom);

    /**
     * @brief 绘制多边形套索的辅助线与固定顶点。
     *
     * previewVertices 包含当前鼠标悬停点形成的临时预览线；
     * fixedVertices 只包含用户已经确认的顶点，用于绘制黑色顶点块。
     */
    void drawPolygonLassoGuide(ImDrawList* drawList,
                               const std::vector<ImVec2>& previewVertices,
                               const std::vector<ImVec2>& fixedVertices,
                               const ImVec2& imagePos,
                               int zoom);
}
