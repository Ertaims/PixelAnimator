#include "ProjectWindow.h"

#include "core/AppContext.h"
#include "core/Project.h"
#include "imgui.h"

#include <cstdint>

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
        if (ImGui::SliderInt("Brush Size", &brushSize, 1, 32)) context->setBrushSize(brushSize);
        break;
    }
    case ToolType::Eraser:
    {
        ImGui::TextUnformatted("Current: Eraser");
        int brushSize = context->getBrushSize();
        if (ImGui::SliderInt("Eraser Size", &brushSize, 1, 32)) context->setBrushSize(brushSize);
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
    case ToolType::RectSelection:
        ImGui::TextUnformatted("Current: Selection");
        if (m_rectSelectionTool.getSelectionShape() == RectSelectionTool::SelectionShape::MagicWand)
        {
            ImGui::BulletText("Left Click: Pick connected region (Replace)");
            ImGui::BulletText("Ctrl + Left Click: Add connected region");
            ImGui::BulletText("Right Click: Remove connected region");
            ImGui::BulletText("Drag inside selection: Move");
            ImGui::BulletText("Drag 8 handles: Resize (Ctrl = keep ratio)");
        }
        else if (m_rectSelectionTool.getSelectionShape() == RectSelectionTool::SelectionShape::Lasso)
        {
            ImGui::BulletText("Left Drag: Free-form selection (Replace)");
            ImGui::BulletText("Ctrl + Left Drag: Add free-form selection");
            ImGui::BulletText("Right Drag: Remove free-form selection");
            ImGui::BulletText("Drag inside selection: Move");
            ImGui::BulletText("Drag 8 handles: Resize (Ctrl = keep ratio)");
        }
        else if (m_rectSelectionTool.getSelectionShape() == RectSelectionTool::SelectionShape::PolygonLasso)
        {
            ImGui::BulletText("Left Click: Add polygon point (Replace/Ctrl=Add)");
            ImGui::BulletText("Right Click: Add polygon point in Remove mode");
            ImGui::BulletText("Click first point: Close and commit polygon");
            ImGui::BulletText("Right click while left polygon active: Cancel");
            ImGui::BulletText("Drag inside selection: Move");
            ImGui::BulletText("Drag 8 handles: Resize (Ctrl = keep ratio)");
        }
        else
        {
            ImGui::BulletText("Left Drag: Replace selection");
            ImGui::BulletText("Ctrl + Left Drag: Add to selection");
            ImGui::BulletText("Right Drag: Remove from selection");
            ImGui::BulletText("Drag inside selection: Move");
            ImGui::BulletText("Drag 8 handles: Resize (Ctrl = keep ratio)");
        }
        if (context->hasPixelSelection())
        {
            if (ImGui::Button("Clear Selection")) context->clearPixelSelection();
        }
        break;
    case ToolType::Line:
    {
        ImGui::TextUnformatted("Current: Line");
        ImGui::TextWrapped("Left drag on canvas to preview and draw a line.");
        int brushSize = context->getBrushSize();
        if (ImGui::SliderInt("Line Width", &brushSize, 1, 32)) context->setBrushSize(brushSize);
        break;
    }
    case ToolType::Curve:
    {
        ImGui::TextUnformatted("Current: Curve");
        ImGui::TextWrapped("Step1: Left drag to define start/end.");
        ImGui::TextWrapped("Step2: move mouse and left click to set Control Point 1.");
        ImGui::TextWrapped("Step3: move mouse and left click to set Control Point 2 and commit.");
        ImGui::TextWrapped("Right click: cancel current curve.");
        int brushSize = context->getBrushSize();
        if (ImGui::SliderInt("Curve Width", &brushSize, 1, 32)) context->setBrushSize(brushSize);
        break;
    }
    case ToolType::Rect:
    {
        ImGui::TextUnformatted("Current: Rectangle");
        ImGui::TextWrapped("Left drag on canvas to preview and draw rectangle outline.");
        int brushSize = context->getBrushSize();
        if (ImGui::SliderInt("Outline Width", &brushSize, 1, 32)) context->setBrushSize(brushSize);
        break;
    }
    case ToolType::RectFilled:
    {
        ImGui::TextUnformatted("Current: Rect Filled");
        ImGui::TextWrapped("Left drag on canvas to preview and draw filled rectangle.");
        break;
    }
    default:
        ImGui::TextUnformatted("Current: Unsupported in toolbar");
        break;
    }

    bool showGrid = context->isGridVisible();
    if (ImGui::Checkbox("Show Grid", &showGrid)) context->setGridVisible(showGrid);

    bool onionSkin = context->isOnionSkinEnabled();
    if (ImGui::Checkbox("Onion Skin", &onionSkin)) context->setOnionSkinEnabled(onionSkin);

    // 对称绘制是全局辅助开关，不再作为某个工具存在；
    // 放在 Onion Skin 下方，方便在任意工具绘制时随手开启/关闭。
    ImGui::TextUnformatted("Symmetry");
    const ImVec2 symmetryIconSize(26.0f, 26.0f);
    auto renderSymmetryToggle = [&](const char* id,
                                    const char* fallbackLabel,
                                    const char* tooltip,
                                    unsigned int icon,
                                    bool enabled) -> bool {
        ImGui::PushID(id);
        if (enabled) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.52f, 0.86f, 1.0f));

        bool clicked = false;
        if (icon != 0)
        {
            clicked = ImGui::ImageButton(
                "##symmetry_icon",
                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(icon)),
                symmetryIconSize);
        }
        else
        {
            clicked = ImGui::Button(fallbackLabel, symmetryIconSize);
        }

        if (enabled)
        {
            ImGui::PopStyleColor();
            ImGui::GetWindowDrawList()->AddRect(
                ImGui::GetItemRectMin(),
                ImGui::GetItemRectMax(),
                IM_COL32(255, 220, 40, 255),
                4.0f,
                0,
                2.0f);
        }

        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
        ImGui::PopID();
        return clicked;
    };

    const bool leftRightSymmetry = context->isLeftRightSymmetryEnabled();
    if (renderSymmetryToggle(
            "LeftRightSymmetry",
            "LR",
            "Toggle Left/Right Symmetry",
            m_toolbarState.symmetryLeftRightIconTexture,
            leftRightSymmetry))
    {
        context->setLeftRightSymmetryEnabled(!leftRightSymmetry);
    }

    ImGui::SameLine();

    const bool upDownSymmetry = context->isUpDownSymmetryEnabled();
    if (renderSymmetryToggle(
            "UpDownSymmetry",
            "UD",
            "Toggle Up/Down Symmetry",
            m_toolbarState.symmetryUpDownIconTexture,
            upDownSymmetry))
    {
        context->setUpDownSymmetryEnabled(!upDownSymmetry);
    }

    // 洋葱皮高级设置
    if (onionSkin)
    {
        ImGui::Indent();

        bool preserveOriginalColors = context->isOnionSkinPreserveOriginalColors();
        if (ImGui::Checkbox("Preserve Original Colors", &preserveOriginalColors))
        {
            context->setOnionSkinPreserveOriginalColors(preserveOriginalColors);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Show onion skin frames with their original colors and only reduce alpha.");
        }
        
        // 前帧设置
        ImGui::TextUnformatted("Previous Frames:");
        int previousFrames = context->getOnionSkinPreviousFrames();
        if (ImGui::SliderInt("Count", &previousFrames, 1, 10)) context->setOnionSkinPreviousFrames(previousFrames);
        
        int previousAlpha = context->getOnionSkinPreviousAlpha();
        if (ImGui::SliderInt("Alpha", &previousAlpha, 10, 200)) context->setOnionSkinPreviousAlpha(previousAlpha);
        
        uint32_t previousColor = context->getOnionSkinPreviousColor();
        // 转换为 ImGui 颜色格式
        float previousColorF[3] = {
            static_cast<float>((previousColor & 0xFFu)) / 255.0f,
            static_cast<float>(((previousColor >> 8) & 0xFFu)) / 255.0f,
            static_cast<float>(((previousColor >> 16) & 0xFFu)) / 255.0f
        };
        if (ImGui::ColorEdit3("Color", previousColorF))
        {
            // 转换回 RGBA 格式
            uint32_t newColor = (
                (static_cast<uint8_t>(previousColorF[0] * 255.0f) & 0xFFu) |
                ((static_cast<uint8_t>(previousColorF[1] * 255.0f) & 0xFFu) << 8) |
                ((static_cast<uint8_t>(previousColorF[2] * 255.0f) & 0xFFu) << 16) |
                (0xFFu << 24)
            );
            context->setOnionSkinPreviousColor(newColor);
        }
        
        // 后帧设置
        ImGui::TextUnformatted("Next Frames:");
        int nextFrames = context->getOnionSkinNextFrames();
        if (ImGui::SliderInt("Count##Next", &nextFrames, 1, 10)) context->setOnionSkinNextFrames(nextFrames);
        
        int nextAlpha = context->getOnionSkinNextAlpha();
        if (ImGui::SliderInt("Alpha##Next", &nextAlpha, 10, 200)) context->setOnionSkinNextAlpha(nextAlpha);
        
        uint32_t nextColor = context->getOnionSkinNextColor();
        // 转换为 ImGui 颜色格式
        float nextColorF[3] = {
            static_cast<float>((nextColor & 0xFFu)) / 255.0f,
            static_cast<float>(((nextColor >> 8) & 0xFFu)) / 255.0f,
            static_cast<float>(((nextColor >> 16) & 0xFFu)) / 255.0f
        };
        if (ImGui::ColorEdit3("Color##Next", nextColorF))
        {
            // 转换回 RGBA 格式
            uint32_t newColor = (
                (static_cast<uint8_t>(nextColorF[0] * 255.0f) & 0xFFu) |
                ((static_cast<uint8_t>(nextColorF[1] * 255.0f) & 0xFFu) << 8) |
                ((static_cast<uint8_t>(nextColorF[2] * 255.0f) & 0xFFu) << 16) |
                (0xFFu << 24)
            );
            context->setOnionSkinNextColor(newColor);
        }
        
        ImGui::Unindent();
    }

    m_layerPanel.render(*project, *context);

    ImGui::Separator();
    ImGui::TextUnformatted("Project");
    ImGui::Text("Name: %s", project->getName().c_str());
    ImGui::Text("Size: %dx%d", project->getWidth(), project->getHeight());
    ImGui::Text("Frames: %d", project->getFrameCount());
    ImGui::Text("Total Pixels: %d", project->getWidth() * project->getHeight());

    if (m_pendingCanvasWidth <= 0 || m_pendingCanvasHeight <= 0)
    {
        m_pendingCanvasWidth = project->getWidth();
        m_pendingCanvasHeight = project->getHeight();
    }

    ImGui::InputInt("Width", &m_pendingCanvasWidth);
    ImGui::InputInt("Height", &m_pendingCanvasHeight);
    if (ImGui::Button("Apply Size") && m_pendingCanvasWidth > 0 && m_pendingCanvasHeight > 0)
    {
        project->resizeCanvas(m_pendingCanvasWidth, m_pendingCanvasHeight, 0x00000000);
        context->setProjectDirty(true, "Resize Canvas");
    }
}



