/**
 * @file App_Edit.cpp
 * @brief App 编辑命令逻辑：撤销重做、剪切复制粘贴、删除、旋转和翻转
 */

#include "app/App.h"

#include "commands/DeleteCommand.h"
#include "commands/FlipCommand.h"
#include "commands/PixelClipboardCommands.h"
#include "commands/RotateCommand.h"
#include "core/AppContext.h"
#include "imgui.h"
#include "ui/windows/ProjectWindow.h"

#include <string>
void App::handleEditMenuShortcuts()
{
    ImGuiIO& io = ImGui::GetIO();

    // Delete：Delete
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
    {
        executeDelete();
        return;
    }

    // 文本输入时不处理全局编辑快捷键，避免和输入框冲突。
    if (io.WantTextInput) return;

    // Shift+H / Shift+V：翻转当前帧或时间轴多选帧。
    // 注意：这里不要求 Ctrl，与 Edit 菜单中的快捷键标注保持一致。
    if (io.KeyShift && !io.KeyCtrl && !io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_H, false))
    {
        executeFlip(commands::FlipDirection::Horizontal);
        return;
    }
    if (io.KeyShift && !io.KeyCtrl && !io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_V, false))
    {
        executeFlip(commands::FlipDirection::Vertical);
        return;
    }

    if (!io.KeyCtrl) return;
    if (!m_activeContext) return;

    // 常见习惯：Ctrl+Shift+Z 作为 Redo（优先于 Ctrl+Z）。
    if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false))
    {
        if (m_activeContext->canRedo()) m_activeContext->redo();
        return;
    }

    // Ctrl+Z：Undo
    if (ImGui::IsKeyPressed(ImGuiKey_Z, false))
    {
        if (m_activeContext->canUndo()) m_activeContext->undo();
        return;
    }

    // Ctrl+Y：Redo
    if (ImGui::IsKeyPressed(ImGuiKey_Y, false))
    {
        if (m_activeContext->canRedo()) m_activeContext->redo();
        return;
    }

    // Ctrl+X：Cut（仅在存在像素框选时生效）
    if (ImGui::IsKeyPressed(ImGuiKey_X, false))
    {
        executeCutSelection();
        return;
    }

    // Ctrl+C：Copy（仅在存在像素框选时生效）
    if (ImGui::IsKeyPressed(ImGuiKey_C, false))
    {
        executeCopySelection();
        return;
    }

    // Ctrl+V：Paste（仅在存在像素框选时生效）
    if (ImGui::IsKeyPressed(ImGuiKey_V, false))
    {
        executePasteSelection();
        return;
    }
}

bool App::executeCutSelection()
{
    if (!m_activeContext) return false;

    std::string error;
    if (!commands::CutSelectionCommand::execute(*m_activeContext, m_pixelClipboard, &error))
    {
        if (!error.empty()) showError(error);
        return false;
    }

    m_activeContext->setProjectDirty(true, "Cut");
    return true;
}

bool App::executeCopySelection()
{
    if (!m_activeContext) return false;

    std::string error;
    if (!commands::CopySelectionCommand::execute(*m_activeContext, m_pixelClipboard, &error))
    {
        if (!error.empty()) showError(error);
        return false;
    }

    return true;
}

bool App::executeDelete()
{
    if (!m_activeContext) return false;

    std::string error;
    if (!commands::DeleteCommand::execute(*m_activeContext, &error))
    {
        if (!error.empty()) showError(error);
        return false;
    }

    return true;
}

bool App::executeRotate(commands::RotationAngle angle)
{
    if (!m_activeContext) return false;

    std::string error;
    if (!commands::RotateCommand::execute(*m_activeContext, angle, &error))
    {
        if (!error.empty()) showError(error);
        return false;
    }

    return true;
}

bool App::executeFlip(commands::FlipDirection direction)
{
    if (!m_activeContext) return false;

    std::string error;
    if (!commands::FlipCommand::execute(*m_activeContext, direction, &error))
    {
        if (!error.empty()) showError(error);
        return false;
    }

    return true;
}

bool App::executePasteSelection()
{
    if (!m_activeContext) return false;
    if (!m_pixelClipboard.isValid())
    {
        showError("Clipboard is empty.");
        return false;
    }
    const int sessionIndex = findSessionIndexByContext(m_activeContext);
    if (sessionIndex < 0) return false;

    ProjectSession& session = m_projectSessions[static_cast<size_t>(sessionIndex)];
    if (!session.window) return false;

    // Paste 进入预览模式：由画布内鼠标定位，左键确认后再真正写入像素并生成历史。
    session.window->beginPastePreview(m_pixelClipboard);
    return true;
}

void App::renderUndoHistoryPopup()
{
    if (m_undoHistoryPopupRequested)
    {
        ImGui::OpenPopup("Undo History");
        m_undoHistoryPopupRequested = false;
    }

    if (!ImGui::BeginPopup("Undo History")) return;

    if (!m_activeContext || !m_activeContext->hasProject())
    {
        ImGui::TextUnformatted("No active project.");
        ImGui::EndPopup();
        return;
    }

    const int count = m_activeContext->getUndoHistoryCount();
    const int current = m_activeContext->getUndoHistoryCurrentIndex();
    const int saved = m_activeContext->getUndoHistorySavedIndex();
    int maxEntries = m_activeContext->getUndoHistoryMaxEntries();

    if (count <= 0)
    {
        ImGui::TextUnformatted("History is empty.");
        ImGui::EndPopup();
        return;
    }

    ImGui::Text("Entries: %d", count);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::InputInt("Max", &maxEntries, 10, 50))
    {
        if (maxEntries < 1) maxEntries = 1;
        m_activeContext->setUndoHistoryMaxEntries(maxEntries);
    }
    ImGui::Separator();

    // 从新到旧显示，便于快速定位最近操作。
    for (int i = count - 1; i >= 0; --i)
    {
        std::string label = m_activeContext->getUndoHistoryLabel(i);
        if (label.empty()) label = "Edit";

        std::string displayLabel = label;
        if (i == current) displayLabel += "  [Current]";
        if (i == saved) displayLabel += "  [Saved]";

        ImGui::PushID(i);
        if (ImGui::Selectable(displayLabel.c_str(), i == current))
        {
            m_activeContext->jumpToUndoHistoryIndex(i);
        }
        ImGui::PopID();
    }

    ImGui::Separator();
    if (ImGui::MenuItem("Undo", "Ctrl+Z", false, m_activeContext->canUndo())) m_activeContext->undo();
    if (ImGui::MenuItem("Redo", "Ctrl+Y / Ctrl+Shift+Z", false, m_activeContext->canRedo())) m_activeContext->redo();
    ImGui::EndPopup();
}


