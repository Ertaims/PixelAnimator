/**
 * @file App.cpp
 * @brief App 主流程实现：初始化、主循环渲染、资源释放、多项目会话切换
 */

#include "app/App.h"

#include "core/AppContext.h"
#include "core/Project.h"
#include "io/ProjectSerializer.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"
#include "imgui_internal.h"

#include "ui/menu/MenuFactory.h"
#include "ui/menu/menu_items/Menu_Edit.h"
#include "ui/menu/menu_items/Menu_File.h"
#include "ui/windows/ProjectWindow.h"
#include "ui/windows/Window.h"
#include "ui/windows/WindowFactory.h"

#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_opengl.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iostream>

namespace
{
    // 将 ImGui 的 float4 颜色转换为 RGBA8888（R 在低字节）
    uint32_t float4ToRgba(const ImVec4& color)
    {
        const uint32_t r = static_cast<uint32_t>(std::round(color.x * 255.0f)) & 0xFF;
        const uint32_t g = static_cast<uint32_t>(std::round(color.y * 255.0f)) & 0xFF;
        const uint32_t b = static_cast<uint32_t>(std::round(color.z * 255.0f)) & 0xFF;
        const uint32_t a = static_cast<uint32_t>(std::round(color.w * 255.0f)) & 0xFF;
        return (r << 0) | (g << 8) | (b << 16) | (a << 24);
    }

    std::string projectNameFromPath(const std::string& path)
    {
        try
        {
            const std::filesystem::path p(path);
            const std::string stem = p.stem().string();
            if (!stem.empty())
                return stem;
        }
        catch (...)
        {
        }
        return "Untitled";
    }
}

App::App() = default;
App::~App() = default;

bool App::init()
{
    // 1) 初始化 SDL
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        std::fprintf(stderr, "Error: SDL_Init(): %s\n", SDL_GetError());
        return false;
    }

    // 2) 创建主窗口与 OpenGL 上下文
    if (!createWindowAndContext())
        return false;

    // 3) 初始化 ImGui
    if (!initImGui())
        return false;

    // 4) 创建菜单与默认项目
    createMenuAndWindows();

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    return true;
}

AppContext& App::getContext()
{
    // 约束：调用方只能在存在活跃项目时访问
    assert(activeContext_ && "No active project context.");
    return *activeContext_;
}

const AppContext& App::getContext() const
{
    assert(activeContext_ && "No active project context.");
    return *activeContext_;
}

bool App::createWindowAndContext()
{
    mainScale_ = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_WindowFlags windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
        | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    window_ = SDL_CreateWindow("Pixel Animator",
                               static_cast<int>(1280 * mainScale_),
                               static_cast<int>(800 * mainScale_),
                               windowFlags);
    if (!window_)
    {
        std::fprintf(stderr, "Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return false;
    }

    glContext_ = SDL_GL_CreateContext(window_);
    if (!glContext_)
    {
        std::fprintf(stderr, "Error: SDL_GL_CreateContext(): %s\n", SDL_GetError());
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        return false;
    }

    SDL_GL_MakeCurrent(window_, glContext_);
    SDL_GL_SetSwapInterval(1);
    SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window_);
    return true;
}

bool App::initImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.ItemSpacing.x = 8.0f;
    style.ScaleAllSizes(mainScale_);
    style.FontScaleDpi = mainScale_;
    io.ConfigDpiScaleFonts = true;
    io.ConfigDpiScaleViewports = true;

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui_ImplSDL3_InitForOpenGL(window_, glContext_);
    ImGui_ImplOpenGL3_Init(glslVersion_);
    return true;
}

