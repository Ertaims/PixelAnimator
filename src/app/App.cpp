/**
 * @file App.cpp
 * @brief App 主流程实现：初始化、主循环渲染、资源释放、多项目会话切换
 */

#include "app/App.h"

#include "core/AppContext.h"
#include "core/Project.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"
#include "imgui_internal.h"
#include "render/Texture.h"

#include "ui/menu/MenuFactory.h"
#include "ui/menu/menu_items/Menu_Edit.h"
#include "ui/menu/menu_items/Menu_File.h"
#include "ui/windows/ProjectWindow.h"
#include "ui/windows/Window.h"
#include "ui/windows/WindowFactory.h"

#include <SDL3/SDL_opengl.h>
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <iostream>
#include <string_view>

namespace
{
    // 拖拽打开项目时只做轻量扩展名判断；完整文件读写逻辑在 App_FileIO.cpp。
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

    bool isSupportedProjectPath(const std::string& path)
    {
        return endsWithInsensitive(path, ".pxanim")
            || endsWithInsensitive(path, ".pxanim.json")
            || endsWithInsensitive(path, ".json");
    }
}

App::App() = default;
App::~App() = default;

bool App::init()
{
    // 初始化 SDL
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        std::fprintf(stderr, "Error: SDL_Init(): %s\n", SDL_GetError());
        return false;
    }

    // 创建主窗口与 OpenGL 上下文
    if (!createWindowAndContext()) return false;

    // 初始化 ImGui
    if (!initImGui()) return false;

    // 启动时先加载 Recent（持久化数据），再创建菜单与默认项目，
    //    这样 File->Open Recent 首帧就能看到历史列表。
    loadRecentProjectPaths();

    // 创建菜单与默认项目
    createMenuAndWindows();

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    return true;
}

AppContext& App::getContext()
{
    // 约束：调用方只能在存在活跃项目时访问
    assert(m_activeContext && "No active project context.");
    return *m_activeContext;
}

const AppContext& App::getContext() const
{
    assert(m_activeContext && "No active project context.");
    return *m_activeContext;
}

bool App::createWindowAndContext()
{
    m_mainScale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_WindowFlags windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
        | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    m_window = SDL_CreateWindow("Pixel Animator",
                               static_cast<int>(1280 * m_mainScale),
                               static_cast<int>(800 * m_mainScale),
                               windowFlags);
    if (!m_window)
    {
        std::fprintf(stderr, "Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return false;
    }

    m_glContext = SDL_GL_CreateContext(m_window);
    if (!m_glContext)
    {
        std::fprintf(stderr, "Error: SDL_GL_CreateContext(): %s\n", SDL_GetError());
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
        return false;
    }

    SDL_GL_MakeCurrent(m_window, m_glContext);
    SDL_GL_SetSwapInterval(1);
    SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(m_window);
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
    style.ScaleAllSizes(m_mainScale);
    style.FontScaleDpi = m_mainScale;
    io.ConfigDpiScaleFonts = true;
    io.ConfigDpiScaleViewports = true;

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui_ImplSDL3_InitForOpenGL(m_window, m_glContext);
    ImGui_ImplOpenGL3_Init(m_glslVersion);
    return true;
}

void App::createMenuAndWindows()
{
    ConcreteMenuFactory menuFactory;
    m_menuManager = menuFactory.createMenuManager();

    // File 菜单回调：
    // - New: 打开 New Project 弹窗
    // - Close / Close All: 关闭当前或全部会话
    // - Exit: 退出主循环
    m_fileMenu = menuFactory.createFileMenu(
        m_menuManager,
        nullptr,
        [this]() { m_done = true; },
        [this]() { m_newProjectPopupRequested = true; },
        [this]() { requestOpenProjectDialog(); },
        [this]() { saveActiveProject(); },
        [this]() { requestSaveAsDialog(ProjectFileFormat::Binary); },
        [this]() { requestSaveAsDialog(ProjectFileFormat::Json); },
        [this]() { requestExportDialog(ExportKind::CurrentFramePng); },
        [this]() { m_spriteSheetExportPopupRequested = true; },
        [this]() { requestImportDialog(ImportKind::CurrentFramePng); },
        [this]() { requestImportDialog(ImportKind::SpriteSheetPng); },
        [this](const std::string& path) { openProjectFromPath(path); },
        [this]() { closeProjectByContext(m_activeContext); },
        [this]() { closeAllProjects(); });
    refreshRecentProjectsMenu();

    m_editMenu = menuFactory.createEditMenu(m_menuManager, nullptr);
    if (m_editMenu)
    {
        m_editMenu->setOnUndoHistoryRequested([this]() { m_undoHistoryPopupRequested = true; });
        m_editMenu->setOnCutRequested([this]() { executeCutSelection(); });
        m_editMenu->setOnCopyRequested([this]() { executeCopySelection(); });
        m_editMenu->setOnPasteRequested([this]() { executePasteSelection(); });
        m_editMenu->setOnDeleteRequested([this]() { executeDelete(); });
        m_editMenu->setOnRotateRequested([this](commands::RotationAngle angle) { executeRotate(angle); });
        m_editMenu->setOnFlipRequested([this](commands::FlipDirection direction) { executeFlip(direction); });
    }
    // menuFactory.createViewMenu(m_menuManager);
    // menuFactory.createHelpMenu(m_menuManager);

    // 启动时默认创建一个项目，保证界面可用
    createNewProject(16, 16, 1, 0x00000000);
}

void App::run()
{
    while (!m_done)
    {
        processEvents();

        // 最小化时降低 CPU 占用
        if (SDL_GetWindowFlags(m_window) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            continue;
        }

        renderFrame();
        SDL_GL_SwapWindow(m_window);
    }
}

void App::processEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type == SDL_EVENT_QUIT) m_done = true;

        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED
            && event.window.windowID == SDL_GetWindowID(m_window))
            m_done = true;
        
        if (event.type == SDL_EVENT_DROP_FILE)
        {
            // 只处理主窗口上的拖拽
            if (event.drop.windowID != SDL_GetWindowID(m_window)) continue;

            // SDL 事件里的 data 生命周期只保证事件处理期间有效，这里先拷贝到 std::string。
            const std::string droppedPath = event.drop.data ? event.drop.data : "";
            if (droppedPath.empty()) continue;

            if (!isSupportedProjectPath(droppedPath))
            {
                showError("Unsupported file type. Please drop .pxanim or .pxanim.json files.");
                continue;
            }

            openProjectFromPath(droppedPath);
        }
    }
}

