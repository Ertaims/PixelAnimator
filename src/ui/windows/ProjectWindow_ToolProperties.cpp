#include "ProjectWindow.h"

#include "core/AppContext.h"
#include "core/Project.h"
#include "imgui.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>

namespace
{
    GLuint loadLayerPanelTextureFromFile(const char* path)
    {
        SDL_Surface* surface = IMG_Load(path);
        if (!surface) return 0;

        SDL_Surface* rgbaSurface = surface;
        if (surface->format != SDL_PIXELFORMAT_RGBA32)
        {
            rgbaSurface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
            SDL_DestroySurface(surface);
            if (!rgbaSurface) return 0;
        }

        GLuint texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, rgbaSurface->w, rgbaSurface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbaSurface->pixels);

        SDL_DestroySurface(rgbaSurface);
        return texture;
    }

    void ensureLayerPanelIconTextures(bool& iconsLoaded,
                                      unsigned int& newLayerIcon,
                                      unsigned int& deleteIcon,
                                      unsigned int& upIcon,
                                      unsigned int& downIcon,
                                      unsigned int& showIcon,
                                      unsigned int& hideIcon,
                                      unsigned int& lockIcon,
                                      unsigned int& unlockIcon)
    {
        if (iconsLoaded) return;

        newLayerIcon = loadLayerPanelTextureFromFile("../src/assets/new_layer.png");
        deleteIcon = loadLayerPanelTextureFromFile("../src/assets/delete.png");
        upIcon = loadLayerPanelTextureFromFile("../src/assets/up.png");
        downIcon = loadLayerPanelTextureFromFile("../src/assets/down.png");
        showIcon = loadLayerPanelTextureFromFile("../src/assets/layer_show.png");
        hideIcon = loadLayerPanelTextureFromFile("../src/assets/layer_hide.png");
        lockIcon = loadLayerPanelTextureFromFile("../src/assets/lock.png");
        unlockIcon = loadLayerPanelTextureFromFile("../src/assets/unlock.png");
        iconsLoaded = true;
    }

    bool renderSmallIconButton(const char* id,
                               const char* fallbackLabel,
                               const char* tooltip,
                               unsigned int icon,
                               const ImVec2& size,
                               bool enabled = true)
    {
        ImGui::PushID(id);
        if (!enabled) ImGui::BeginDisabled();

        bool clicked = false;
        if (icon != 0)
        {
            clicked = ImGui::ImageButton(
                "##icon",
                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(icon)),
                size);
        }
        else
        {
            clicked = ImGui::Button(fallbackLabel, size);
        }

        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
        if (!enabled) ImGui::EndDisabled();
        ImGui::PopID();
        return enabled && clicked;
    }

    ImU32 toPreviewColor(uint32_t rgba)
    {
        return IM_COL32(
            static_cast<int>(rgba & 0xFFu),
            static_cast<int>((rgba >> 8) & 0xFFu),
            static_cast<int>((rgba >> 16) & 0xFFu),
            255);
    }

    ImU32 sampleLayerPreviewColor(const Project& project, int frameIndex, int layerIndex, bool& outHasPixel)
    {
        outHasPixel = false;
        if (frameIndex < 0 || frameIndex >= project.getFrameCount()) return IM_COL32(90, 95, 102, 255);

        const std::vector<uint32_t>& pixels = project.getLayerPixels(frameIndex, layerIndex);
        uint64_t r = 0;
        uint64_t g = 0;
        uint64_t b = 0;
        uint64_t count = 0;
        for (uint32_t pixel : pixels)
        {
            const uint32_t alpha = (pixel >> 24) & 0xFFu;
            if (alpha == 0) continue;
            r += pixel & 0xFFu;
            g += (pixel >> 8) & 0xFFu;
            b += (pixel >> 16) & 0xFFu;
            ++count;
        }

        if (count == 0) return IM_COL32(86, 90, 96, 255);
        outHasPixel = true;
        return IM_COL32(
            static_cast<int>(r / count),
            static_cast<int>(g / count),
            static_cast<int>(b / count),
            255);
    }

    void drawLayerPreviewSquare(ImDrawList* drawList, const ImVec2& pos, float size, ImU32 color, bool hasPixel)
    {
        const ImVec2 max(pos.x + size, pos.y + size);
        drawList->AddRectFilled(pos, max, IM_COL32(54, 58, 64, 255), 3.0f);
        drawList->AddRectFilled(pos, ImVec2(pos.x + size * 0.5f, pos.y + size * 0.5f), IM_COL32(78, 82, 88, 255), 3.0f);
        drawList->AddRectFilled(ImVec2(pos.x + size * 0.5f, pos.y + size * 0.5f), max, IM_COL32(78, 82, 88, 255), 3.0f);
        if (hasPixel)
        {
            drawList->AddRectFilled(
                ImVec2(pos.x + 3.0f, pos.y + 3.0f),
                ImVec2(max.x - 3.0f, max.y - 3.0f),
                color,
                2.0f);
        }
        drawList->AddRect(pos, max, IM_COL32(158, 166, 176, 180), 3.0f);
    }
}

