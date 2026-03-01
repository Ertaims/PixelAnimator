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
#include <vector>

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
} // namespace

void ProjectWindow::renderCanvasPanel(Project* project)
{
    const int width = project->getWidth();
    const int height = project->getHeight();
    int zoom = context->getCanvasZoom();
    const int frameIndex = context->getCurrentFrameIndex();
    const int frameCount = project->getFrameCount();
    ImGui::Text("Canvas  %dx%d   Zoom %dx   Frame %d/%d", width, height, zoom, frameIndex + 1, frameCount);
    ImGui::Separator();

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

    if (ImGui::IsItemHovered())
    {
        const float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f)
        {
            const int zoomLevels[] = {1, 2, 4, 8, 16, 32};
            int zoomIndex = 0;
            for (int i = 0; i < 6; ++i)
            {
                if (zoomLevels[i] == zoom)
                {
                    zoomIndex = i;
                    break;
                }
            }
            zoomIndex = std::clamp(zoomIndex + (wheel > 0.0f ? 1 : -1), 0, 5);
            zoom = zoomLevels[zoomIndex];
            context->setCanvasZoom(zoom);
        }
    }

    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
    {
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        context->addCanvasPan(delta.x, delta.y);
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 imageMin = imagePos;
    const ImVec2 imageMax = ImVec2(imagePos.x + imageW, imagePos.y + imageH);

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
            const float gx = imagePos.x + x * zoom;
            drawList->AddLine(ImVec2(gx, imagePos.y), ImVec2(gx, imagePos.y + imageH), gridColor);
        }
        for (int y = 1; y < height; ++y)
        {
            const float gy = imagePos.y + y * zoom;
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

    // 只在“无弹窗”时才处理画布编辑输入，避免弹窗期间误绘制。
    if (!anyPopupOpen && hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        const float localX = mousePos.x - imagePos.x;
        const float localY = mousePos.y - imagePos.y;
        const int pixelX = std::clamp(static_cast<int>(localX / zoom), 0, width - 1);
        const int pixelY = std::clamp(static_cast<int>(localY / zoom), 0, height - 1);

        const Tool* tool = resolveTool(context->getTool());
        if (tool)
        {
            const bool changed = tool->apply(
                frame,
                width,
                height,
                pixelX,
                pixelY,
                *context,
                ImGui::IsMouseClicked(ImGuiMouseButton_Left));
            if (changed)
                context->setProjectDirty(true);
        }
    }

    if (!anyPopupOpen && hovered)
    {
        const float localX = mousePos.x - imagePos.x;
        const float localY = mousePos.y - imagePos.y;
        const int pixelX = std::clamp(static_cast<int>(localX / zoom), 0, width - 1);
        const int pixelY = std::clamp(static_cast<int>(localY / zoom), 0, height - 1);
        const ImVec2 hlMin(imagePos.x + pixelX * zoom, imagePos.y + pixelY * zoom);
        const ImVec2 hlMax(hlMin.x + zoom, hlMin.y + zoom);
        drawList->AddRect(hlMin, hlMax, IM_COL32(255, 255, 0, 200));
    }
}