void App::renderFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();

    if (m_menuManager) m_menuManager->render();

    // 每帧更新：New Project 弹窗、快捷切换、窗口标题脏标记
    pollDialogResults();
    renderNewProjectPopup();
    renderSpriteSheetExportPopup();
    renderSpriteSheetImportPopup();
    renderErrorPopup();
    renderUndoHistoryPopup();
    handleFileMenuShortcuts();
    handleEditMenuShortcuts();
    handleToolShortcuts();
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

        if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode) window_flags |= ImGuiWindowFlags_NoBackground;

        if (!opt_padding) ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace Demo", nullptr, window_flags);
        if (!opt_padding) ImGui::PopStyleVar();

        if (opt_fullscreen) ImGui::PopStyleVar(2);

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
        if (window) window->render();
    }

    ImGui::Render();
    glViewport(0, 0, static_cast<int>(io.DisplaySize.x), static_cast<int>(io.DisplaySize.y));
    glClearColor(m_clearColor.x * m_clearColor.w, m_clearColor.y * m_clearColor.w,
                 m_clearColor.z * m_clearColor.w, m_clearColor.w);
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
    // 退出前持久化 Recent 列表，确保本次会话更新不会丢失。
    saveRecentProjectPaths();

    // 先释放导出/导入预览纹理，避免 GL 资源泄漏。
    render::deleteTexture(m_spriteSheetRowIconTexture);
    render::deleteTexture(m_spriteSheetColumnIconTexture);
    render::deleteTexture(m_spriteSheetRowColumnIconTexture);
    render::deleteTexture(m_spriteSheetImportPreviewTexture);

    // 先销毁窗口，再清空会话，防止悬空指针
    WindowFactory::getInstance().cleanup();
    m_projectSessions.clear();
    m_activeContext = nullptr;

    if (m_menuManager)
    {
        delete m_menuManager;
        m_menuManager = nullptr;
    }
    m_fileMenu = nullptr;
    m_editMenu = nullptr;

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    if (m_glContext && m_window)
    {
        SDL_GL_DestroyContext(m_glContext);
        m_glContext = nullptr;
    }
    if (m_window)
    {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    SDL_Quit();
}

void App::setupDefaultDockLayout()
{
    // 仅在需要时重建布局（新建/关闭会话后会置 false）
    if (m_dockLayoutInitialized) return;

    ImGuiID dockspaceId = ImGui::GetID("MyDockSpace");
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

    for (const ProjectSession& session : m_projectSessions)
    {
        if (session.window) ImGui::DockBuilderDockWindow(session.windowLabel.c_str(), dockspaceId);
    }

    ImGui::DockBuilderFinish(dockspaceId);
    m_dockLayoutInitialized = true;
}