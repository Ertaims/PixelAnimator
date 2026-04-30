/**
 * @file App_Project.cpp
 * @brief App 项目会话逻辑：新建项目、关闭项目、多窗口切换和窗口标题刷新
 */

#include "app/App.h"

#include "core/AppContext.h"
#include "core/Project.h"
#include "imgui.h"
#include "ui/menu/menu_items/Menu_Edit.h"
#include "ui/menu/menu_items/Menu_File.h"
#include "ui/windows/ProjectWindow.h"
#include "ui/windows/WindowFactory.h"

#include <algorithm>
#include <cmath>

namespace
{
    /**
     * @brief 将 ImGui 颜色转换为项目内部使用的 RGBA8888。
     *
     * 新建项目弹窗里用户选择的是 float4，这里统一打包成像素填充色。
     */
    uint32_t float4ToRgba(const ImVec4& color)
    {
        const uint32_t r = static_cast<uint32_t>(std::round(color.x * 255.0f)) & 0xFF;
        const uint32_t g = static_cast<uint32_t>(std::round(color.y * 255.0f)) & 0xFF;
        const uint32_t b = static_cast<uint32_t>(std::round(color.z * 255.0f)) & 0xFF;
        const uint32_t a = static_cast<uint32_t>(std::round(color.w * 255.0f)) & 0xFF;
        return (r << 0) | (g << 8) | (b << 16) | (a << 24);
    }
}
void App::setActiveContext(AppContext* context)
{
    // 统一切换“当前活动上下文”，菜单命令也同步切换目标
    m_activeContext = context;
    if (m_fileMenu) m_fileMenu->setContext(m_activeContext);
    if (m_editMenu) m_editMenu->setContext(m_activeContext);
}

int App::findSessionIndexByContext(const AppContext* context) const
{
    if (!context) return -1;

    for (int i = 0; i < static_cast<int>(m_projectSessions.size()); ++i)
    {
        if (m_projectSessions[static_cast<size_t>(i)].context.get() == context) return i;
    }
    return -1;
}

void App::closeProjectByContext(AppContext* context)
{
    const int index = findSessionIndexByContext(context);
    if (index < 0) return;

    ProjectSession& session = m_projectSessions[static_cast<size_t>(index)];
    Window* windowToDestroy = session.window;
    if (windowToDestroy) WindowFactory::getInstance().destroyWindow(windowToDestroy);

    m_projectSessions.erase(m_projectSessions.begin() + index);
    m_dockLayoutInitialized = false;

    // 全部关闭后，活动上下文清空
    if (m_projectSessions.empty())
    {
        setActiveContext(nullptr);
        return;
    }

    // 关闭后激活相邻会话
    const int newIndex = std::min(index, static_cast<int>(m_projectSessions.size()) - 1);
    AppContext* newActive = m_projectSessions[static_cast<size_t>(newIndex)].context.get();
    setActiveContext(newActive);
    ImGui::SetWindowFocus(m_projectSessions[static_cast<size_t>(newIndex)].windowLabel.c_str());
}

void App::closeAllProjects()
{
    for (ProjectSession& session : m_projectSessions)
    {
        if (session.window) WindowFactory::getInstance().destroyWindow(session.window);
    }

    m_projectSessions.clear();
    setActiveContext(nullptr);
    m_dockLayoutInitialized = false;
}

void App::refreshWindowLabels()
{
    // 窗口标题展示项目名 + 脏标记：Name*
    for (ProjectSession& session : m_projectSessions)
    {
        if (session.project && !session.project->getName().empty()) session.windowBaseTitle = session.project->getName();

        const bool dirty = session.context && session.context->isProjectDirty();
        std::string label = session.windowBaseTitle;
        if (dirty) label += "*";
        label += "###ProjectWindow_" + std::to_string(session.projectId);

        if (session.windowLabel == label) continue;

        session.windowLabel = label;
        if (session.window) session.window->setWindowLabel(session.windowLabel);
    }
}

void App::handleProjectSwitchShortcut()
{
    // 少于两个会话时不需要切换
    if (m_projectSessions.size() < 2) return;

    ImGuiIO& io = ImGui::GetIO();
    if (!io.KeyCtrl) return;
    if (!ImGui::IsKeyPressed(ImGuiKey_Tab, false)) return;

    int currentIndex = findSessionIndexByContext(m_activeContext);
    if (currentIndex < 0) currentIndex = 0;

    const int n = static_cast<int>(m_projectSessions.size());
    const bool backward = io.KeyShift; // Ctrl+Shift+Tab 反向
    const int nextIndex = backward
        ? (currentIndex - 1 + n) % n
        : (currentIndex + 1) % n;

    ProjectSession& nextSession = m_projectSessions[static_cast<size_t>(nextIndex)];
    setActiveContext(nextSession.context.get());
    ImGui::SetWindowFocus(nextSession.windowLabel.c_str());
}

