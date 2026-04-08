#pragma once

#include "Tool.h"
#include "imgui.h"
#include <vector>

/**
 * @brief 矩形框选工具。
 *
 * 设计说明：
 * - 与 Brush/Eraser/Fill 一样放在 tools 模块中，成为独立工具类；
 * - 由于框选是“交互型工具”（需要拖拽状态机 + 叠加渲染），
 *   因此除了 Tool::apply 之外，额外提供 handleInteraction / renderOverlay 接口；
 * - Tool::apply 在该工具中不直接修改像素，返回 false。
 */
class RectSelectionTool final : public Tool
{
public:
    ToolType type() const override { return ToolType::RectSelection; }

    /**
     * @brief 与 Tool 接口保持一致；矩形框选不通过该入口改像素，因此固定返回 false。
     */
    bool apply(Project::Frame& frame,
               int canvasWidth,
               int canvasHeight,
               int x,
               int y,
               AppContext& context,
               bool isMouseClicked) const override;

    /**
     * @brief 每帧处理矩形框选输入状态机（创建/增减/平移/缩放）。
     *
     * @param context 编辑器上下文（选区数据读写）
     * @param mousePos 当前鼠标屏幕坐标
     * @param canvasHitboxHovered 鼠标是否在 Canvas 面板命中区域内
     * @param hoveredOnImage 鼠标是否在画布图像矩形内
     * @param anyPopupOpen 当前是否有任意弹窗打开
     * @param imagePos 画布图像左上角屏幕坐标
     * @param zoom 当前缩放倍率
     * @param canvasWidth 画布像素宽
     * @param canvasHeight 画布像素高
     * @param outPixelsChanged 输出参数，表示本帧是否对像素数据产生改动
     */
    void handleInteraction(AppContext& context,
                           Project::Frame& frame,
                           const ImVec2& mousePos,
                           bool canvasHitboxHovered,
                           bool hoveredOnImage,
                           bool anyPopupOpen,
                           const ImVec2& imagePos,
                           int zoom,
                           int canvasWidth,
                           int canvasHeight,
                           bool& outPixelsChanged);

    /**
     * @brief 绘制选区叠加层（外接框、蚂蚁线、8 个手柄）。
     */
    void renderOverlay(const AppContext& context,
                       ImDrawList* drawList,
                       const ImVec2& imagePos,
                       int zoom,
                       bool anyPopupOpen) const;

    /**
     * @brief 清空交互状态（切换工具或窗口切换时可调用）。
     */
    void resetInteractionState();

private:
    /**
     * @brief 矩形框选交互状态机。
     */
    struct InteractionState
    {
        enum class Mode : int
        {
            None = 0,      // 空闲
            BoxSelecting,  // 拖拽框选
            Moving,        // 拖拽平移
            Resizing       // 拖拽手柄缩放
        };

        Mode mode = Mode::None;
        int dragStartX = 0;
        int dragStartY = 0;
        int startMouseX = 0;
        int startMouseY = 0;
        bool removeMode = false;
        AppContext::PixelSelectionOp previewOp = AppContext::PixelSelectionOp::Replace;
        int activeHandle = -1;
        AppContext::PixelRect initialBounds;
        AppContext::PixelRect previewBounds;
        bool previewBoundsValid = false;
        bool previewFlipX = false; // 当前预览是否发生 X 方向翻转
        bool previewFlipY = false; // 当前预览是否发生 Y 方向翻转
    };

    InteractionState state_;

    // 非破坏性变换缓存：
    std::vector<uint32_t> sourceFramePixels_;       // 首次变换时的像素基准
    std::vector<uint8_t> sourceSelectionMask_;      // 与基准对应的选区掩码
    AppContext::PixelRect sourceBounds_;            // 基准选区外接框
    AppContext::PixelRect lastCommittedBounds_;     // 上一次提交后的选区外接框（用于判断缓存是否仍可复用）
    bool sourceCacheValid_ = false;
};
