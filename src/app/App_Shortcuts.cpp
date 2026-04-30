/**
 * @file App_Shortcuts.cpp
 * @brief App 全局快捷键逻辑：File 菜单快捷键与工具切换快捷键
 */

#include "app/App.h"

#include "core/AppContext.h"
#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <string_view>

namespace
{
    // 快捷键里只需要根据路径判断保存格式；完整文件读写逻辑在 App_FileIO.cpp。
    std::string toLowerCopy(const std::string& value)
    {
        std::string result = value;
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return result;
    }

    bool endsWithInsensitive(const std::string& text, std::string_view suffix)
    {
        if (text.size() < suffix.size()) return false;
        const std::string lower = toLowerCopy(text);
        return lower.compare(lower.size() - suffix.size(), suffix.size(), suffix.data()) == 0;
    }

    App::ProjectFileFormat detectFormatFromPath(const std::string& path)
    {
        if (endsWithInsensitive(path, ".pxanim.json") || endsWithInsensitive(path, ".json")) return App::ProjectFileFormat::Json;
        return App::ProjectFileFormat::Binary;
    }
}
void App::handleFileMenuShortcuts()
{
    ImGuiIO& io = ImGui::GetIO();

    // 正在编辑文本时不处理全局文件快捷键，避免与输入冲突。
    if (io.WantTextInput) return;
    if (!io.KeyCtrl) return;

    // 优先处理带 Shift 的组合，避免与 Ctrl+S / Ctrl+W 冲突。
    if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S, false))
    {
        // Ctrl+Shift+S：Save As。
        // 若当前项目已有路径，则沿用当前格式（Binary/Json）；否则默认 Binary。
        ProjectFileFormat format = ProjectFileFormat::Binary;
        if (m_activeContext && !m_activeContext->getProjectFilePath().empty()) format = detectFormatFromPath(m_activeContext->getProjectFilePath());
        requestSaveAsDialog(format);
        return;
    }
    if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_W, false))
    {
        // Ctrl+Shift+W：Close All
        closeAllProjects();
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_N, false))
    {
        // Ctrl+N：New Project
        m_newProjectPopupRequested = true;
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_O, false))
    {
        // Ctrl+O：Open Project
        requestOpenProjectDialog();
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_S, false))
    {
        // Ctrl+S：Save
        saveActiveProject();
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_W, false))
    {
        // Ctrl+W：Close
        closeProjectByContext(m_activeContext);
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Q, false))
    {
        // Ctrl+Q：Exit
        m_done = true;
        return;
    }
}

void App::handleToolShortcuts()
{
    if (!m_activeContext) return;

    ImGuiIO& io = ImGui::GetIO();
    // 文本输入场景（例如命名弹窗输入框）不响应工具快捷键，避免误切换。
    if (io.WantTextInput) return;

    // Ctrl+D：仅在框选工具下用于清空当前像素选区。
    if (io.KeyCtrl)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_D, false)
            && m_activeContext->getTool() == ToolType::RectSelection)
        {
            m_activeContext->clearPixelSelection();
        }
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_B, false))
    {
        m_activeContext->setTool(ToolType::Brush);
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_M, false))
    {
        m_activeContext->setTool(ToolType::RectSelection);
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_E, false))
    {
        m_activeContext->setTool(ToolType::Eraser);
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_L, false))
    {
        m_activeContext->setTool(ToolType::Line);
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_I, false))
    {
        m_activeContext->setTool(ToolType::Eyedropper);
        return;
    }
}