void App::renderNewProjectPopup()
{
    // 菜单中点击 New 后，仅设置请求标志；真正 OpenPopup 放在渲染帧中执行
    if (m_newProjectPopupRequested)
    {
        ImGui::OpenPopup("New Project");
        m_newProjectPopupRequested = false;
    }

    if (!ImGui::BeginPopupModal("New Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

    ImGui::TextUnformatted("Create a new project");
    ImGui::Separator();

    ImGui::SetNextItemWidth(140.0f);
    ImGui::InputInt("Width", &m_newProjectWidth);
    ImGui::SetNextItemWidth(140.0f);
    ImGui::InputInt("Height", &m_newProjectHeight);
    ImGui::SetNextItemWidth(140.0f);
    ImGui::InputInt("Frames", &m_newProjectFrameCount);
    ImGui::ColorEdit4("Background", &m_newProjectBgColor.x);

    // 可选画布底纹：棋盘 / 纯白
    const char* canvasBgItems[] = {"Checkerboard", "White"};
    ImGui::SetNextItemWidth(140.0f);
    ImGui::Combo("Canvas Background", &m_newProjectCanvasBgMode, canvasBgItems, 2);

    // 输入兜底，避免非法参数
    if (m_newProjectWidth < 1) m_newProjectWidth = 1;
    if (m_newProjectHeight < 1) m_newProjectHeight = 1;
    if (m_newProjectFrameCount < 1) m_newProjectFrameCount = 1;

    ImGui::Separator();
    if (ImGui::Button("Create", ImVec2(120.0f, 0.0f)))
    {
        createNewProject(
            m_newProjectWidth,
            m_newProjectHeight,
            m_newProjectFrameCount,
            float4ToRgba(m_newProjectBgColor),
            m_newProjectCanvasBgMode == 0);
        ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
    {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void App::createSessionFromProject(std::unique_ptr<Project> project, const std::string& projectPath)
{
    ProjectSession session;
    session.project = std::move(project);
    session.context = std::make_unique<AppContext>();
    session.context->setProject(session.project.get());
    session.context->setProjectFilePath(projectPath);
    session.context->setProjectDirty(false);
    session.context->setCurrentAnimationIndex(0);
    session.context->setCurrentFrameIndex(0);
    // 会话初始化时明确为单选第 0 帧，避免出现空选区。
    session.context->setSingleFrameSelection(0, session.project ? session.project->getFrameCount() : 1);
    session.context->setCanvasPan(0.0f, 0.0f);
    session.context->setCanvasZoom(4);
    session.context->setCheckerboardBackgroundEnabled(true);
    // 打开项目后，以当前状态作为撤销基线。
    session.context->resetUndoRedoHistory("Open Project");

    session.projectId = m_nextProjectId++;
    session.windowBaseTitle = session.project && !session.project->getName().empty()
        ? session.project->getName()
        : "Untitled";
    session.windowLabel = session.windowBaseTitle + "###ProjectWindow_" + std::to_string(session.projectId);

    AppContext* rawContext = session.context.get();
    session.window = WindowFactory::getInstance().createProjectWindow(
        rawContext,
        session.windowLabel,
        [this](AppContext* focusedContext) { setActiveContext(focusedContext); });
    session.window->setVisible(true);

    m_projectSessions.push_back(std::move(session));
    setActiveContext(rawContext);
    m_dockLayoutInitialized = false;
}

void App::createNewProject(int width, int height, int frameCount, uint32_t fillColor, bool checkerboardBackground)
{
    ProjectSession session;

    // 为新窗口创建独立项目数据 + 独立上下文
    session.project = std::make_unique<Project>(width, height, frameCount, fillColor);
    session.context = std::make_unique<AppContext>();
    session.context->setProject(session.project.get());
    session.context->setProjectFilePath("");
    session.context->setProjectDirty(false);
    session.context->setCurrentAnimationIndex(0);
    session.context->setCurrentFrameIndex(0);
    // 新建项目默认选中第 0 帧。
    session.context->setSingleFrameSelection(0, session.project ? session.project->getFrameCount() : 1);
    session.context->setCanvasPan(0.0f, 0.0f);
    session.context->setCanvasZoom(4);
    session.context->setCheckerboardBackgroundEnabled(checkerboardBackground);
    // 新建项目后，以当前空白状态作为撤销基线。
    session.context->resetUndoRedoHistory("New Project");

    // 生成唯一窗口标题/ID
    session.projectId = m_nextProjectId++;
    session.windowBaseTitle = session.project && !session.project->getName().empty()
        ? session.project->getName()
        : "Untitled";
    session.windowLabel = session.windowBaseTitle + "###ProjectWindow_" + std::to_string(session.projectId);

    AppContext* rawContext = session.context.get();
    session.window = WindowFactory::getInstance().createProjectWindow(
        rawContext,
        session.windowLabel,
        [this](AppContext* focusedContext) { setActiveContext(focusedContext); });
    session.window->setVisible(true);

    m_projectSessions.push_back(std::move(session));
    setActiveContext(rawContext);

    // 下帧重建 dock，确保新窗口可见
    m_dockLayoutInitialized = false;
}
