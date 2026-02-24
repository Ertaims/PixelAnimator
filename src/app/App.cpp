/**
 * @file App.cpp
 * @brief App 主流程实现：初始化、主循环渲染、资源释放、多项目会话管理
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
#include "ui/windows/ProjectWindow.h"
#include "ui/windows/Window.h"
#include "ui/windows/WindowFactory.h"
#include <SDL3/SDL_opengl.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <iostream>

namespace
{
    // ImGui 颜色(float4) 转为项目使用的 RGBA8888（R 在低字节）
    uint32_t float4ToRgba(const ImVec4& color)
    {
        const uint32_t r = static_cast<uint32_t>(std::round(color.x * 255.0f)) & 0xFF;
        const uint32_t g = static_cast<uint32_t>(std::round(color.y * 255.0f)) & 0xFF;
        const uint32_t b = static_cast<uint32_t>(std::round(color.z * 255.0f)) & 0xFF;
        const uint32_t a = static_cast<uint32_t>(std::round(color.w * 255.0f)) & 0xFF;
        return (r << 0) | (g << 8) | (b << 16) | (a << 24);
    }
}

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
        [this]() { newProjectPopupRequested_ = true; },
        [this]() { closeProjectByContext(activeContext_); },
        [this]() { closeAllProjects(); });

    editMenu_ = menuFactory.createEditMenu(menuManager_, nullptr);
    menuFactory.createViewMenu(menuManager_);
    menuFactory.createHelpMenu(menuManager_);

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

    renderNewProjectPopup();
    handleProjectSwitchShortcut();
    refreshWindowLabels();

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

    if (projectSessions_.empty())
    {
        setActiveContext(nullptr);
        return;
    }

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
    for (ProjectSession& session : projectSessions_)
    {
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
    const bool backward = io.KeyShift;
    const int nextIndex = backward
        ? (currentIndex - 1 + n) % n
        : (currentIndex + 1) % n;

    ProjectSession& nextSession = projectSessions_[static_cast<size_t>(nextIndex)];
    setActiveContext(nextSession.context.get());
    ImGui::SetWindowFocus(nextSession.windowLabel.c_str());
}

void App::renderNewProjectPopup()
{
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
    const char* canvasBgItems[] = {"Checkerboard", "White"};
    ImGui::SetNextItemWidth(140.0f);
    ImGui::Combo("Canvas Background", &newProjectCanvasBgMode_, canvasBgItems, 2);

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

void App::createNewProject(int width, int height, int frameCount, uint32_t fillColor, bool checkerboardBackground)
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
    session.context->setCheckerboardBackgroundEnabled(checkerboardBackground);

    session.projectId = nextProjectId_++;
    session.windowBaseTitle = "Project " + std::to_string(session.projectId);
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
