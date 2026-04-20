#pragma once

#include "Tool.h"
#include "imgui.h"
#include <vector>

/**
 * @brief 圆形描边工具（独立工具类，支持实时预览）。
 *
 * 交互：
 * - 左键按下记录起点并缓存快照；
 * - 拖拽时按外接矩形实时预览圆形轮廓；
 * - 松开左键提交。
 */
class CircleTool final : public Tool 
{
public:
    ToolType type() const override { return ToolType::Circle; };

    bool apply(Project::Frame& frame,
               int canvasWidth,
               int canvasHeight,
               int x,
               int y,
               AppContext& context,
               bool isMouseClicked) const override;
    
    /**
     * @brief 处理工具交互。
     *
     * @param context 应用程序上下文。
     * @param frame 当前帧。
     * @param canvasHitboxHovered 画布是否被鼠标悬停。
     * @param hoveredOnImage 鼠标是否悬停在图像上。
     * @param anyPopupOpen 是否有任何弹出窗口打开。
     * @param mousePixelX 鼠标在画布中的 X 坐标。
     **/
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
     * @brief 可选叠加层（当前版本无需额外叠加，保留接口以便后续扩展）。
     */
    void renderOverlay(const AppContext& context,
                       ImDrawList* drawList,
                       const ImVec2& imagePos,
                       int zoom,
                       bool anyPopupOpen) const;

    /**
     * @brief 重置交互状态；若传入 frame 且当前在预览中，会恢复到拖拽前快照。
     */
    void resetInteractionState(Project::Frame* frame = nullptr);

private:
    struct InteractionState
    {
        int startX = 0;
        int startY = 0;
        int endX = 0;
        int endY = 0;
        bool isActive = false;
    };

    InteractionState m_interactionState;
};