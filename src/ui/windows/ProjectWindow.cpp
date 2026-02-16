#include "ProjectWindow.h"

#include "core/AppContext.h"
#include "core/Project.h"
#include "imgui.h"

#include <algorithm>
#include <string>

namespace
{
ImVec4 rgbaToFloat4(uint32_t rgba)
{
    const float r = static_cast<float>((rgba >> 0) & 0xFF) / 255.0f;
    const float g = static_cast<float>((rgba >> 8) & 0xFF) / 255.0f;
    const float b = static_cast<float>((rgba >> 16) & 0xFF) / 255.0f;
    const float a = static_cast<float>((rgba >> 24) & 0xFF) / 255.0f;
    return ImVec4(r, g, b, a);
}

uint32_t float4ToRgba(const ImVec4& color)
{
    const uint32_t r = static_cast<uint32_t>(color.x * 255.0f + 0.5f) & 0xFF;
    const uint32_t g = static_cast<uint32_t>(color.y * 255.0f + 0.5f) & 0xFF;
    const uint32_t b = static_cast<uint32_t>(color.z * 255.0f + 0.5f) & 0xFF;
    const uint32_t a = static_cast<uint32_t>(color.w * 255.0f + 0.5f) & 0xFF;
    return (r << 0) | (g << 8) | (b << 16) | (a << 24);
}
} // namespace

