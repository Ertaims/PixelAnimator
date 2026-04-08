#include "ProjectWindow.h"

#include "core/AppContext.h"
#include "core/Project.h"
#include "imgui.h"
#include "tools/BrushTool.h"
#include "tools/EraserTool.h"
#include "tools/EyedropperTool.h"
#include "tools/FillTool.h"
#include "tools/LineTool.h"
#include "tools/RectFilledTool.h"
#include "tools/RectangleTool.h"
#include "tools/Tool.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{
    const Tool* resolveTool(ToolType toolType)
    {
        static const BrushTool kBrushTool;
        static const EraserTool kEraserTool;
        static const EyedropperTool kEyedropperTool;
        static const FillTool kFillTool;

        switch (toolType)
        {
        case ToolType::Brush:
            return &kBrushTool;
        case ToolType::Eraser:
            return &kEraserTool;
        case ToolType::Eyedropper:
            return &kEyedropperTool;
        case ToolType::Fill:
            return &kFillTool;
        default:
            return nullptr;
        }
    }

    // 将鼠标屏幕坐标映射到画布像素坐标，并夹到合法范围。
    void getClampedPixelFromMouse(const ImVec2& mousePos,
                                  const ImVec2& imagePos,
                                  int zoom,
                                  int canvasWidth,
                                  int canvasHeight,
                                  int& outX,
                                  int& outY)
    {
        const float localX = mousePos.x - imagePos.x;
        const float localY = mousePos.y - imagePos.y;
        outX = std::clamp(static_cast<int>(localX / static_cast<float>(zoom)), 0, canvasWidth - 1);
        outY = std::clamp(static_cast<int>(localY / static_cast<float>(zoom)), 0, canvasHeight - 1);
    }

    // 将项目内使用的 RGBA8888（R 低字节，A 高字节）转换为 ImGui 颜色。
    ImU32 toImGuiColor(uint32_t rgba)
    {
        const int r = static_cast<int>(rgba & 0xFFu);
        const int g = static_cast<int>((rgba >> 8) & 0xFFu);
        const int b = static_cast<int>((rgba >> 16) & 0xFFu);
        const int a = static_cast<int>((rgba >> 24) & 0xFFu);
        return IM_COL32(r, g, b, a);
    }

    /**
     * @brief 在两点间做离散插值，并调用工具逐点落笔。
     *
     * 设计目的：
     * - 解决“快速拖拽时两帧采样点间距过大，导致笔迹断裂”的问题；
     * - 使用整数步进（DDA），保证每个经过像素都能被工具处理一次。
     */
    bool applyInterpolatedStroke(const Tool& tool,
                                 Project::Frame& frame,
                                 int canvasWidth,
                                 int canvasHeight,
                                 int fromX,
                                 int fromY,
                                 int toX,
                                 int toY,
                                 AppContext& context,
                                 bool isMouseClicked)
    {
        const int dx = toX - fromX;
        const int dy = toY - fromY;
        const int steps = std::max(std::abs(dx), std::abs(dy));

        // 退化情况：起点终点相同，直接单点落笔。
        if (steps <= 0)
        {
            return tool.apply(frame,
                              canvasWidth,
                              canvasHeight,
                              toX,
                              toY,
                              context,
                              isMouseClicked);
        }

        bool changed = false;
        for (int i = 0; i <= steps; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            const int px = fromX + static_cast<int>(std::lround(static_cast<float>(dx) * t));
            const int py = fromY + static_cast<int>(std::lround(static_cast<float>(dy) * t));

            // 仅第一步保留“点击瞬间”语义（给 Fill/Eyedropper 等一次性工具用），
            // 后续补点统一按拖拽连续输入处理。
            const bool stepClicked = (i == 0) ? isMouseClicked : false;
            if (tool.apply(frame,
                           canvasWidth,
                           canvasHeight,
                           px,
                           py,
                           context,
                           stepClicked))
            {
                changed = true;
            }
        }
        return changed;
    }
} // namespace