void App::createMenuAndWindows()
{
    ConcreteMenuFactory menuFactory;
    menuManager_ = menuFactory.createMenuManager();

    // File 菜单回调：
    // - New: 打开 New Project 弹窗
    // - Close / Close All: 关闭当前或全部会话
    // - Exit: 退出主循环
    fileMenu_ = menuFactory.createFileMenu(
        menuManager_,
        nullptr,
        [this]() { done_ = true; },
        [this]() { newProjectPopupRequested_ = true; },
        [this]() { requestOpenProjectDialog(); },
        [this]() { saveActiveProject(); },
        [this]() { requestSaveAsDialog(); },
        [this]() { closeProjectByContext(activeContext_); },
        [this]() { closeAllProjects(); });

    editMenu_ = menuFactory.createEditMenu(menuManager_, nullptr);
    menuFactory.createViewMenu(menuManager_);
    menuFactory.createHelpMenu(menuManager_);

    // 启动时默认创建一个项目，保证界面可用
    createNewProject(16, 16, 1, 0x00000000);
}

void App::run()
{
    while (!done_)
    {
        processEvents();

        // 最小化时降低 CPU 占用
        if (SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            continue;
        }

        renderFrame();
        SDL_GL_SwapWindow(window_);
    }
}

void App::processEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type == SDL_EVENT_QUIT)
            done_ = true;
        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED
            && event.window.windowID == SDL_GetWindowID(window_))
            done_ = true;
    }
}

void App::renderFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();

    if (menuManager_)
        menuManager_->render();

    // 每帧更新：New Project 弹窗、快捷切换、窗口标题脏标记
    pollDialogResults();
    renderNewProjectPopup();
    renderErrorPopup();
    handleProjectSwitchShortcut();
    refreshWindowLabels();

    // 全屏 DockSpace 容器，所有工具窗口停靠其中
    {
        static bool opt_fullscreen = true;
        static bool opt_padding = false;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
        if (opt_fullscreen)
        {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
                | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        }
        else
        {
            dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
        }

        if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
            window_flags |= ImGuiWindowFlags_NoBackground;

        if (!opt_padding)
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace Demo", nullptr, window_flags);
        if (!opt_padding)
            ImGui::PopStyleVar();

        if (opt_fullscreen)
            ImGui::PopStyleVar(2);

        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
            setupDefaultDockLayout();
        }
        ImGui::End();
    }

    // 渲染所有注册窗口（包括多个 ProjectWindow）
    for (Window* window : WindowFactory::getInstance().getWindows())
    {
        if (window)
            window->render();
    }

    ImGui::Render();
    glViewport(0, 0, static_cast<int>(io.DisplaySize.x), static_cast<int>(io.DisplaySize.y));
    glClearColor(clearColor_.x * clearColor_.w, clearColor_.y * clearColor_.w,
                 clearColor_.z * clearColor_.w, clearColor_.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        SDL_Window* backup_window = SDL_GL_GetCurrentWindow();
        SDL_GLContext backup_context = SDL_GL_GetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        SDL_GL_MakeCurrent(backup_window, backup_context);
    }
}