void ProjectWindow::render()
{
    if (!visible)
        return;

    if (!ImGui::Begin(name, &visible))
    {
        ImGui::End();
        return;
    }

    if (!context || !context->hasProject())
    {
        ImGui::TextUnformatted("No project loaded.");
        ImGui::End();
        return;
    }

    Project* project = context->getProject();

    const float timelineHeight = 140.0f;
    const float availableHeight = ImGui::GetContentRegionAvail().y;
    const float topHeight = std::max(120.0f, availableHeight - timelineHeight - 8.0f);

    if (ImGui::BeginChild("##ProjectTopRegion", ImVec2(0.0f, topHeight), false))
    {
        if (ImGui::BeginTable("##ProjectMainColumns", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Left", ImGuiTableColumnFlags_WidthStretch, 0.95f);
            ImGui::TableSetupColumn("Center", ImGuiTableColumnFlags_WidthStretch, 1.8f);
            ImGui::TableSetupColumn("Right", ImGuiTableColumnFlags_WidthStretch, 0.95f);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            if (ImGui::BeginChild("##LeftPanel", ImVec2(0.0f, 0.0f), true))
            {
                ImGui::TextUnformatted("Color Picker");
                ImVec4 color = rgbaToFloat4(context->getColorRGBA());
                if (ImGui::ColorPicker4("##ProjectColorPicker", &color.x, ImGuiColorEditFlags_AlphaBar))
                {
                    context->setColorRGBA(float4ToRgba(color));
                    context->setProjectDirty(true);
                }

                ImGui::Separator();
                ImGui::TextUnformatted("Palette");
                constexpr uint32_t palette[] = {
                    0xFF000000, 0xFFFFFFFF, 0xFF404040, 0xFFC0C0C0,
                    0xFF0000FF, 0xFF00FF00, 0xFFFF0000, 0xFF00FFFF,
                    0xFFFF00FF, 0xFFFFFF00, 0xFF804000, 0xFFFFA500,
                    0xFF8B4513, 0xFF800080, 0xFF008080, 0xFF1E90FF
                };
                for (int i = 0; i < static_cast<int>(sizeof(palette) / sizeof(palette[0])); ++i)
                {
                    ImGui::PushID(i);
                    if (ImGui::ColorButton("##palette", rgbaToFloat4(palette[i]), ImGuiColorEditFlags_NoTooltip, ImVec2(24.0f, 24.0f)))
                    {
                        context->setColorRGBA(palette[i]);
                    }
                    ImGui::PopID();
                    if ((i + 1) % 4 != 0)
                        ImGui::SameLine();
                }
            }
            ImGui::EndChild();

            ImGui::TableSetColumnIndex(1);
            if (ImGui::BeginChild("##CanvasPanel", ImVec2(0.0f, 0.0f), true))
            {
                const int width = project->getWidth();
                const int height = project->getHeight();
                const int zoom = context->getCanvasZoom();
                const int frameIndex = context->getCurrentFrameIndex();
                const int frameCount = project->getFrameCount();
                ImGui::Text("Canvas  %dx%d   Zoom %dx   Frame %d/%d",
                    width, height, zoom, frameIndex + 1, frameCount);
                ImGui::Separator();

                const ImVec2 avail = ImGui::GetContentRegionAvail();
                const float drawW = std::max(1.0f, avail.x);
                const float drawH = std::max(1.0f, avail.y);
                const ImVec2 topLeft = ImGui::GetCursorScreenPos();
                const ImVec2 bottomRight = ImVec2(topLeft.x + drawW, topLeft.y + drawH);

                ImDrawList* drawList = ImGui::GetWindowDrawList();
                drawList->AddRectFilled(topLeft, bottomRight, IM_COL32(35, 35, 35, 255));
                drawList->AddRect(topLeft, bottomRight, IM_COL32(180, 180, 180, 255));
                drawList->AddText(ImVec2(topLeft.x + 12.0f, topLeft.y + 12.0f), IM_COL32(220, 220, 220, 255), "Canvas placeholder");
                ImGui::Dummy(ImVec2(drawW, drawH));
            }
            ImGui::EndChild();

            ImGui::TableSetColumnIndex(2);
            if (ImGui::BeginChild("##ToolPropsPanel", ImVec2(0.0f, 0.0f), true))
            {
                ImGui::TextUnformatted("Tool Properties");
                const char* toolNames[] = {"Brush", "Eraser", "Eyedropper", "Fill", "Line", "Rect", "RectFilled"};
                int toolIndex = static_cast<int>(context->getTool());
                if (ImGui::Combo("Tool", &toolIndex, toolNames, static_cast<int>(ToolType::Count)))
                {
                    context->setTool(static_cast<ToolType>(toolIndex));
                }

                int brushSize = context->getBrushSize();
                if (ImGui::SliderInt("Brush Size", &brushSize, 1, 32))
                {
                    context->setBrushSize(brushSize);
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

                static int newWidth = 128;
                static int newHeight = 128;
                if (newWidth <= 0 || newHeight <= 0)
                {
                    newWidth = project->getWidth();
                    newHeight = project->getHeight();
                }

                ImGui::InputInt("Width", &newWidth);
                ImGui::InputInt("Height", &newHeight);
                if (ImGui::Button("Apply Size") && newWidth > 0 && newHeight > 0)
                {
                    project->resizeCanvas(newWidth, newHeight, 0x00000000);
                    context->setProjectDirty(true);
                }
            }
            ImGui::EndChild();

            ImGui::EndTable();
        }
    }
    ImGui::EndChild();

    if (ImGui::BeginChild("##TimelinePanel", ImVec2(0.0f, 0.0f), true))
    {
        ImGui::TextUnformatted("Timeline");
        const int frameCount = project->getFrameCount();
        int current = context->getCurrentFrameIndex();
        current = std::clamp(current, 0, std::max(0, frameCount - 1));
        context->setCurrentFrameIndex(current);

        if (ImGui::Button("+ Frame"))
        {
            project->setFrameCount(frameCount + 1, 0x00000000);
            context->setCurrentFrameIndex(frameCount);
            context->setProjectDirty(true);
        }
        ImGui::SameLine();
        if (ImGui::Button("- Frame") && frameCount > 1)
        {
            project->setFrameCount(frameCount - 1);
            context->setCurrentFrameIndex(std::min(current, frameCount - 2));
            context->setProjectDirty(true);
        }
        ImGui::Separator();

        if (ImGui::BeginChild("##FrameStrip", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar))
        {
            for (int i = 0; i < project->getFrameCount(); ++i)
            {
                ImGui::PushID(i);
                const std::string label = "F" + std::to_string(i + 1);
                const bool selected = (i == context->getCurrentFrameIndex());
                if (ImGui::Selectable(label.c_str(), selected, 0, ImVec2(56.0f, 28.0f)))
                {
                    context->setCurrentFrameIndex(i);
                }
                ImGui::PopID();
                ImGui::SameLine();
            }
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();

    ImGui::End();
}