void ProjectWindow::renderRightPanel(Project* project)
{
    ensureLayerPanelIconTextures(
        layerPanelState_.iconsLoaded,
        layerPanelState_.newLayerIconTexture,
        layerPanelState_.deleteIconTexture,
        layerPanelState_.upIconTexture,
        layerPanelState_.downIconTexture,
        layerPanelState_.showIconTexture,
        layerPanelState_.hideIconTexture,
        layerPanelState_.lockIconTexture,
        layerPanelState_.unlockIconTexture);

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
        if (rectSelectionTool_.getSelectionShape() == RectSelectionTool::SelectionShape::MagicWand)
        {
            ImGui::BulletText("Left Click: Pick connected region (Replace)");
            ImGui::BulletText("Ctrl + Left Click: Add connected region");
            ImGui::BulletText("Right Click: Remove connected region");
            ImGui::BulletText("Drag inside selection: Move");
            ImGui::BulletText("Drag 8 handles: Resize (Ctrl = keep ratio)");
        }
        else if (rectSelectionTool_.getSelectionShape() == RectSelectionTool::SelectionShape::Lasso)
        {
            ImGui::BulletText("Left Drag: Free-form selection (Replace)");
            ImGui::BulletText("Ctrl + Left Drag: Add free-form selection");
            ImGui::BulletText("Right Drag: Remove free-form selection");
            ImGui::BulletText("Drag inside selection: Move");
            ImGui::BulletText("Drag 8 handles: Resize (Ctrl = keep ratio)");
        }
        else if (rectSelectionTool_.getSelectionShape() == RectSelectionTool::SelectionShape::PolygonLasso)
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
            toolbarState_.symmetryLeftRightIconTexture,
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
            toolbarState_.symmetryUpDownIconTexture,
            upDownSymmetry))
    {
        context->setUpDownSymmetryEnabled(!upDownSymmetry);
    }

    // 洋葱皮高级设置
    if (onionSkin)
    {
        ImGui::Indent();
        
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

    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.82f, 0.88f, 0.96f, 1.0f));
    ImGui::TextUnformatted("Layers");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextDisabled("%d", project->getLayerCount());

    const ImVec2 layerIconSize(22.0f, 22.0f);
    auto& selectedLayerIndices = layerPanelState_.selectedLayerIndices;
    auto sanitizeLayerSelection = [&]() {
        selectedLayerIndices.erase(
            std::remove_if(
                selectedLayerIndices.begin(),
                selectedLayerIndices.end(),
                [&](int index) { return index < 0 || index >= project->getLayerCount(); }),
            selectedLayerIndices.end());
        std::sort(selectedLayerIndices.begin(), selectedLayerIndices.end());
        selectedLayerIndices.erase(
            std::unique(selectedLayerIndices.begin(), selectedLayerIndices.end()),
            selectedLayerIndices.end());

        if (project->getLayerCount() <= 0) return;
        if (selectedLayerIndices.empty() ||
            std::find(selectedLayerIndices.begin(), selectedLayerIndices.end(), project->getActiveLayerIndex()) == selectedLayerIndices.end())
        {
            selectedLayerIndices.clear();
            selectedLayerIndices.push_back(project->getActiveLayerIndex());
        }
    };
    auto selectSingleLayer = [&](int layerIndex) {
        selectedLayerIndices.clear();
        selectedLayerIndices.push_back(layerIndex);
        project->setActiveLayerIndex(layerIndex);
    };
    auto isLayerSelected = [&](int layerIndex) {
        return std::find(selectedLayerIndices.begin(), selectedLayerIndices.end(), layerIndex) != selectedLayerIndices.end();
    };
    sanitizeLayerSelection();
    int selectedLayerCount = static_cast<int>(selectedLayerIndices.size());
    if (selectedLayerCount > 1)
    {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.73f, 0.84f, 0.98f, 1.0f));
        ImGui::Text("[%d selected]", selectedLayerCount);
        ImGui::PopStyleColor();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5.0f, 4.0f));
    if (renderSmallIconButton(
            "AddLayer",
            "+",
            "New Layer",
            layerPanelState_.newLayerIconTexture,
            layerIconSize))
    {
        project->addLayer();
        selectSingleLayer(project->getActiveLayerIndex());
        context->setProjectDirty(true, "Add Layer");
    }

    ImGui::SameLine();
    if (renderSmallIconButton(
            "DeleteLayer",
            "-",
            "Delete Active Layer",
            layerPanelState_.deleteIconTexture,
            layerIconSize,
            project->getLayerCount() > 1))
    {
        if (project->removeLayer(project->getActiveLayerIndex()))
        {
            selectSingleLayer(project->getActiveLayerIndex());
            context->setProjectDirty(true, "Delete Layer");
        }
    }

    ImGui::SameLine();
    if (renderSmallIconButton(
            "MoveLayerUp",
            "Up",
            "Move Layer Up",
            layerPanelState_.upIconTexture,
            layerIconSize,
            project->getActiveLayerIndex() < project->getLayerCount() - 1))
    {
        if (project->moveLayerUp(project->getActiveLayerIndex()))
        {
            sanitizeLayerSelection();
            context->setProjectDirty(true, "Move Layer");
        }
    }

    ImGui::SameLine();
    if (renderSmallIconButton(
            "MoveLayerDown",
            "Dn",
            "Move Layer Down",
            layerPanelState_.downIconTexture,
            layerIconSize,
            project->getActiveLayerIndex() > 0))
    {
        if (project->moveLayerDown(project->getActiveLayerIndex()))
        {
            sanitizeLayerSelection();
            context->setProjectDirty(true, "Move Layer");
        }
    }
    ImGui::PopStyleVar(2);

    // 图层操作按钮可能在本帧改变数量/当前层，因此列表渲染前必须重新读取。
    int layerCount = project->getLayerCount();
    bool canMergeSelection = project->canMergeLayers(selectedLayerIndices);
    const bool mergeButtonsDisabled = !canMergeSelection;
    if (mergeButtonsDisabled)
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Merge", ImVec2(0.0f, 0.0f)))
    {
        const int mergedLayerIndex = project->mergeLayers(selectedLayerIndices, false);
        if (mergedLayerIndex >= 0)
        {
            selectSingleLayer(mergedLayerIndex);
            context->setProjectDirty(true, "Merge Layers");
            sanitizeLayerSelection();
            layerCount = project->getLayerCount();
            selectedLayerCount = static_cast<int>(selectedLayerIndices.size());
            canMergeSelection = project->canMergeLayers(selectedLayerIndices);
        }
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Merge selected contiguous layers into one layer");
    }
    ImGui::SameLine();
    if (ImGui::Button("Merge New", ImVec2(0.0f, 0.0f)))
    {
        const int mergedLayerIndex = project->mergeLayers(selectedLayerIndices, true);
        if (mergedLayerIndex >= 0)
        {
            selectSingleLayer(mergedLayerIndex);
            context->setProjectDirty(true, "Merge Layers to New");
            sanitizeLayerSelection();
            layerCount = project->getLayerCount();
            selectedLayerCount = static_cast<int>(selectedLayerIndices.size());
            canMergeSelection = project->canMergeLayers(selectedLayerIndices);
        }
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Create a merged copy layer and keep original layers");
    }
    if (mergeButtonsDisabled)
    {
        if (ImGui::IsItemHovered() || ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip("Hold Ctrl to select 2 or more contiguous layers");
        }
        ImGui::EndDisabled();
    }
    if (selectedLayerCount > 1)
    {
        ImGui::SameLine();
        if (canMergeSelection) ImGui::TextDisabled("Contiguous selection");
        else ImGui::TextColored(ImVec4(0.96f, 0.74f, 0.30f, 1.0f), "Selection must be contiguous");
    }

    const int previewFrameIndex = std::clamp(
        context->getPrimarySelectedFrameIndex(),
        0,
        std::max(0, project->getFrameCount() - 1));
    const float layersHeight = std::clamp(static_cast<float>(layerCount) * 34.0f + 16.0f, 84.0f, 220.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.11f, 0.13f, 0.72f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 6.0f));
    if (ImGui::BeginChild("##LayersPanel", ImVec2(0.0f, layersHeight), true))
    {
        if (ImGui::BeginTable(
                "##LayersTable",
                4,
                ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_SizingStretchProp |
                    ImGuiTableFlags_NoSavedSettings))
        {
            ImGui::TableSetupColumn("V", ImGuiTableColumnFlags_WidthFixed, 26.0f);
            ImGui::TableSetupColumn("L", ImGuiTableColumnFlags_WidthFixed, 26.0f);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Opacity", ImGuiTableColumnFlags_WidthFixed, 46.0f);

            // UI 按“上方图层在上面”的习惯展示，因此从高索引往低索引绘制。
            bool layerStructureChanged = false;
            for (int layerIndex = layerCount - 1; layerIndex >= 0; --layerIndex)
            {
                Project::LayerInfo& layer = project->getLayerInfo(layerIndex);
                const bool selected = (layerIndex == project->getActiveLayerIndex());
                const bool multiSelected = isLayerSelected(layerIndex);
                ImGui::PushID(layerIndex);
                ImGui::TableNextRow(ImGuiTableRowFlags_None, 32.0f);
                if (selected)
                {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(45, 92, 138, 190));
                }
                else if (multiSelected)
                {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(60, 72, 92, 150));
                }
                else if (!layer.visible)
                {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(30, 32, 36, 130));
                }

                ImGui::TableSetColumnIndex(0);
                if (renderSmallIconButton(
                        "LayerVisibility",
                        layer.visible ? "V" : "H",
                        layer.visible ? "Hide Layer" : "Show Layer",
                        layer.visible ? layerPanelState_.showIconTexture : layerPanelState_.hideIconTexture,
                        layerIconSize))
                {
                    project->setActiveLayerIndex(layerIndex);
                    project->setLayerVisible(layerIndex, !layer.visible);
                    context->setProjectDirty(true, layer.visible ? "Hide Layer" : "Show Layer");
                }

                ImGui::TableSetColumnIndex(1);
                if (renderSmallIconButton(
                        "LayerLock",
                        layer.locked ? "L" : "U",
                        layer.locked ? "Unlock Layer" : "Lock Layer",
                        layer.locked ? layerPanelState_.lockIconTexture : layerPanelState_.unlockIconTexture,
                        layerIconSize))
                {
                    project->setActiveLayerIndex(layerIndex);
                    project->setLayerLocked(layerIndex, !layer.locked);
                    context->setProjectDirty(true, layer.locked ? "Unlock Layer" : "Lock Layer");
                }

                ImGui::TableSetColumnIndex(2);
                bool hasPreviewPixel = false;
                const ImU32 previewColor = sampleLayerPreviewColor(*project, previewFrameIndex, layerIndex, hasPreviewPixel);
                const ImVec2 previewPos = ImGui::GetCursorScreenPos();
                drawLayerPreviewSquare(ImGui::GetWindowDrawList(), previewPos, 20.0f, previewColor, hasPreviewPixel);
                ImGui::Dummy(ImVec2(22.0f, 22.0f));
                ImGui::SameLine();

                if (!layer.visible) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                if (selected)
                {
                    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.24f, 0.52f, 0.86f, 0.0f));
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.30f, 0.62f, 0.92f, 0.22f));
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.34f, 0.38f, 0.44f, 0.25f));
                }
                if (ImGui::Selectable(
                        layer.name.c_str(),
                        multiSelected,
                        ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap,
                        ImVec2(0.0f, 24.0f)))
                {
                    if (ImGui::GetIO().KeyCtrl)
                    {
                        const auto it = std::find(selectedLayerIndices.begin(), selectedLayerIndices.end(), layerIndex);
                        if (it == selectedLayerIndices.end())
                        {
                            selectedLayerIndices.push_back(layerIndex);
                            std::sort(selectedLayerIndices.begin(), selectedLayerIndices.end());
                            project->setActiveLayerIndex(layerIndex);
                        }
                        else if (selectedLayerIndices.size() > 1)
                        {
                            const bool removedActive = (project->getActiveLayerIndex() == layerIndex);
                            selectedLayerIndices.erase(it);
                            if (removedActive)
                            {
                                project->setActiveLayerIndex(selectedLayerIndices.back());
                            }
                        }
                        else
                        {
                            project->setActiveLayerIndex(layerIndex);
                        }
                    }
                    else
                    {
                        selectSingleLayer(layerIndex);
                    }
                }
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && !multiSelected)
                {
                    selectSingleLayer(layerIndex);
                }
                if (selected) ImGui::PopStyleColor(2);
                else ImGui::PopStyleColor();
                if (!layer.visible) ImGui::PopStyleColor();
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Double-click to rename\n%s%s", layer.name.c_str(), layer.locked ? " (Locked)" : "");
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        project->setActiveLayerIndex(layerIndex);
                        layerPanelState_.renameLayerIndex = layerIndex;
                        std::snprintf(layerPanelState_.renameLayerName,
                                      sizeof(layerPanelState_.renameLayerName),
                                      "%s",
                                      layer.name.c_str());
                        layerPanelState_.openRenamePopup = true;
                    }
                }
                if (ImGui::BeginPopupContextItem("##LayerContextMenu"))
                {
                    ImGui::TextDisabled("%s", layer.name.c_str());
                    ImGui::Separator();

                    if (ImGui::MenuItem(layer.visible ? "Hide Layer" : "Show Layer"))
                    {
                        project->setLayerVisible(layerIndex, !layer.visible);
                        context->setProjectDirty(true, layer.visible ? "Hide Layer" : "Show Layer");
                    }
                    if (ImGui::MenuItem(layer.locked ? "Unlock Layer" : "Lock Layer"))
                    {
                        project->setLayerLocked(layerIndex, !layer.locked);
                        context->setProjectDirty(true, layer.locked ? "Unlock Layer" : "Lock Layer");
                    }

                    ImGui::Separator();
                    if (ImGui::MenuItem("Rename"))
                    {
                        layerPanelState_.renameLayerIndex = layerIndex;
                        std::snprintf(layerPanelState_.renameLayerName,
                                      sizeof(layerPanelState_.renameLayerName),
                                      "%s",
                                      layer.name.c_str());
                        layerPanelState_.openRenamePopup = true;
                    }
                    if (ImGui::MenuItem("Merge", nullptr, false, canMergeSelection))
                    {
                        const int mergedLayerIndex = project->mergeLayers(selectedLayerIndices, false);
                        if (mergedLayerIndex >= 0)
                        {
                            selectSingleLayer(mergedLayerIndex);
                            context->setProjectDirty(true, "Merge Layers");
                            sanitizeLayerSelection();
                            layerStructureChanged = true;
                        }
                    }
                    if (ImGui::MenuItem("Merge To New", nullptr, false, canMergeSelection))
                    {
                        const int mergedLayerIndex = project->mergeLayers(selectedLayerIndices, true);
                        if (mergedLayerIndex >= 0)
                        {
                            selectSingleLayer(mergedLayerIndex);
                            context->setProjectDirty(true, "Merge Layers to New");
                            sanitizeLayerSelection();
                            layerStructureChanged = true;
                        }
                    }

                    ImGui::Separator();
                    if (ImGui::MenuItem("Delete", nullptr, false, project->getLayerCount() > 1))
                    {
                        if (project->removeLayer(project->getActiveLayerIndex()))
                        {
                            selectSingleLayer(project->getActiveLayerIndex());
                            context->setProjectDirty(true, "Delete Layer");
                            sanitizeLayerSelection();
                            layerStructureChanged = true;
                        }
                    }
                    ImGui::EndPopup();
                }

                if (layerStructureChanged)
                {
                    layerCount = project->getLayerCount();
                    selectedLayerCount = static_cast<int>(selectedLayerIndices.size());
                    canMergeSelection = project->canMergeLayers(selectedLayerIndices);
                    ImGui::PopID();
                    break;
                }

                ImGui::TableSetColumnIndex(3);
                const int opacityPercent = static_cast<int>(std::round(layer.opacity * 100.0f));
                if (selected) ImGui::TextColored(ImVec4(0.82f, 0.90f, 1.0f, 1.0f), "%d%%", opacityPercent);
                else ImGui::TextDisabled("%d%%", opacityPercent);

                ImGui::PopID();
            }

            ImGui::EndTable();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    if (layerPanelState_.openRenamePopup)
    {
        ImGui::OpenPopup("Rename Layer");
        layerPanelState_.openRenamePopup = false;
    }

    if (ImGui::BeginPopupModal("Rename Layer", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Layer name");
        ImGui::SetNextItemWidth(220.0f);
        const bool enterPressed = ImGui::InputText(
            "##RenameLayerInput",
            layerPanelState_.renameLayerName,
            sizeof(layerPanelState_.renameLayerName),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

        if (enterPressed || ImGui::Button("OK", ImVec2(86.0f, 0.0f)))
        {
            if (layerPanelState_.renameLayerIndex >= 0 &&
                layerPanelState_.renameLayerIndex < project->getLayerCount() &&
                layerPanelState_.renameLayerName[0] != '\0')
            {
                project->renameLayer(layerPanelState_.renameLayerIndex, layerPanelState_.renameLayerName);
                context->setProjectDirty(true, "Rename Layer");
            }
            layerPanelState_.renameLayerIndex = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(86.0f, 0.0f)))
        {
            layerPanelState_.renameLayerIndex = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (project->isActiveLayerLocked())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.25f, 1.0f), "Current layer is locked.");
    }

    Project::LayerInfo& activeLayer = project->getActiveLayerInfo();
    float activeOpacityPercent = activeLayer.opacity * 100.0f;
    if (ImGui::SliderFloat("Opacity", &activeOpacityPercent, 0.0f, 100.0f, "%.0f%%"))
    {
        project->setLayerOpacity(project->getActiveLayerIndex(), activeOpacityPercent / 100.0f);
    }
    if (ImGui::IsItemDeactivatedAfterEdit())
    {
        context->setProjectDirty(true, "Layer Opacity");
    }

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
        context->setProjectDirty(true, "Resize Canvas");
    }
}