void App::shutdown()
{
    // 先销毁窗口，再清空会话，防止悬空指针
    WindowFactory::getInstance().cleanup();
    projectSessions_.clear();
    activeContext_ = nullptr;

    if (menuManager_)
    {
        delete menuManager_;
        menuManager_ = nullptr;
    }
    fileMenu_ = nullptr;
    editMenu_ = nullptr;

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    if (glContext_ && window_)
    {
        SDL_GL_DestroyContext(glContext_);
        glContext_ = nullptr;
    }
    if (window_)
    {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

void App::setupDefaultDockLayout()
{
    // 仅在需要时重建布局（新建/关闭会话后会置 false）
    if (dockLayoutInitialized_)
        return;

    ImGuiID dockspaceId = ImGui::GetID("MyDockSpace");
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

    for (const ProjectSession& session : projectSessions_)
    {
        if (session.window)
            ImGui::DockBuilderDockWindow(session.windowLabel.c_str(), dockspaceId);
    }

    ImGui::DockBuilderFinish(dockspaceId);
    dockLayoutInitialized_ = true;
}

void App::setActiveContext(AppContext* context)
{
    // 统一切换“当前活动上下文”，菜单命令也同步切换目标
    activeContext_ = context;
    if (fileMenu_)
        fileMenu_->setContext(activeContext_);
    if (editMenu_)
        editMenu_->setContext(activeContext_);
}

int App::findSessionIndexByContext(const AppContext* context) const
{
    if (!context)
        return -1;

    for (int i = 0; i < static_cast<int>(projectSessions_.size()); ++i)
    {
        if (projectSessions_[static_cast<size_t>(i)].context.get() == context)
            return i;
    }
    return -1;
}

void App::closeProjectByContext(AppContext* context)
{
    const int index = findSessionIndexByContext(context);
    if (index < 0)
        return;

    ProjectSession& session = projectSessions_[static_cast<size_t>(index)];
    Window* windowToDestroy = session.window;
    if (windowToDestroy)
        WindowFactory::getInstance().destroyWindow(windowToDestroy);

    projectSessions_.erase(projectSessions_.begin() + index);
    dockLayoutInitialized_ = false;

    // 全部关闭后，活动上下文清空
    if (projectSessions_.empty())
    {
        setActiveContext(nullptr);
        return;
    }

    // 关闭后激活相邻会话
    const int newIndex = std::min(index, static_cast<int>(projectSessions_.size()) - 1);
    AppContext* newActive = projectSessions_[static_cast<size_t>(newIndex)].context.get();
    setActiveContext(newActive);
    ImGui::SetWindowFocus(projectSessions_[static_cast<size_t>(newIndex)].windowLabel.c_str());
}

void App::closeAllProjects()
{
    for (ProjectSession& session : projectSessions_)
    {
        if (session.window)
            WindowFactory::getInstance().destroyWindow(session.window);
    }

    projectSessions_.clear();
    setActiveContext(nullptr);
    dockLayoutInitialized_ = false;
}

void App::refreshWindowLabels()
{
    // 窗口标题展示项目名 + 脏标记：Name*
    for (ProjectSession& session : projectSessions_)
    {
        if (session.project && !session.project->getName().empty())
            session.windowBaseTitle = session.project->getName();

        const bool dirty = session.context && session.context->isProjectDirty();
        std::string label = session.windowBaseTitle;
        if (dirty)
            label += "*";
        label += "###ProjectWindow_" + std::to_string(session.projectId);

        if (session.windowLabel == label)
            continue;

        session.windowLabel = label;
        if (session.window)
            session.window->setWindowLabel(session.windowLabel);
    }
}

void App::handleProjectSwitchShortcut()
{
    // 少于两个会话时不需要切换
    if (projectSessions_.size() < 2)
        return;

    ImGuiIO& io = ImGui::GetIO();
    if (!io.KeyCtrl)
        return;
    if (!ImGui::IsKeyPressed(ImGuiKey_Tab, false))
        return;

    int currentIndex = findSessionIndexByContext(activeContext_);
    if (currentIndex < 0)
        currentIndex = 0;

    const int n = static_cast<int>(projectSessions_.size());
    const bool backward = io.KeyShift; // Ctrl+Shift+Tab 反向
    const int nextIndex = backward
        ? (currentIndex - 1 + n) % n
        : (currentIndex + 1) % n;

    ProjectSession& nextSession = projectSessions_[static_cast<size_t>(nextIndex)];
    setActiveContext(nextSession.context.get());
    ImGui::SetWindowFocus(nextSession.windowLabel.c_str());
}

void App::renderNewProjectPopup()
{
    // 菜单中点击 New 后，仅设置请求标志；真正 OpenPopup 放在渲染帧中执行
    if (newProjectPopupRequested_)
    {
        ImGui::OpenPopup("New Project");
        newProjectPopupRequested_ = false;
    }

    if (!ImGui::BeginPopupModal("New Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ImGui::TextUnformatted("Create a new project");
    ImGui::Separator();

    ImGui::SetNextItemWidth(140.0f);
    ImGui::InputInt("Width", &newProjectWidth_);
    ImGui::SetNextItemWidth(140.0f);
    ImGui::InputInt("Height", &newProjectHeight_);
    ImGui::SetNextItemWidth(140.0f);
    ImGui::InputInt("Frames", &newProjectFrameCount_);
    ImGui::ColorEdit4("Background", &newProjectBgColor_.x);

    // 可选画布底纹：棋盘 / 纯白
    const char* canvasBgItems[] = {"Checkerboard", "White"};
    ImGui::SetNextItemWidth(140.0f);
    ImGui::Combo("Canvas Background", &newProjectCanvasBgMode_, canvasBgItems, 2);

    // 输入兜底，避免非法参数
    if (newProjectWidth_ < 1)
        newProjectWidth_ = 1;
    if (newProjectHeight_ < 1)
        newProjectHeight_ = 1;
    if (newProjectFrameCount_ < 1)
        newProjectFrameCount_ = 1;

    ImGui::Separator();
    if (ImGui::Button("Create", ImVec2(120.0f, 0.0f)))
    {
        createNewProject(
            newProjectWidth_,
            newProjectHeight_,
            newProjectFrameCount_,
            float4ToRgba(newProjectBgColor_),
            newProjectCanvasBgMode_ == 0);
        ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
    {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void App::requestOpenProjectDialog()
{
    if (openDialogInFlight_)
        return;

    static const SDL_DialogFileFilter filters[] = {
        {"PixelAnimator Project", "pxanim"},
        {"All Files", "*"}
    };

    openDialogInFlight_ = true;
    SDL_ShowOpenFileDialog(
        &App::onOpenDialogClosed,
        this,
        window_,
        filters,
        2,
        nullptr,
        false);
}

void App::requestSaveAsDialog()
{
    if (saveDialogInFlight_)
        return;

    static const SDL_DialogFileFilter filters[] = {
        {"PixelAnimator Project", "pxanim"},
        {"All Files", "*"}
    };

    const char* defaultLocation = nullptr;
    std::string candidatePath;
    if (activeContext_)
    {
        candidatePath = activeContext_->getProjectFilePath();
        if (candidatePath.empty())
        {
            const Project* project = activeContext_->getProject();
            if (project && !project->getName().empty())
                candidatePath = project->getName() + ".pxanim";
        }
        if (!candidatePath.empty())
            defaultLocation = candidatePath.c_str();
    }

    saveDialogInFlight_ = true;
    SDL_ShowSaveFileDialog(
        &App::onSaveDialogClosed,
        this,
        window_,
        filters,
        2,
        defaultLocation);
}

void SDLCALL App::onOpenDialogClosed(void* userdata, const char* const* filelist, int filter)
{
    (void)filter;
    App* app = static_cast<App*>(userdata);
    if (!app)
        return;

    std::lock_guard<std::mutex> guard(app->dialogMutex_);
    app->openDialogInFlight_ = false;

    if (!filelist)
    {
        app->pendingDialogError_ = SDL_GetError();
        if (app->pendingDialogError_.empty())
            app->pendingDialogError_ = "Open dialog failed.";
        app->pendingDialogErrorReady_ = true;
        return;
    }

    if (!filelist[0])
        return; // 用户取消

    app->pendingOpenPath_ = filelist[0];
    app->pendingOpenReady_ = true;
}

void SDLCALL App::onSaveDialogClosed(void* userdata, const char* const* filelist, int filter)
{
    (void)filter;
    App* app = static_cast<App*>(userdata);
    if (!app)
        return;

    std::lock_guard<std::mutex> guard(app->dialogMutex_);
    app->saveDialogInFlight_ = false;

    if (!filelist)
    {
        app->pendingDialogError_ = SDL_GetError();
        if (app->pendingDialogError_.empty())
            app->pendingDialogError_ = "Save dialog failed.";
        app->pendingDialogErrorReady_ = true;
        return;
    }

    if (!filelist[0])
        return; // 用户取消

    app->pendingSavePath_ = filelist[0];
    app->pendingSaveReady_ = true;
}

void App::pollDialogResults()
{
    std::string openPath;
    std::string savePath;
    std::string dialogError;
    {
        std::lock_guard<std::mutex> guard(dialogMutex_);
        if (pendingOpenReady_)
        {
            openPath = pendingOpenPath_;
            pendingOpenPath_.clear();
            pendingOpenReady_ = false;
        }
        if (pendingSaveReady_)
        {
            savePath = pendingSavePath_;
            pendingSavePath_.clear();
            pendingSaveReady_ = false;
        }
        if (pendingDialogErrorReady_)
        {
            dialogError = pendingDialogError_;
            pendingDialogError_.clear();
            pendingDialogErrorReady_ = false;
        }
    }

    if (!dialogError.empty())
        showError(dialogError);
    if (!openPath.empty())
        openProjectFromPath(openPath);
    if (!savePath.empty())
        saveActiveProjectAs(savePath);
}

void App::renderErrorPopup()
{
    if (!pendingErrorMessage_.empty())
    {
        ImGui::OpenPopup("Error");
    }

    if (!ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ImGui::TextWrapped("%s", pendingErrorMessage_.c_str());
    ImGui::Separator();
    if (ImGui::Button("OK", ImVec2(120.0f, 0.0f)))
    {
        pendingErrorMessage_.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void App::showError(const std::string& message)
{
    pendingErrorMessage_ = message;
}

bool App::saveProjectAs(AppContext* context, const std::string& path)
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

    std::string finalPath = path;
    try
    {
        const std::filesystem::path fsPath(path);
        const std::string ext = fsPath.extension().string();
        if (ext.empty() || ext == ".")
            finalPath += ".pxanim";
    }
    catch (...)
    {
        // 路径解析失败时保持原始输入，交给序列化阶段返回具体错误。
    }

    Project* project = context->getProject();
    if (!project)
    {
        showError("No project data to save.");
        return false;
    }

    // Save As 时把文件名（不含扩展）同步为项目名，便于窗口标题显示。
    project->setName(projectNameFromPath(finalPath));

    std::string error;
    if (!ProjectSerializer::save(*project, finalPath, &error))
    {
        showError(error.empty() ? "Failed to save project." : error);
        return false;
    }

    context->setProjectFilePath(finalPath);
    context->setProjectDirty(false);
    return true;
}

bool App::saveActiveProject()
{
    if (!activeContext_ || !activeContext_->hasProject())
    {
        showError("No active project to save.");
        return false;
    }

    const std::string& path = activeContext_->getProjectFilePath();
    if (path.empty())
    {
        requestSaveAsDialog();
        return false;
    }

    return saveProjectAs(activeContext_, path);
}

bool App::saveActiveProjectAs(const std::string& path)
{
    return saveProjectAs(activeContext_, path);
}

bool App::openProjectFromPath(const std::string& path)
{
    if (path.empty())
    {
        showError("Path is empty.");
        return false;
    }

    std::string error;
    std::unique_ptr<Project> loadedProject = ProjectSerializer::load(path, &error);
    if (!loadedProject)
    {
        showError(error.empty() ? "Failed to open project." : error);
        return false;
    }

    if (loadedProject->getName().empty())
        loadedProject->setName(projectNameFromPath(path));

    createSessionFromProject(std::move(loadedProject), path);
    return true;
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
    session.context->setCanvasPan(0.0f, 0.0f);
    session.context->setCanvasZoom(4);
    session.context->setCheckerboardBackgroundEnabled(true);

    session.projectId = nextProjectId_++;
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

    projectSessions_.push_back(std::move(session));
    setActiveContext(rawContext);
    dockLayoutInitialized_ = false;
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
    session.context->setCanvasPan(0.0f, 0.0f);
    session.context->setCanvasZoom(4);
    session.context->setCheckerboardBackgroundEnabled(checkerboardBackground);

    // 生成唯一窗口标题/ID
    session.projectId = nextProjectId_++;
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

    projectSessions_.push_back(std::move(session));
    setActiveContext(rawContext);

    // 下帧重建 dock，确保新窗口可见
    dockLayoutInitialized_ = false;
}
