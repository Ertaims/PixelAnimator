#pragma once


#include "Tool.h"
#include "imgui.h"
#include <vector>

/**
 * @brief 描边矩形工具（独立工具类，支持实时预览）。
 *
 * 交互模型：
 * - 左键按下：记录起点并缓存当前帧像素；
 * - 拖拽过程：每帧基于缓存重算矩形预览，避免累计叠加失真；
 * - 左键抬起：提交最终结果。
 */
class RectangleTool final : public Tool
{
public:
    ToolType type() const override { return ToolType::Rect; }

    /**
     * @brief 为兼容 Tool 接口保留；矩形工具主流程在 handleInteraction(...) 中。
     */
    bool apply(Project::Frame& frame,
               int canvasWidth,
               int canvasHeight,
               int x,
               int y,
               AppContext& context,
               bool isMouseClicked) const override;

    /**
     * @brief 处理矩形绘制交互并执行实时预览。
     *
     * @param context 编辑器上下文（颜色、选区限制等）
     * @param frame 当前画布帧像素（会被实时预览更新）
     * @param canvasHitboxHovered 鼠标是否命中画布面板区域
     * @param hoveredOnImage 鼠标是否在画布图像区域内
     * @param anyPopupOpen 是否存在弹窗
     * @param mousePixelX 当前鼠标像素 X（已夹取）
     * @param mousePixelY 当前鼠标像素 Y（已夹取）
     * @param canvasWidth 画布宽
     * @param canvasHeight 画布高
     * @param outPixelsCommitted 是否发生最终提交
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
        bool drawing = false;               // 是否正在拖拽矩形
        int startX = 0;                     // 起点 X
        int startY = 0;                     // 起点 Y
        int endX = 0;                       // 终点 X（实时）
        int endY = 0;                       // 终点 Y（实时）
        std::vector<uint32_t> basePixels;   // 拖拽开始时像素快照
        bool hasBasePixels = false;         // 是否已捕获快照
    };

    InteractionState state_;
};

