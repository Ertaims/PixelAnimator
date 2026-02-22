/**
 * @file App.cpp
 * @brief 应用程序主类实现：初始化、主循环、收尾
 */

#include "app/App.h"

#include "core/AppContext.h"
#include "core/Project.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"
#include "imgui_internal.h"
#include "ui/menu/MenuFactory.h"
#include "ui/menu/menu_items/Menu_Edit.h"
#include "ui/menu/menu_items/Menu_File.h"
#include "ui/windows/Window.h"
#include "ui/windows/WindowFactory.h"
#include <SDL3/SDL_opengl.h>
#include <cassert>
#include <cstdio>
#include <iostream>

App::App() = default;
App::~App() = default;

bool App::init()
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        std::fprintf(stderr, "Error: SDL_Init(): %s\n", SDL_GetError());
        return false;
    }

    if (!createWindowAndContext())
        return false;

    if (!initImGui())
        return false;

    createMenuAndWindows();

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    return true;
}

AppContext& App::getContext()
{
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

    fileMenu_ = menuFactory.createFileMenu(
        menuManager_,
        nullptr,
        [this]() { done_ = true; },
        [this]() { createNewProject(16, 16, 1, 0x00000000); });

    editMenu_ = menuFactory.createEditMenu(menuManager_, nullptr);
    menuFactory.createViewMenu(menuManager_);
    menuFactory.createHelpMenu(menuManager_);

    // 默认创建一个项目窗口，保证启动后可编辑。
    createNewProject(16, 16, 1, 0x00000000);
}

void App::run()
{
    while (!done_)
    {
        processEvents();

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
    if (!context || activeContext_ == context)
        return;

    activeContext_ = context;
    if (fileMenu_)
        fileMenu_->setContext(activeContext_);
    if (editMenu_)
        editMenu_->setContext(activeContext_);
}

void App::createNewProject(int width, int height, int frameCount, uint32_t fillColor)
{
    ProjectSession session;
    session.project = std::make_unique<Project>(width, height, frameCount, fillColor);
    session.context = std::make_unique<AppContext>();
    session.context->setProject(session.project.get());
    session.context->setProjectFilePath("");
    session.context->setProjectDirty(false);
    session.context->setCurrentAnimationIndex(0);
    session.context->setCurrentFrameIndex(0);
    session.context->setCanvasPan(0.0f, 0.0f);
    session.context->setCanvasZoom(4);

    const int projectId = nextProjectId_++;
    session.windowLabel = "Project " + std::to_string(projectId) +
        "###ProjectWindow_" + std::to_string(projectId);

    AppContext* rawContext = session.context.get();
    session.window = WindowFactory::getInstance().createProjectWindow(
        rawContext,
        session.windowLabel,
        [this](AppContext* focusedContext) { setActiveContext(focusedContext); });
    session.window->setVisible(true);

    projectSessions_.push_back(std::move(session));
    setActiveContext(rawContext);

    // 新项目创建后重新应用 dock 布局，保证窗口可见。
    dockLayoutInitialized_ = false;
}
