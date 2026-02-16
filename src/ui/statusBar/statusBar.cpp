#include "statusBar.h"
#include "core/AppContext.h"

#include <imgui.h>

#include <algorithm>
#include <array>

namespace
{
const std::array<const char*, 7> kToolNames = {
    "Brush",
    "Eraser",
    "Eyedropper",
    "Fill",
    "Line",
    "Rect",
    "RectFilled"
};

const char* getToolName(const AppContext& ctx)
{
    const int index = static_cast<int>(ctx.getTool());
    if (index >= 0 && index < static_cast<int>(kToolNames.size()))
    {
        return kToolNames[static_cast<size_t>(index)];
    }
    return "Unknown";
}

void drawLeft(const AppContext& ctx)
{
    const char* name = "Untitled";
    if (ctx.hasProject())
    {
        name = ctx.getProjectFilePath().empty()
            ? "Untitled"
            : ctx.getProjectFilePath().c_str();
    }

    ImGui::TextUnformatted(name);

    if (ctx.isProjectDirty())
    {
        ImGui::SameLine();
        ImGui::TextUnformatted("*");
    }
}

void drawCenter(const AppContext& ctx, float columnWidth)
{
    const char* toolName = getToolName(ctx);
    const ImVec2 textSize = ImGui::CalcTextSize(toolName);

    // 把光标移动到本列的中心位置
    const float offsetX = std::max(0.0f, (columnWidth - textSize.x) * 0.5f);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

    ImGui::Text("Tool: %s", toolName);
}

void drawRight(float columnWidth)
{
    ImGuiIO& io = ImGui::GetIO();
    const char* fmt = "FPS: %.1f";
    const ImVec2 textSize = ImGui::CalcTextSize("FPS: 999.9");

    // 右对齐：当前列宽 - 文本宽度
    const float offsetX = std::max(0.0f, columnWidth - textSize.x);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

    ImGui::Text(fmt, io.Framerate);
}
} // namespace

void statusBar::draw(const AppContext& ctx)
{
    // 核心思路：
    // 1) 每帧根据主视口(WorkPos/WorkSize)计算位置与尺寸，窗口缩放时自动更新。
    // 2) 高度使用 ImGui 的当前字体与内边距，保证 DPI/字号变化下视觉一致。
    // 3) 通过表格分三列实现左/中/右区域布局。

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImGuiStyle& style = ImGui::GetStyle();
    const float height = ImGui::GetFrameHeight() + style.WindowPadding.y * 2.0f;

    // 固定在主视口工作区底部，宽度随视口变化。
    ImGui::SetNextWindowPos(
        ImVec2(viewport->WorkPos.x,
               viewport->WorkPos.y + viewport->WorkSize.y - height));

    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, height));
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    if (!ImGui::Begin("##statusBar", nullptr, flags))
    {
        ImGui::End();
        return;
    }

    ImGuiTableFlags tableFlags =
        ImGuiTableFlags_SizingStretchProp |
        ImGuiTableFlags_NoSavedSettings |
        ImGuiTableFlags_BordersInnerV;

    if (ImGui::BeginTable("##statusBarTable", 3, tableFlags))
    {
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        drawLeft(ctx);

        ImGui::TableSetColumnIndex(1);
        drawCenter(ctx, ImGui::GetColumnWidth());

        ImGui::TableSetColumnIndex(2);
        drawRight(ImGui::GetColumnWidth());

        ImGui::EndTable();
    }

    ImGui::End();
}
