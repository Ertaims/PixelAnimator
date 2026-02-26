#include "ProjectWindow.h"

#include "core/AppContext.h"
#include "core/Project.h"
#include "imgui.h"

#include <cstdint>
#include <vector>

namespace
{
// RGBA8888 -> ImGui float4
ImVec4 rgbaToFloat4(uint32_t rgba)
{
    const float r = static_cast<float>((rgba >> 0) & 0xFF) / 255.0f;
    const float g = static_cast<float>((rgba >> 8) & 0xFF) / 255.0f;
    const float b = static_cast<float>((rgba >> 16) & 0xFF) / 255.0f;
    const float a = static_cast<float>((rgba >> 24) & 0xFF) / 255.0f;
    return ImVec4(r, g, b, a);
}

// ImGui float4 -> RGBA8888
uint32_t float4ToRgba(const ImVec4& color)
{
    const uint32_t r = static_cast<uint32_t>(color.x * 255.0f + 0.5f) & 0xFF;
    const uint32_t g = static_cast<uint32_t>(color.y * 255.0f + 0.5f) & 0xFF;
    const uint32_t b = static_cast<uint32_t>(color.z * 255.0f + 0.5f) & 0xFF;
    const uint32_t a = static_cast<uint32_t>(color.w * 255.0f + 0.5f) & 0xFF;
    return (r << 0) | (g << 8) | (b << 16) | (a << 24);
}
} // namespace

void ProjectWindow::renderLeftPanel(Project* project)
{
    (void)project;

    static const uint32_t kDefaultPalette[] = {
        0xFF000000, 0xFFFFFFFF, 0xFF404040, 0xFFC0C0C0,
        0xFF0000FF, 0xFF00FF00, 0xFFFF0000, 0xFF00FFFF,
        0xFFFF00FF, 0xFFFFFF00, 0xFF804000, 0xFFFFA500,
        0xFF8B4513, 0xFF800080, 0xFF008080, 0xFF1E90FF
    };
    std::vector<uint32_t>& userPalette = paletteState_.userPalette;
    int& selectedIndex = paletteState_.selectedIndex;
    bool& selectedIsUser = paletteState_.selectedIsUser;

    const int defaultCount = static_cast<int>(sizeof(kDefaultPalette) / sizeof(kDefaultPalette[0]));
    const int userCount = static_cast<int>(userPalette.size());

    if (!selectedIsUser)
    {
        if (selectedIndex < 0 || selectedIndex >= defaultCount)
            selectedIndex = 0;
    }
    else
    {
        if (userCount == 0)
        {
            selectedIsUser = false;
            selectedIndex = 0;
        }
        else if (selectedIndex < 0 || selectedIndex >= userCount)
        {
            selectedIndex = 0;
        }
    }

    const uint32_t selectedColor = selectedIsUser
        ? userPalette[static_cast<size_t>(selectedIndex)]
        : kDefaultPalette[selectedIndex];

    ImGui::TextUnformatted("Color Picker");
    ImVec4 color = rgbaToFloat4(selectedColor);
    if (ImGui::ColorPicker4("##ProjectColorPicker", &color.x, ImGuiColorEditFlags_AlphaBar))
    {
        const uint32_t newColor = float4ToRgba(color);
        if (selectedIsUser && !userPalette.empty())
            userPalette[static_cast<size_t>(selectedIndex)] = newColor;
        context->setColorRGBA(newColor);
        context->setProjectDirty(true);
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Palette - Default");
    for (int i = 0; i < defaultCount; ++i)
    {
        const bool isSelected = (!selectedIsUser && selectedIndex == i);
        ImGui::PushID(i);
        if (ImGui::ColorButton("##palette_default", rgbaToFloat4(kDefaultPalette[i]), ImGuiColorEditFlags_NoTooltip, ImVec2(24.0f, 24.0f)))
        {
            selectedIsUser = false;
            selectedIndex = i;
            context->setColorRGBA(kDefaultPalette[i]);
        }
        if (isSelected)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRect(
                ImGui::GetItemRectMin(),
                ImGui::GetItemRectMax(),
                IM_COL32(255, 220, 40, 255),
                2.0f,
                0,
                2.0f);
        }
        ImGui::PopID();
        if ((i + 1) % 4 != 0)
            ImGui::SameLine();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Palette - User");
    if (userPalette.empty())
    {
        ImGui::TextUnformatted("No user colors yet.");
    }
    else
    {
        for (int i = 0; i < static_cast<int>(userPalette.size()); ++i)
        {
            const bool isSelected = (selectedIsUser && selectedIndex == i);
            ImGui::PushID(i);
            if (ImGui::ColorButton("##palette_user", rgbaToFloat4(userPalette[static_cast<size_t>(i)]), ImGuiColorEditFlags_NoTooltip, ImVec2(24.0f, 24.0f)))
            {
                selectedIsUser = true;
                selectedIndex = i;
                context->setColorRGBA(userPalette[static_cast<size_t>(i)]);
            }
            if (isSelected)
            {
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                drawList->AddRect(
                    ImGui::GetItemRectMin(),
                    ImGui::GetItemRectMax(),
                    IM_COL32(255, 220, 40, 255),
                    2.0f,
                    0,
                    2.0f);
            }
            ImGui::PopID();
            if ((i + 1) % 4 != 0)
                ImGui::SameLine();
        }
    }

    ImGui::Separator();
    if (ImGui::Button("+ Add Color"))
    {
        const uint32_t newColor = context->getColorRGBA();
        userPalette.push_back(newColor);
        selectedIsUser = true;
        selectedIndex = static_cast<int>(userPalette.size()) - 1;
        context->setProjectDirty(true);
    }
}
