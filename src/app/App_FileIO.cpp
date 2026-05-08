/**
 * @file App_FileIO.cpp
 * @brief App 项目文件读写逻辑：打开、保存、另存为和最近项目列表
 */

#include "app/App.h"

#include "core/AppContext.h"
#include "core/Project.h"
#include "io/ProjectJsonSerializer.h"
#include "io/ProjectSerializer.h"
#include "ui/menu/menu_items/Menu_File.h"

#include <SDL3/SDL_dialog.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string_view>
#include <utility>

namespace
{
    std::vector<ProjectSerializer::FrameGroupInfo> toSerializerFrameGroups(
        const std::vector<AppContext::FrameGroup>& frameGroups)
    {
        std::vector<ProjectSerializer::FrameGroupInfo> result;
        result.reserve(frameGroups.size());
        for (const AppContext::FrameGroup& group : frameGroups)
        {
            ProjectSerializer::FrameGroupInfo info;
            info.name = group.name;
            info.frameIndices = group.frameIndices;
            info.colorRGBA = group.colorRGBA;
            result.push_back(std::move(info));
        }
        return result;
    }

    std::vector<AppContext::FrameGroup> fromSerializerFrameGroups(
        const std::vector<ProjectSerializer::FrameGroupInfo>& frameGroups)
    {
        std::vector<AppContext::FrameGroup> result;
        result.reserve(frameGroups.size());
        for (const ProjectSerializer::FrameGroupInfo& info : frameGroups)
        {
            AppContext::FrameGroup group;
            group.name = info.name;
            group.frameIndices = info.frameIndices;
            group.colorRGBA = info.colorRGBA;
            result.push_back(std::move(group));
        }
        return result;
    }

    /**
     * @brief 返回字符串的小写副本。
     *
     * 文件路径比较时使用大小写不敏感逻辑，避免同一路径因大小写差异重复进入 Recent。
     */
    std::string toLowerCopy(const std::string& value)
    {
        std::string result = value;
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return result;
    }

    /**
     * @brief 判断字符串是否以指定后缀结尾，忽略大小写。
     */
    bool endsWithInsensitive(const std::string& text, std::string_view suffix)
    {
        if (text.size() < suffix.size()) return false;
        const std::string lower = toLowerCopy(text);
        return lower.compare(lower.size() - suffix.size(), suffix.size(), suffix.data()) == 0;
    }

    /**
     * @brief 从文件扩展名推断项目保存格式。
     *
     * .pxanim.json / .json 使用 JSON 格式，其余默认使用二进制 .pxanim。
     */
    App::ProjectFileFormat detectFormatFromPath(const std::string& path)
    {
        if (endsWithInsensitive(path, ".pxanim.json") || endsWithInsensitive(path, ".json")) return App::ProjectFileFormat::Json;
        return App::ProjectFileFormat::Binary;
    }

    /**
     * @brief 判断路径是否是 PixelAnimator 支持的项目文件。
     */
    bool isSupportedProjectPath(const std::string& path)
    {
        return endsWithInsensitive(path, ".pxanim")
            || endsWithInsensitive(path, ".pxanim.json")
            || endsWithInsensitive(path, ".json");
    }

    /**
     * @brief 按目标格式补齐或转换保存路径扩展名。
     *
     * 例如用户选择 JSON 保存时，会优先生成 .pxanim.json；
     * 选择二进制保存时，会规范为 .pxanim。
     */
    std::string normalizeSavePath(const std::string& path, App::ProjectFileFormat preferredFormat)
    {
        if (path.empty()) return path;

        if (preferredFormat == App::ProjectFileFormat::Json)
        {
            if (endsWithInsensitive(path, ".pxanim.json") || endsWithInsensitive(path, ".json")) return path;
            if (endsWithInsensitive(path, ".pxanim")) return path + ".json";
            return path + ".pxanim.json";
        }

        if (endsWithInsensitive(path, ".pxanim")) return path;
        if (endsWithInsensitive(path, ".pxanim.json")) return path.substr(0, path.size() - 5);
        if (endsWithInsensitive(path, ".json")) return path.substr(0, path.size() - 5) + ".pxanim";
        return path + ".pxanim";
    }

    /**
     * @brief 从路径推导项目名。
     *
     * 会特别处理 .pxanim.json 复合扩展名，避免项目名残留 .pxanim。
     */
    std::string projectNameFromPath(const std::string& path)
    {
        try
        {
            const std::filesystem::path p(path);
            const std::string filename = p.filename().string();
            if (filename.size() > std::string(".pxanim.json").size())
            {
                const std::string lowerFilename = toLowerCopy(filename);
                if (lowerFilename.size() >= 12
                    && lowerFilename.rfind(".pxanim.json") == lowerFilename.size() - 12)
                {
                    return filename.substr(0, filename.size() - 12);
                }
            }
            const std::string stem = p.stem().string();
            if (!stem.empty()) return stem;
        }
        catch (...)
        {
        }
        return "Untitled";
    }
}
void App::requestOpenProjectDialog()
{
    if (m_openDialogInFlight) return;

    static const SDL_DialogFileFilter filters[] = {
        {"PixelAnimator Project", "pxanim"},
        {"PixelAnimator JSON Project", "pxanim.json;json"},
        {"All Files", "*"}
    };

    m_openDialogInFlight = true;
    SDL_ShowOpenFileDialog(
        &App::onOpenDialogClosed,
        this,
        m_window,
        filters,
        3,
        nullptr,
        false);
}

