/**
 * @file App.cpp
 * @brief 应用程序主类实现：初始化、主循环、收尾
 */

#include "app/App.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"
#include "ui/menu/MenuFactory.h"
#include "ui/windows/WindowFactory.h"
#include "ui/windows/Window.h"
#include "core/Project.h"
#include <SDL3/SDL_opengl.h>
#include <cstdio>
#include <iostream>

App::App() = default;
App::~App() = default;
bool App::init()
{
    // -------------------------------------------------------------------------
    // 1. 初始化 SDL
    // -------------------------------------------------------------------------
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        std::fprintf(stderr, "Error: SDL_Init(): %s\n", SDL_GetError());
        return false;
    }

    // -------------------------------------------------------------------------
    // 2. 创建窗口与 OpenGL 上下文
    // -------------------------------------------------------------------------
    if (!createWindowAndContext())
        return false;

    // -------------------------------------------------------------------------
    // 3. 初始化 ImGui
    // -------------------------------------------------------------------------
    if (!initImGui())
        return false;

    // -------------------------------------------------------------------------
    // 4. 创建菜单与窗口（依赖 ImGui 已就绪）
    // -------------------------------------------------------------------------
    createMenuAndWindows();

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    return true;
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
        (int)(1280 * mainScale_), (int)(800 * mainScale_), windowFlags);
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

    // 文件菜单：传入 context_，New/Open/Save 操作 context；Exit 时设置 done_
    menuFactory.createFileMenu(menuManager_, &context_, [this]() { done_ = true; });
    // 编辑菜单：Undo/Redo 调用 context_.undo() / context_.redo()
    menuFactory.createEditMenu(menuManager_, &context_);
    menuFactory.createViewMenu(menuManager_);
    menuFactory.createHelpMenu(menuManager_);

    WindowFactory& windowFactory = WindowFactory::getInstance();
    projectWindow_ = windowFactory.createProjectWindow(&context_);

    // Start with a default in-memory project so canvas/timeline panels are usable.
    activeProject_ = std::make_unique<Project>();
    context_.setProject(activeProject_.get());
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

    // 顶部菜单栏
    if (menuManager_)
        menuManager_->render();

    // 主 DockSpace（全屏铺满，子窗口可停靠）
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

    // Render all registered tool windows/panels.
    for (Window* window : WindowFactory::getInstance().getWindows())
    {
        if (window)
        {
            window->render();
        }
    }
    // statusBar::draw(context_);

    ImGui::Render();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
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
    // 先清理由我们或工厂创建的 UI 对象
    WindowFactory::getInstance().cleanup();
    projectWindow_ = nullptr;
    context_.setProject(nullptr);
    activeProject_.reset();

    if (menuManager_)
    {
        delete menuManager_;
        menuManager_ = nullptr;
    }

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

    ImGuiID centerId = dockspaceId;

    ImGui::DockBuilderDockWindow("Project", centerId);

    ImGui::DockBuilderFinish(dockspaceId);
    dockLayoutInitialized_ = true;
}
