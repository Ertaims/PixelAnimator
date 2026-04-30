#pragma once

#include "Tool.h"
#include "imgui.h"
#include <vector>

/**
 * @brief 直线工具（独立工具类，支持实时预览）。
 *
 * 设计目标：
 * - 与现有 Brush/Eraser/Fill/RectSelection 一样放在 tools 模块中；
 * - 鼠标按下时记录起点，拖拽时实时预览，松开时提交最终结果；
 * - 预览采用“基于快照重算”，避免拖拽过程中重复叠加造成失真。
 */
class LineTool final : public Tool
{
public:
    ToolType type() const override { return ToolType::Line; }

    /**
     * @brief 为了兼容 Tool 接口保留此方法。
     *
     * 说明：
     * - 直线工具的核心交互在 handleInteraction(...) 中完成；
     * - apply(...) 在本工具中不直接使用，固定返回 false。
     */
    bool apply(Project::Frame& frame,
               int canvasWidth,
               int canvasHeight,
               int x,
               int y,
               AppContext& context,
               bool isMouseClicked) const override;

    /**
     * @brief 处理直线工具输入状态机，并在拖拽过程中实时更新预览像素。
     *
     * @param context 应用上下文（颜色、选区限制等）
     * @param frame 当前帧像素（会被实时预览更新）
     * @param canvasHitboxHovered 鼠标是否命中画布面板区域
     * @param hoveredOnImage 鼠标是否位于画布图像矩形内
     * @param anyPopupOpen 是否有弹窗打开
     * @param mousePixelX 当前鼠标像素 X（已夹到画布范围）
     * @param mousePixelY 当前鼠标像素 Y（已夹到画布范围）
     * @param canvasWidth 画布宽
     * @param canvasHeight 画布高
     * @param outPixelsCommitted 本帧是否发生“最终提交”像素变更
     */
    void handleInteraction(AppContext& context,
                           Project::Frame& frame,
                           bool canvasHitboxHovered,
                           bool hoveredOnImage,
                           bool anyPopupOpen,
                           int mousePixelX,
                           int mousePixelY,
                           int canvasWidth,
                           int canvasHeight,
                           bool& outPixelsCommitted);

    /**
     * @brief 绘制直线工具叠加层（拖拽预览辅助线）。
     */
    void renderOverlay(const AppContext& context,
                       ImDrawList* drawList,
                       const ImVec2& imagePos,
                       int zoom,
                       bool anyPopupOpen) const;

    /**
     * @brief 重置交互状态；可选恢复到拖拽前快照。
     *
     * @param frame 若非空且当前处于拖拽预览中，则恢复到拖拽前像素快照。
     */
    void resetInteractionState(Project::Frame* frame = nullptr);

private:
    struct InteractionState
    {
        bool drawing = false;               // 是否正在拖拽绘制直线
        int startX = 0;                     // 直线起点 X
        int startY = 0;                     // 直线起点 Y
        int endX = 0;                       // 直线终点 X（实时）
        int endY = 0;                       // 直线终点 Y（实时）
        std::vector<uint32_t> basePixels;   // 拖拽开始时的像素快照（预览基准）
        bool hasBasePixels = false;         // 是否已捕获快照
    };

    InteractionState m_state;
};

