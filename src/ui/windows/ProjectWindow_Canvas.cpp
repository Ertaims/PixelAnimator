#include "ProjectWindow.h"

#include "core/AppContext.h"
#include "core/Project.h"
#include "imgui.h"
#include "tools/BrushTool.h"
#include "tools/EraserTool.h"
#include "tools/EyedropperTool.h"
#include "tools/FillTool.h"
#include "tools/Tool.h"

#include <algorithm>

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
    const ImVec2 imagePos(panelPos.x + centerOffset.x + panX, panelPos.y + centerOffset.y + panY);

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
    drawList->AddRect(imageMin, imageMax, IM_COL32(180, 180, 180, 255));

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
            anyPopupOpen,
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

    // 常规像素编辑工具仅在非 RectSelection 下处理。
    if (!anyPopupOpen
        && canvasHitboxHovered
        && hovered
        && ImGui::IsMouseDown(ImGuiMouseButton_Left)
        && context->getTool() != ToolType::RectSelection)
    {
        const Tool* tool = resolveTool(context->getTool());
        if (tool)
        {
            const bool changed = tool->apply(
                frame,
                width,
                height,
                mousePixelX,
                mousePixelY,
                *context,
                ImGui::IsMouseClicked(ImGuiMouseButton_Left));
            if (changed)
            {
                // 在多选状态下发生实际编辑时，自动退出多选并保留当前帧单选。
                if (context->hasMultiFrameSelection())
                    context->setSingleFrameSelection(frameIndex, frameCount);
                context->setProjectDirty(true);
            }
        }
    }

    // 选区叠加层（包含蚂蚁线）由框选工具类负责绘制。
    rectSelectionTool_.renderOverlay(*context, drawList, imagePos, zoom, anyPopupOpen);

    // 鼠标高亮框（弹窗期间隐藏）。
    if (!anyPopupOpen && canvasHitboxHovered && hovered)
    {
        const ImVec2 hlMin(imagePos.x + static_cast<float>(mousePixelX * zoom), imagePos.y + static_cast<float>(mousePixelY * zoom));
        const ImVec2 hlMax(hlMin.x + static_cast<float>(zoom), hlMin.y + static_cast<float>(zoom));
        drawList->AddRect(hlMin, hlMax, IM_COL32(255, 255, 0, 200));
    }
}
