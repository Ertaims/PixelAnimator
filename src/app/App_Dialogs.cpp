/**
 * @file App_Dialogs.cpp
 * @brief App 通用弹窗逻辑：异步文件弹窗结果轮询与错误提示弹窗
 */

#include "app/App.h"

#include "imgui.h"

#include <mutex>
void App::pollDialogResults()
{
    std::string openPath;
    std::string savePath;
    std::string exportPath;
    std::string importPath;
    std::string dialogError;
    ProjectFileFormat saveFormat = ProjectFileFormat::Binary;
    ExportKind exportKind = ExportKind::CurrentFramePng;
    SpriteSheetExportMode exportSpriteMode = SpriteSheetExportMode::Row;
    bool exportUseSelectedFrames = false;
    int exportColumnsPerRow = 4;
    bool exportUseCustomGroups = false;
    int exportGroupSpacing = 8;
    std::vector<SpriteSheetGroupResolved> exportCustomGroups;
    ImportKind importKind = ImportKind::CurrentFramePng;
    bool importSpriteSheetRowMajor = true;
    {
        std::lock_guard<std::mutex> guard(m_dialogMutex);
        if (m_pendingOpenReady)
        {
            openPath = m_pendingOpenPath;
            m_pendingOpenPath.clear();
            m_pendingOpenReady = false;
        }
        if (m_pendingSaveReady)
        {
            savePath = m_pendingSavePath;
            m_pendingSavePath.clear();
            m_pendingSaveReady = false;
            saveFormat = m_pendingSaveFormat;
        }
        if (m_pendingDialogErrorReady)
        {
            dialogError = m_pendingDialogError;
            m_pendingDialogError.clear();
            m_pendingDialogErrorReady = false;
        }
        if (m_pendingExportReady)
        {
            exportPath = m_pendingExportPath;
            m_pendingExportPath.clear();
            m_pendingExportReady = false;
            exportKind = m_pendingExportKind;
            exportSpriteMode = m_pendingExportSpriteMode;
            exportUseSelectedFrames = m_pendingExportUseSelectedFrames;
            exportColumnsPerRow = m_pendingExportColumnsPerRow;
            exportUseCustomGroups = m_pendingExportUseCustomGroups;
            exportGroupSpacing = m_pendingExportGroupSpacing;
            exportCustomGroups = m_pendingExportCustomGroups;
            m_pendingExportCustomGroups.clear();
        }
        if (m_pendingImportReady)
        {
            importPath = m_pendingImportPath;
            m_pendingImportPath.clear();
            m_pendingImportReady = false;
            importKind = m_pendingImportKind;
            importSpriteSheetRowMajor = m_pendingImportSpriteSheetRowMajor;
        }
    }

    if (!dialogError.empty()) showError(dialogError);
    if (!openPath.empty()) openProjectFromPath(openPath);
    if (!savePath.empty()) saveActiveProjectAs(savePath, saveFormat);
    if (!exportPath.empty()) exportToPath(exportPath,
                                          exportKind,
                                          exportSpriteMode,
                                          exportUseSelectedFrames,
                                          exportColumnsPerRow,
                                          exportUseCustomGroups,
                                          exportCustomGroups,
                                          exportGroupSpacing);
    if (!importPath.empty()) importFromPath(importPath, importKind, importSpriteSheetRowMajor);
}

void App::renderErrorPopup()
{
    if (!m_pendingErrorMessage.empty())
    {
        ImGui::OpenPopup("Error");
    }

    if (!ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

    ImGui::TextWrapped("%s", m_pendingErrorMessage.c_str());
    ImGui::Separator();
    if (ImGui::Button("OK", ImVec2(120.0f, 0.0f)))
    {
        m_pendingErrorMessage.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void App::showError(const std::string& message)
{
    m_pendingErrorMessage = message;
}