void App::requestSaveAsDialog(ProjectFileFormat format)
{
    if (m_saveDialogInFlight) return;

    m_saveDialogFormat = format;

    const SDL_DialogFileFilter* filters = nullptr;
    int filterCount = 0;
    static const SDL_DialogFileFilter binaryFilters[] = {
        {"PixelAnimator Binary Project", "pxanim"},
        {"All Files", "*"}
    };
    static const SDL_DialogFileFilter jsonFilters[] = {
        {"PixelAnimator JSON Project", "pxanim.json;json"},
        {"All Files", "*"}
    };
    if (format == ProjectFileFormat::Json)
    {
        filters = jsonFilters;
        filterCount = 2;
    }
    else
    {
        filters = binaryFilters;
        filterCount = 2;
    }

    const char* defaultLocation = nullptr;
    std::string candidatePath;
    if (m_activeContext)
    {
        candidatePath = m_activeContext->getProjectFilePath();
        if (candidatePath.empty())
        {
            const Project* project = m_activeContext->getProject();
            if (project && !project->getName().empty()) candidatePath = format == ProjectFileFormat::Json
                ? (project->getName() + ".pxanim.json") : (project->getName() + ".pxanim");
        }
        else
        {
            // Save As 按当前用户选择的格式预填路径，避免 JSON 模式仍默认二进制扩展名。
            candidatePath = normalizeSavePath(candidatePath, format);
        }
        if (!candidatePath.empty()) defaultLocation = candidatePath.c_str();
    }

    m_saveDialogInFlight = true;
    SDL_ShowSaveFileDialog(
        &App::onSaveDialogClosed,
        this,
        m_window,
        filters,
        filterCount,
        defaultLocation);
}

void SDLCALL App::onOpenDialogClosed(void* userdata, const char* const* filelist, int filter)
{
    (void)filter;
    App* app = static_cast<App*>(userdata);
    if (!app) return;

    std::lock_guard<std::mutex> guard(app->m_dialogMutex);
    app->m_openDialogInFlight = false;

    if (!filelist)
    {
        app->m_pendingDialogError = SDL_GetError();
        if (app->m_pendingDialogError.empty()) app->m_pendingDialogError = "Open dialog failed.";
        app->m_pendingDialogErrorReady = true;
        return;
    }

    if (!filelist[0]) return; // 用户取消

    app->m_pendingOpenPath = filelist[0];
    app->m_pendingOpenReady = true;
}

void SDLCALL App::onSaveDialogClosed(void* userdata, const char* const* filelist, int filter)
{
    (void)filter;
    App* app = static_cast<App*>(userdata);
    if (!app) return;

    std::lock_guard<std::mutex> guard(app->m_dialogMutex);
    app->m_saveDialogInFlight = false;

    if (!filelist)
    {
        app->m_pendingDialogError = SDL_GetError();
        if (app->m_pendingDialogError.empty()) app->m_pendingDialogError = "Save dialog failed.";
        app->m_pendingDialogErrorReady = true;
        return;
    }

    if (!filelist[0]) return; // 用户取消

    app->m_pendingSavePath = filelist[0];
    app->m_pendingSaveReady = true;
    app->m_pendingSaveFormat = app->m_saveDialogFormat;
}

bool App::saveProjectAs(AppContext* context, const std::string& path, ProjectFileFormat preferredFormat)
{
    if (!context || !context->hasProject())
    {
        showError("No active project to save.");
        return false;
    }
    if (path.empty())
    {
        showError("Path is empty.");
        return false;
    }

    const std::string finalPath = normalizeSavePath(path, preferredFormat);

    Project* project = context->getProject();
    if (!project)
    {
        showError("No project data to save.");
        return false;
    }

    // Save As 时把文件名（不含扩展）同步为项目名，便于窗口标题显示。
    project->setName(projectNameFromPath(finalPath));

    std::string error;
    const bool ok = preferredFormat == ProjectFileFormat::Json
        ? ProjectJsonSerializer::save(*project, finalPath, &error)
        : ProjectSerializer::save(*project, toSerializerFrameGroups(context->getFrameGroups()), finalPath, &error);
    if (!ok)
    {
        showError(error.empty() ? "Failed to save project." : error);
        return false;
    }

    context->setProjectFilePath(finalPath);
    context->setProjectDirty(false);
    addRecentProjectPath(finalPath);
    return true;
}