// 画布面板
void ProjectWindow::renderCanvasPanel(Project* project)
{
    const int width = project->getWidth();
    const int height = project->getHeight();
    int zoom = context->getCanvasZoom();

    // 同步选区掩码尺寸，确保画布尺寸变化后选区状态一致。
    context->ensurePixelSelectionCanvasSize(width, height);

    // 多选状态下，画布始终显示“主选中帧”（选区第一帧）。
    context->sanitizeFrameSelection(project->getFrameCount(), context->getCurrentFrameIndex());
    int frameIndex = context->getPrimarySelectedFrameIndex();
    frameIndex = std::clamp(frameIndex, 0, std::max(0, project->getFrameCount() - 1));
    context->setCurrentFrameIndex(frameIndex);
    const int frameCount = project->getFrameCount();

    Project::Frame& frame = project->getFrame(frameIndex);
    ensureCanvasTexture(width, height);
    uploadCanvasPixels(frame.pixels);

    const ImVec2 panelPos = ImGui::GetCursorScreenPos();
    const ImVec2 panelAvail = ImGui::GetContentRegionAvail();
    const float imageW = static_cast<float>(width * zoom);
    const float imageH = static_cast<float>(height * zoom);
    const ImVec2 centerOffset((panelAvail.x - imageW) * 0.5f, (panelAvail.y - imageH) * 0.5f);
    const float panX = context->getCanvasPanX();
    const float panY = context->getCanvasPanY();
    // 将画布左上角对齐到整数像素，避免亚像素位置导致边缘出现细线伪影。
    const ImVec2 imagePos(
        std::round(panelPos.x + centerOffset.x + panX),
        std::round(panelPos.y + centerOffset.y + panY));

    const ImVec2 hitboxSize(std::max(1.0f, panelAvail.x), std::max(1.0f, panelAvail.y));
    ImGui::InvisibleButton(
        "##CanvasHitbox",
        hitboxSize,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle | ImGuiButtonFlags_MouseButtonRight);

    const bool canvasHitboxHovered = ImGui::IsItemHovered();
    if (canvasHitboxHovered)
    {
        const float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f)
        {
            const int zoomLevels[] = {1, 2, 4, 8, 16, 32, 64, 128, 256};
            int zoomIndex = 0;
            for (int i = 0; i < static_cast<int>(sizeof(zoomLevels) / sizeof(zoomLevels[0])); ++i)
            {
                if (zoomLevels[i] == zoom)
                {
                    zoomIndex = i;
                    break;
                }
            }
            zoomIndex = std::clamp(zoomIndex + (wheel > 0.0f ? 1 : -1), 0, 8);
            context->setCanvasZoom(zoomLevels[zoomIndex]);
            zoom = zoomLevels[zoomIndex];
        }
    }

    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
    {
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        context->addCanvasPan(delta.x, delta.y);
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 imageMin = imagePos;
    const ImVec2 imageMax(imagePos.x + imageW, imagePos.y + imageH);

    if (context->isCheckerboardBackgroundEnabled())
    {
        const ImU32 c1 = IM_COL32(70, 70, 70, 255);
        const ImU32 c2 = IM_COL32(90, 90, 90, 255);
        const float tileW = imageW * 0.5f;
        const float tileH = imageH * 0.5f;
        for (int ty = 0; ty < 2; ++ty)
        {
            for (int tx = 0; tx < 2; ++tx)
            {
                const ImU32 col = ((tx + ty) % 2 == 0) ? c1 : c2;
                const ImVec2 p0(imageMin.x + tx * tileW, imageMin.y + ty * tileH);
                const ImVec2 p1(p0.x + tileW, p0.y + tileH);
                drawList->AddRectFilled(p0, p1, col);
            }
        }
    }
    else
    {
        drawList->AddRectFilled(imageMin, imageMax, IM_COL32(255, 255, 255, 255));
    }

    drawList->AddImage(
        reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(canvasTexture_.texture)),
        imageMin,
        imageMax,
        ImVec2(0, 0),
        ImVec2(1, 1));
    // 将边框向外扩 1px，避免边框压在画布内容内侧。
    const ImVec2 borderMin(imageMin.x - 1.0f, imageMin.y - 1.0f);
    const ImVec2 borderMax(imageMax.x + 1.0f, imageMax.y + 1.0f);
    drawList->AddRect(borderMin, borderMax, IM_COL32(180, 180, 180, 255));

    if (context->isGridVisible() && zoom >= 4)
    {
        const ImU32 gridColor = IM_COL32(80, 80, 80, 120);
        for (int x = 1; x < width; ++x)
        {
            const float gx = imagePos.x + static_cast<float>(x * zoom);
            drawList->AddLine(ImVec2(gx, imagePos.y), ImVec2(gx, imagePos.y + imageH), gridColor);
        }
        for (int y = 1; y < height; ++y)
        {
            const float gy = imagePos.y + static_cast<float>(y * zoom);
            drawList->AddLine(ImVec2(imagePos.x, gy), ImVec2(imagePos.x + imageW, gy), gridColor);
        }
    }

    const ImVec2 mousePos = ImGui::GetMousePos();
    const bool anyPopupOpen = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
    // 矩形模式切换弹窗是“非阻塞 popup”：打开时不应禁用窗口内其它功能。
    const bool blockingPopupOpen = anyPopupOpen && !toolbarState_.rectModePopupVisible;
    const bool hovered =
        mousePos.x >= imagePos.x &&
        mousePos.y >= imagePos.y &&
        mousePos.x < (imagePos.x + imageW) &&
        mousePos.y < (imagePos.y + imageH);

    int mousePixelX = 0;
    int mousePixelY = 0;
    getClampedPixelFromMouse(mousePos, imagePos, zoom, width, height, mousePixelX, mousePixelY);

    // 将矩形框选工具作为独立类处理输入与叠加渲染。
    if (context->getTool() == ToolType::RectSelection)
    {
        bool selectionPixelsChanged = false;
        rectSelectionTool_.handleInteraction(
            *context,
            frame,
            mousePos,
            canvasHitboxHovered,
            hovered,
            blockingPopupOpen,
            imagePos,
            zoom,
            width,
            height,
            selectionPixelsChanged);

        // 框选工具对像素产生变换（平移/缩放）后，同样需要标记项目已修改。
        if (selectionPixelsChanged)
        {
            if (context->hasMultiFrameSelection()) context->setSingleFrameSelection(frameIndex, frameCount);
            context->setProjectDirty(true);
        }
    }
    else
    {
        // 切换到其它工具时，清理框选交互临时态，避免残留拖拽预览。
        rectSelectionTool_.resetInteractionState();
    }

    // 将直线工具作为独立类处理输入与实时预览。
    if (context->getTool() == ToolType::Line)
    {
        bool linePixelsCommitted = false;
        lineTool_.handleInteraction(
            *context,
            frame,
            canvasHitboxHovered,
            hovered,
            blockingPopupOpen,
            mousePixelX,
            mousePixelY,
            width,
            height,
            linePixelsCommitted);
        if (linePixelsCommitted)
        {
            if (context->hasMultiFrameSelection()) context->setSingleFrameSelection(frameIndex, frameCount);
            context->setProjectDirty(true);
        }
    }
    else
    {
        // 切换到其它工具时，清理直线工具预览状态并恢复快照（若有）。
        lineTool_.resetInteractionState(&frame);
    }

    // 将矩形描边工具作为独立类处理输入与实时预览。
    if (context->getTool() == ToolType::Rect)
    {
        bool rectPixelsCommitted = false;
        rectangleTool_.handleInteraction(
            *context,
            frame,
            canvasHitboxHovered,
            hovered,
            blockingPopupOpen,
            mousePixelX,
            mousePixelY,
            width,
            height,
            rectPixelsCommitted);
        if (rectPixelsCommitted)
        {
            if (context->hasMultiFrameSelection()) context->setSingleFrameSelection(frameIndex, frameCount);
            context->setProjectDirty(true);
        }
    }
    else
    {
        rectangleTool_.resetInteractionState(&frame);
    }

    // 将填充矩形工具作为独立类处理输入与实时预览。
    if (context->getTool() == ToolType::RectFilled)
    {
        bool rectFilledPixelsCommitted = false;
        rectFilledTool_.handleInteraction(
            *context,
            frame,
            canvasHitboxHovered,
            hovered,
            anyPopupOpen,
            mousePixelX,
            mousePixelY,
            width,
            height,
            rectFilledPixelsCommitted);
        if (rectFilledPixelsCommitted)
        {
            if (context->hasMultiFrameSelection()) context->setSingleFrameSelection(frameIndex, frameCount);
            context->setProjectDirty(true);
        }
    }
    else
    {
        rectFilledTool_.resetInteractionState(&frame);
    }

    // 常规像素编辑工具仅在非 RectSelection 下处理。
    const ToolType activeTool = context->getTool();
    const bool isBrushLikeTool = (activeTool == ToolType::Brush || activeTool == ToolType::Eraser);
    if (!blockingPopupOpen
        && canvasHitboxHovered
        && hovered
        && ImGui::IsMouseDown(ImGuiMouseButton_Left)
        && context->getTool() != ToolType::RectSelection
        && context->getTool() != ToolType::Line
        && context->getTool() != ToolType::Rect
        && context->getTool() != ToolType::RectFilled)
    {
        const Tool* tool = resolveTool(context->getTool());
        if (tool)
        {
            bool changed = false;
            const bool justClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);

            if (isBrushLikeTool)
            {
                /**
                 * 连续笔划策略：
                 * - 首帧：初始化笔划锚点并正常落笔；
                 * - 后续帧：在上一点与当前点之间插值补点，避免快速拖拽断线。
                 */
                if (!strokeState_.active || justClicked || strokeState_.tool != activeTool)
                {
                    strokeState_.active = true;
                    strokeState_.lastX = mousePixelX;
                    strokeState_.lastY = mousePixelY;
                    strokeState_.tool = activeTool;
                }

                changed = applyInterpolatedStroke(
                    *tool,
                    frame,
                    width,
                    height,
                    strokeState_.lastX,
                    strokeState_.lastY,
                    mousePixelX,
                    mousePixelY,
                    *context,
                    justClicked);

                strokeState_.lastX = mousePixelX;
                strokeState_.lastY = mousePixelY;
            }
            else
            {
                // 非连续笔划工具维持原有单点输入逻辑。
                changed = tool->apply(
                    frame,
                    width,
                    height,
                    mousePixelX,
                    mousePixelY,
                    *context,
                    justClicked);
            }

            if (changed)
            {
                // 在多选状态下发生实际编辑时，自动退出多选并保留当前帧单选。
                if (context->hasMultiFrameSelection()) context->setSingleFrameSelection(frameIndex, frameCount);
                context->setProjectDirty(true);
            }
        }
    }
    else
    {
        // 鼠标抬起或切换到其它逻辑分支时，结束连续笔划会话。
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || !isBrushLikeTool) strokeState_.active = false;
    }

    // 选区叠加层（包含蚂蚁线）由框选工具类负责绘制。
    rectSelectionTool_.renderOverlay(*context, drawList, imagePos, zoom, blockingPopupOpen);
    // 直线工具叠加层（拖拽辅助线）由直线工具类负责绘制。
    lineTool_.renderOverlay(*context, drawList, imagePos, zoom, blockingPopupOpen);
    // 矩形工具叠加层（当前为空实现，预留扩展）。
    rectangleTool_.renderOverlay(*context, drawList, imagePos, zoom, blockingPopupOpen);
    rectFilledTool_.renderOverlay(*context, drawList, imagePos, zoom, blockingPopupOpen);

    // 鼠标高亮框（弹窗期间隐藏）。
    if (!blockingPopupOpen
        && canvasHitboxHovered
        && hovered
        && context->getTool() != ToolType::RectSelection)
    {
        // 高亮预览与笔刷大小联动：
        // - Brush / Eraser / Line 使用 brushSize（与实际落笔区域一致）；
        // - 其他工具维持 1 像素高亮。
        int previewRadius = 0;
        const ToolType activeTool = context->getTool();
        if (activeTool == ToolType::Brush || activeTool == ToolType::Eraser || activeTool == ToolType::Line) previewRadius = std::max(0, context->getBrushSize() - 1);

        const int minX = std::max(0, mousePixelX - previewRadius);
        const int maxX = std::min(width - 1, mousePixelX + previewRadius);
        const int minY = std::max(0, mousePixelY - previewRadius);
        const int maxY = std::min(height - 1, mousePixelY + previewRadius);

        const ImVec2 hlMin(
            imagePos.x + static_cast<float>(minX * zoom),
            imagePos.y + static_cast<float>(minY * zoom));
        const ImVec2 hlMax(
            imagePos.x + static_cast<float>((maxX + 1) * zoom),
            imagePos.y + static_cast<float>((maxY + 1) * zoom));
        const uint32_t currentColor = context->getColorRGBA();
        // 仅使用当前所选颜色进行实心填充高亮（无半透明、无描边）。
        // 这里增加裁剪区，确保边界像素高亮不会在画布外出现“细线/毛边”。
        drawList->PushClipRect(imageMin, imageMax, true);
        drawList->AddRectFilled(hlMin, hlMax, toImGuiColor(currentColor));
        drawList->PopClipRect();
    }
}
