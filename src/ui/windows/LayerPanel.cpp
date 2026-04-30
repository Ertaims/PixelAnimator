#include "LayerPanel.h"

#include "core/AppContext.h"
#include "core/Project.h"
#include "imgui.h"
#include "render/Texture.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace
{
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

    // 从当前帧采样一个代表色，用作图层列表里的小缩略色块。
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

    // 绘制棋盘底 + 平均颜色色块，让透明图层和空图层也容易区分。
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

void LayerPanel::ensureIconTextures()
{
    if (m_state.iconsLoaded) return;

    m_state.newLayerIconTexture = render::loadTextureFromFile("../src/assets/new_layer.png");
    m_state.deleteIconTexture = render::loadTextureFromFile("../src/assets/delete.png");
    m_state.upIconTexture = render::loadTextureFromFile("../src/assets/up.png");
    m_state.downIconTexture = render::loadTextureFromFile("../src/assets/down.png");
    m_state.showIconTexture = render::loadTextureFromFile("../src/assets/layer_show.png");
    m_state.hideIconTexture = render::loadTextureFromFile("../src/assets/layer_hide.png");
    m_state.lockIconTexture = render::loadTextureFromFile("../src/assets/lock.png");
    m_state.unlockIconTexture = render::loadTextureFromFile("../src/assets/unlock.png");
    m_state.iconsLoaded = true;
}

void LayerPanel::releaseTextures()
{
    render::deleteTexture(m_state.newLayerIconTexture);
    render::deleteTexture(m_state.deleteIconTexture);
    render::deleteTexture(m_state.upIconTexture);
    render::deleteTexture(m_state.downIconTexture);
    render::deleteTexture(m_state.showIconTexture);
    render::deleteTexture(m_state.hideIconTexture);
    render::deleteTexture(m_state.lockIconTexture);
    render::deleteTexture(m_state.unlockIconTexture);
    m_state.iconsLoaded = false;
}

void LayerPanel::render(Project& project, AppContext& context)
{
    ensureIconTextures();

    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.82f, 0.88f, 0.96f, 1.0f));
    ImGui::TextUnformatted("Layers");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextDisabled("%d", project.getLayerCount());

    const ImVec2 layerIconSize(22.0f, 22.0f);
    auto& selectedLayerIndices = m_state.selectedLayerIndices;

    // 图层增删/合并后索引会变化，这里每帧清理失效索引并保证至少选中当前层。
    auto sanitizeLayerSelection = [&]() {
        selectedLayerIndices.erase(
            std::remove_if(
                selectedLayerIndices.begin(),
                selectedLayerIndices.end(),
                [&](int index) { return index < 0 || index >= project.getLayerCount(); }),
            selectedLayerIndices.end());
        std::sort(selectedLayerIndices.begin(), selectedLayerIndices.end());
        selectedLayerIndices.erase(
            std::unique(selectedLayerIndices.begin(), selectedLayerIndices.end()),
            selectedLayerIndices.end());

        if (project.getLayerCount() <= 0) return;
        if (selectedLayerIndices.empty() ||
            std::find(selectedLayerIndices.begin(), selectedLayerIndices.end(), project.getActiveLayerIndex()) == selectedLayerIndices.end())
        {
            selectedLayerIndices.clear();
            selectedLayerIndices.push_back(project.getActiveLayerIndex());
        }
    };
    auto selectSingleLayer = [&](int layerIndex) {
        selectedLayerIndices.clear();
        selectedLayerIndices.push_back(layerIndex);
        project.setActiveLayerIndex(layerIndex);
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
    if (renderSmallIconButton("AddLayer", "+", "New Layer", m_state.newLayerIconTexture, layerIconSize))
    {
        project.addLayer();
        selectSingleLayer(project.getActiveLayerIndex());
        context.setProjectDirty(true, "Add Layer");
    }

    ImGui::SameLine();
    if (renderSmallIconButton(
            "DeleteLayer",
            "-",
            "Delete Active Layer",
            m_state.deleteIconTexture,
            layerIconSize,
            project.getLayerCount() > 1))
    {
        if (project.removeLayer(project.getActiveLayerIndex()))
        {
            selectSingleLayer(project.getActiveLayerIndex());
            context.setProjectDirty(true, "Delete Layer");
        }
    }

    ImGui::SameLine();
    if (renderSmallIconButton(
            "MoveLayerUp",
            "Up",
            "Move Layer Up",
            m_state.upIconTexture,
            layerIconSize,
            project.getActiveLayerIndex() < project.getLayerCount() - 1))
    {
        if (project.moveLayerUp(project.getActiveLayerIndex()))
        {
            sanitizeLayerSelection();
            context.setProjectDirty(true, "Move Layer");
        }
    }

    ImGui::SameLine();
    if (renderSmallIconButton(
            "MoveLayerDown",
            "Dn",
            "Move Layer Down",
            m_state.downIconTexture,
            layerIconSize,
            project.getActiveLayerIndex() > 0))
    {
        if (project.moveLayerDown(project.getActiveLayerIndex()))
        {
            sanitizeLayerSelection();
            context.setProjectDirty(true, "Move Layer");
        }
    }
    ImGui::PopStyleVar(2);

    int layerCount = project.getLayerCount();
    bool canMergeSelection = project.canMergeLayers(selectedLayerIndices);
    const bool mergeButtonsDisabled = !canMergeSelection;
    if (mergeButtonsDisabled)
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Merge", ImVec2(0.0f, 0.0f)))
    {
        const int mergedLayerIndex = project.mergeLayers(selectedLayerIndices, false);
        if (mergedLayerIndex >= 0)
        {
            selectSingleLayer(mergedLayerIndex);
            context.setProjectDirty(true, "Merge Layers");
            sanitizeLayerSelection();
            layerCount = project.getLayerCount();
            selectedLayerCount = static_cast<int>(selectedLayerIndices.size());
            canMergeSelection = project.canMergeLayers(selectedLayerIndices);
        }
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Merge selected contiguous layers into one layer");
    }
    ImGui::SameLine();
    if (ImGui::Button("Merge New", ImVec2(0.0f, 0.0f)))
    {
        const int mergedLayerIndex = project.mergeLayers(selectedLayerIndices, true);
        if (mergedLayerIndex >= 0)
        {
            selectSingleLayer(mergedLayerIndex);
            context.setProjectDirty(true, "Merge Layers to New");
            sanitizeLayerSelection();
            layerCount = project.getLayerCount();
            selectedLayerCount = static_cast<int>(selectedLayerIndices.size());
            canMergeSelection = project.canMergeLayers(selectedLayerIndices);
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
        context.getPrimarySelectedFrameIndex(),
        0,
        std::max(0, project.getFrameCount() - 1));
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
                Project::LayerInfo& layer = project.getLayerInfo(layerIndex);
                const bool selected = (layerIndex == project.getActiveLayerIndex());
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
                        layer.visible ? m_state.showIconTexture : m_state.hideIconTexture,
                        layerIconSize))
                {
                    project.setActiveLayerIndex(layerIndex);
                    project.setLayerVisible(layerIndex, !layer.visible);
                    context.setProjectDirty(true, layer.visible ? "Hide Layer" : "Show Layer");
                }

                ImGui::TableSetColumnIndex(1);
                if (renderSmallIconButton(
                        "LayerLock",
                        layer.locked ? "L" : "U",
                        layer.locked ? "Unlock Layer" : "Lock Layer",
                        layer.locked ? m_state.lockIconTexture : m_state.unlockIconTexture,
                        layerIconSize))
                {
                    project.setActiveLayerIndex(layerIndex);
                    project.setLayerLocked(layerIndex, !layer.locked);
                    context.setProjectDirty(true, layer.locked ? "Unlock Layer" : "Lock Layer");
                }

                ImGui::TableSetColumnIndex(2);
                bool hasPreviewPixel = false;
                const ImU32 previewColor = sampleLayerPreviewColor(project, previewFrameIndex, layerIndex, hasPreviewPixel);
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
                            project.setActiveLayerIndex(layerIndex);
                        }
                        else if (selectedLayerIndices.size() > 1)
                        {
                            const bool removedActive = (project.getActiveLayerIndex() == layerIndex);
                            selectedLayerIndices.erase(it);
                            if (removedActive)
                            {
                                project.setActiveLayerIndex(selectedLayerIndices.back());
                            }
                        }
                        else
                        {
                            project.setActiveLayerIndex(layerIndex);
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
                        project.setActiveLayerIndex(layerIndex);
                        m_state.renameLayerIndex = layerIndex;
                        std::snprintf(
                            m_state.renameLayerName,
                            sizeof(m_state.renameLayerName),
                            "%s",
                            layer.name.c_str());
                        m_state.openRenamePopup = true;
                    }
                }
                if (ImGui::BeginPopupContextItem("##LayerContextMenu"))
                {
                    ImGui::TextDisabled("%s", layer.name.c_str());
                    ImGui::Separator();

                    if (ImGui::MenuItem(layer.visible ? "Hide Layer" : "Show Layer"))
                    {
                        project.setLayerVisible(layerIndex, !layer.visible);
                        context.setProjectDirty(true, layer.visible ? "Hide Layer" : "Show Layer");
                    }
                    if (ImGui::MenuItem(layer.locked ? "Unlock Layer" : "Lock Layer"))
                    {
                        project.setLayerLocked(layerIndex, !layer.locked);
                        context.setProjectDirty(true, layer.locked ? "Unlock Layer" : "Lock Layer");
                    }

                    ImGui::Separator();
                    if (ImGui::MenuItem("Rename"))
                    {
                        m_state.renameLayerIndex = layerIndex;
                        std::snprintf(
                            m_state.renameLayerName,
                            sizeof(m_state.renameLayerName),
                            "%s",
                            layer.name.c_str());
                        m_state.openRenamePopup = true;
                    }
                    if (ImGui::MenuItem("Merge", nullptr, false, canMergeSelection))
                    {
                        const int mergedLayerIndex = project.mergeLayers(selectedLayerIndices, false);
                        if (mergedLayerIndex >= 0)
                        {
                            selectSingleLayer(mergedLayerIndex);
                            context.setProjectDirty(true, "Merge Layers");
                            sanitizeLayerSelection();
                            layerStructureChanged = true;
                        }
                    }
                    if (ImGui::MenuItem("Merge To New", nullptr, false, canMergeSelection))
                    {
                        const int mergedLayerIndex = project.mergeLayers(selectedLayerIndices, true);
                        if (mergedLayerIndex >= 0)
                        {
                            selectSingleLayer(mergedLayerIndex);
                            context.setProjectDirty(true, "Merge Layers to New");
                            sanitizeLayerSelection();
                            layerStructureChanged = true;
                        }
                    }

                    ImGui::Separator();
                    if (ImGui::MenuItem("Delete", nullptr, false, project.getLayerCount() > 1))
                    {
                        if (project.removeLayer(project.getActiveLayerIndex()))
                        {
                            selectSingleLayer(project.getActiveLayerIndex());
                            context.setProjectDirty(true, "Delete Layer");
                            sanitizeLayerSelection();
                            layerStructureChanged = true;
                        }
                    }
                    ImGui::EndPopup();
                }

                if (layerStructureChanged)
                {
                    layerCount = project.getLayerCount();
                    selectedLayerCount = static_cast<int>(selectedLayerIndices.size());
                    canMergeSelection = project.canMergeLayers(selectedLayerIndices);
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

    if (m_state.openRenamePopup)
    {
        ImGui::OpenPopup("Rename Layer");
        m_state.openRenamePopup = false;
    }

    if (ImGui::BeginPopupModal("Rename Layer", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Layer name");
        ImGui::SetNextItemWidth(220.0f);
        const bool enterPressed = ImGui::InputText(
            "##RenameLayerInput",
            m_state.renameLayerName,
            sizeof(m_state.renameLayerName),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

        if (enterPressed || ImGui::Button("OK", ImVec2(86.0f, 0.0f)))
        {
            if (m_state.renameLayerIndex >= 0 &&
                m_state.renameLayerIndex < project.getLayerCount() &&
                m_state.renameLayerName[0] != '\0')
            {
                project.renameLayer(m_state.renameLayerIndex, m_state.renameLayerName);
                context.setProjectDirty(true, "Rename Layer");
            }
            m_state.renameLayerIndex = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(86.0f, 0.0f)))
        {
            m_state.renameLayerIndex = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (project.isActiveLayerLocked())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.25f, 1.0f), "Current layer is locked.");
    }

    Project::LayerInfo& activeLayer = project.getActiveLayerInfo();
    float activeOpacityPercent = activeLayer.opacity * 100.0f;
    if (ImGui::SliderFloat("Opacity", &activeOpacityPercent, 0.0f, 100.0f, "%.0f%%"))
    {
        project.setLayerOpacity(project.getActiveLayerIndex(), activeOpacityPercent / 100.0f);
    }
    if (ImGui::IsItemDeactivatedAfterEdit())
    {
        context.setProjectDirty(true, "Layer Opacity");
    }
}