bool App::saveActiveProject()
{
    if (!m_activeContext || !m_activeContext->hasProject())
    {
        showError("No active project to save.");
        return false;
    }

    const std::string& path = m_activeContext->getProjectFilePath();
    if (path.empty())
    {
        requestSaveAsDialog(ProjectFileFormat::Binary);
        return false;
    }

    return saveProjectAs(m_activeContext, path, detectFormatFromPath(path));
}

bool App::saveActiveProjectAs(const std::string& path, ProjectFileFormat preferredFormat)
{
    return saveProjectAs(m_activeContext, path, preferredFormat);
}

bool App::openProjectFromPath(const std::string& path)
{
    if (path.empty())
    {
        showError("Path is empty.");
        return false;
    }

    std::string error;
    const ProjectFileFormat format = detectFormatFromPath(path);
    std::vector<ProjectSerializer::FrameGroupInfo> loadedFrameGroups;
    std::unique_ptr<Project> loadedProject = format == ProjectFileFormat::Json
        ? ProjectJsonSerializer::load(path, &error)
        : ProjectSerializer::load(path, &loadedFrameGroups, &error);
    if (!loadedProject)
    {
        showError(error.empty() ? "Failed to open project." : error);
        return false;
    }

    if (loadedProject->getName().empty()) loadedProject->setName(projectNameFromPath(path));

    const int loadedFrameCount = loadedProject->getFrameCount();
    createSessionFromProject(std::move(loadedProject), path);
    if (format == ProjectFileFormat::Binary && m_activeContext)
    {
        m_activeContext->setFrameGroups(fromSerializerFrameGroups(loadedFrameGroups), loadedFrameCount);
        // 帧分组属于撤销快照的一部分；恢复后重建打开基线，避免 Undo History 漏掉分组状态。
        m_activeContext->resetUndoRedoHistory("Open Project");
    }
    addRecentProjectPath(path);
    return true;
}

void App::addRecentProjectPath(const std::string& path)
{
    if (path.empty()) return;

    // 仅记录可再次打开的项目文件，避免把 PNG 导入/导出路径混入 Open Recent。
    if (!isSupportedProjectPath(path)) return;

    const std::string lowerPath = toLowerCopy(path);
    m_recentProjectPaths.erase(
        std::remove_if(m_recentProjectPaths.begin(),
                       m_recentProjectPaths.end(),
                       [&lowerPath](const std::string& item) {
                           return toLowerCopy(item) == lowerPath;
                       }),
        m_recentProjectPaths.end());

    m_recentProjectPaths.insert(m_recentProjectPaths.begin(), path);
    static constexpr size_t kRecentLimit = 12;
    if (m_recentProjectPaths.size() > kRecentLimit) m_recentProjectPaths.resize(kRecentLimit);

    refreshRecentProjectsMenu();
    // 每次 Recent 变更后立即落盘，避免异常退出导致数据丢失。
    saveRecentProjectPaths();
}

void App::refreshRecentProjectsMenu()
{
    if (m_fileMenu) m_fileMenu->setRecentProjectPaths(m_recentProjectPaths);
}

std::string App::getRecentProjectsStoragePath() const
{
    // MVP 方案：把 Recent 文件放在当前工作目录下，便于调试和跨平台实现。
    // 后续若需要可迁移到用户配置目录（如 AppData / ~/.config）。
    try
    {
        return (std::filesystem::current_path() / "recent_projects.txt").string();
    }
    catch (...)
    {
        // 获取失败时回退到相对路径，避免功能直接失效。
        return "recent_projects.txt";
    }
}

void App::loadRecentProjectPaths()
{
    m_recentProjectPaths.clear();
    const std::string storagePath = getRecentProjectsStoragePath();
    std::ifstream input(storagePath);
    if (!input.is_open()) return; // 首次运行或文件不存在，视为无历史记录。

    std::string line;
    static constexpr size_t kRecentLimit = 12;
    while (std::getline(input, line))
    {
        if (line.empty()) continue;
        if (!isSupportedProjectPath(line)) continue;

        // 读取阶段做去重（大小写不敏感），避免历史文件中出现重复路径。
        const std::string lowerLine = toLowerCopy(line);
        const bool existed = std::any_of(
            m_recentProjectPaths.begin(),
            m_recentProjectPaths.end(),
            [&lowerLine](const std::string& item) { return toLowerCopy(item) == lowerLine; });
        if (existed) continue;

        m_recentProjectPaths.push_back(line);
        if (m_recentProjectPaths.size() >= kRecentLimit) break;
    }
}

void App::saveRecentProjectPaths() const
{
    const std::string storagePath = getRecentProjectsStoragePath();
    std::ofstream output(storagePath, std::ios::trunc);
    if (!output.is_open()) return;

    for (const std::string& path : m_recentProjectPaths)
    {
        if (path.empty()) continue;
        output << path << '\n';
    }
}



