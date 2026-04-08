#pragma once

#include "Tool.h"
#include "imgui.h"
#include <vector>

/**
 * @brief 填充矩形工具（独立工具类，支持实时预览）。
 *
 * 交互：
 * - 左键按下记录起点并缓存快照；
 * - 拖拽时每帧基于快照重算填充矩形预览；
 * - 松开左键提交。
 */
class RectFilledTool final : public Tool
{
public:
    ToolType type() const override { return ToolType::RectFilled; }

    /**
     * @brief 与 Tool 接口保持一致；核心交互在 handleInteraction(...) 中完成。
     */
    bool apply(Project::Frame& frame,
               int canvasWidth,
               int canvasHeight,
               int x,
               int y,
               AppContext& context,
               bool isMouseClicked) const override;

    /**
     * @brief 处理填充矩形工具输入与实时预览。
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
     * @brief 当前版本无需额外叠加层，预留接口。
     */
    void renderOverlay(const AppContext& context,
                       ImDrawList* drawList,
                       const ImVec2& imagePos,
                       int zoom,
                       bool anyPopupOpen) const;

    /**
     * @brief 重置交互状态；若存在预览中快照，恢复到快照像素。
     */
    void resetInteractionState(Project::Frame* frame = nullptr);

private:
    struct InteractionState
    {
        bool drawing = false;
        int startX = 0;
        int startY = 0;
        int endX = 0;
        int endY = 0;
        std::vector<uint32_t> basePixels;
        bool hasBasePixels = false;
    };

    InteractionState state_;
};

