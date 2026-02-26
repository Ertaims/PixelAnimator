#include "ProjectWindow.h"

#include "core/AppContext.h"
#include "core/Project.h"
#include "imgui.h"

void ProjectWindow::renderRightPanel(Project* project)
{
    ImGui::TextUnformatted("Tool Properties");
    const ToolType tool = context->getTool();
    switch (tool)
    {
    case ToolType::Brush:
    {
        ImGui::TextUnformatted("Current: Brush");
        int brushSize = context->getBrushSize();
        if (ImGui::SliderInt("Brush Size", &brushSize, 1, 32))
            context->setBrushSize(brushSize);
        break;
    }
    case ToolType::Eraser:
    {
        ImGui::TextUnformatted("Current: Eraser");
        int brushSize = context->getBrushSize();
        if (ImGui::SliderInt("Eraser Size", &brushSize, 1, 32))
            context->setBrushSize(brushSize);
        break;
    }
    case ToolType::Eyedropper:
        ImGui::TextUnformatted("Current: Eyedropper");
        ImGui::TextWrapped("Click a pixel on canvas to sample its RGBA color.");
        break;
    case ToolType::Fill:
        ImGui::TextUnformatted("Current: Fill");
        ImGui::TextWrapped("Click a pixel on canvas to flood-fill connected area.");
        break;
    default:
        ImGui::TextUnformatted("Current: Unsupported in toolbar");
        break;
    }

    const int zoomLevels[] = {1, 2, 4, 8, 16, 32};
    const char* zoomLabels[] = {"1x", "2x", "4x", "8x", "16x", "32x"};
    int zoomIndex = 0;
    for (int i = 0; i < 6; ++i)
    {
        if (zoomLevels[i] == context->getCanvasZoom())
        {
            zoomIndex = i;
            break;
        }
    }
    if (ImGui::Combo("Canvas Zoom", &zoomIndex, zoomLabels, 6))
    {
        context->setCanvasZoom(zoomLevels[zoomIndex]);
    }

    bool showGrid = context->isGridVisible();
    if (ImGui::Checkbox("Show Grid", &showGrid))
        context->setGridVisible(showGrid);

    bool onionSkin = context->isOnionSkinEnabled();
    if (ImGui::Checkbox("Onion Skin", &onionSkin))
        context->setOnionSkinEnabled(onionSkin);

    ImGui::Separator();
    ImGui::TextUnformatted("Project");
    ImGui::Text("Name: %s", project->getName().c_str());
    ImGui::Text("Size: %dx%d", project->getWidth(), project->getHeight());
    ImGui::Text("Frames: %d", project->getFrameCount());
    ImGui::Text("Total Pixels: %d", project->getWidth() * project->getHeight());

    if (pendingCanvasWidth_ <= 0 || pendingCanvasHeight_ <= 0)
    {
        pendingCanvasWidth_ = project->getWidth();
        pendingCanvasHeight_ = project->getHeight();
    }

    ImGui::InputInt("Width", &pendingCanvasWidth_);
    ImGui::InputInt("Height", &pendingCanvasHeight_);
    if (ImGui::Button("Apply Size") && pendingCanvasWidth_ > 0 && pendingCanvasHeight_ > 0)
    {
        project->resizeCanvas(pendingCanvasWidth_, pendingCanvasHeight_, 0x00000000);
        context->setProjectDirty(true);
    }
}
