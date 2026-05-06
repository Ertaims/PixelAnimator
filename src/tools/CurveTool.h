#pragma once

#include "Tool.h"
#include "imgui.h"
#include <vector>

/**
 * @brief 曲线工具（独立工具类，三次贝塞尔）。
 *
 * 交互流程：
 * 1. 左键按下并拖拽：确定起点和终点（弦）；
 * 2. 释放后进入“控制点1调整阶段”（左键确认）；
 * 3. 进入“控制点2调整阶段”（左键提交）；
 * 4. 任一阶段右键取消。
 *
 * 说明：
 * - 预览始终基于 basePixels 重算，避免累计重采样误差；
 * - 线宽复用 BrushSize，与直线工具一致。
 */
class CurveTool final : public Tool
{
public:
    ToolType type() const override { return ToolType::Curve; }

    bool apply(Project::Frame& frame,
               int canvasWidth,
               int canvasHeight,
               int x,
               int y,
               AppContext& context,
               bool isMouseClicked) const override;

    /**
     * @brief 处理曲线工具状态机并更新实时预览。
     *
     * @param context 应用上下文（颜色、选区限制等）
     * @param frame 当前帧像素（会被预览重写）
     * @param canvasHitboxHovered 鼠标是否位于画布面板命中区域
     * @param hoveredOnImage 鼠标是否位于画布图像矩形内
     * @param anyPopupOpen 是否有弹窗打开（打开时会中断交互）
     * @param mousePixelX 当前鼠标像素 X（已夹取）
     * @param mousePixelY 当前鼠标像素 Y（已夹取）
     * @param canvasWidth 画布宽
     * @param canvasHeight 画布高
     * @param outPixelsCommitted 是否完成一次最终提交
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
     * @brief 可选叠加层渲染（当前未使用，保留接口一致性）。
     */
    void renderOverlay(const AppContext& context,
                       ImDrawList* drawList,
                       const ImVec2& imagePos,
                       int zoom,
                       bool anyPopupOpen) const;

    /**
     * @brief 重置交互状态；若传入 frame 则恢复到快照。
     */
    void resetInteractionState(Project::Frame* frame = nullptr);

private:
    struct InteractionState
    {
        enum class Phase : int
        {
            None = 0,          // 空闲
            DefiningSegment,   // 正在拖拽定义起终点（左键按住）
            AdjustingControl1, // 已确定起终点，正在调整控制点1（左键确认）
            AdjustingControl2  // 已确定控制点1，正在调整控制点2（左键提交）
        };

        Phase phase = Phase::None;
        int startX = 0;                    // 起点 X
        int startY = 0;                    // 起点 Y
        int endX = 0;                      // 终点 X
        int endY = 0;                      // 终点 Y
        int control1X = 0;                 // 控制点1 X
        int control1Y = 0;                 // 控制点1 Y
        int control2X = 0;                 // 控制点2 X
        int control2Y = 0;                 // 控制点2 Y
        int control2AnchorX = 0;           // 进入控制点2阶段时的鼠标 X，用于避免刚确认控制点1时预览跳变
        int control2AnchorY = 0;           // 进入控制点2阶段时的鼠标 Y
        int control2StartX = 0;            // 控制点2开始调整前的位置，用于按鼠标位移平滑移动
        int control2StartY = 0;            // 控制点2开始调整前的位置，用于按鼠标位移平滑移动
        std::vector<uint32_t> basePixels;  // 交互开始时像素快照（所有预览都基于它）
        bool hasBasePixels = false;        // 是否已有有效快照
        bool waitingForControl2Move = false; // 控制点1刚确认后，等待鼠标真正移动再更新控制点2
    };

    InteractionState m_state;
};

