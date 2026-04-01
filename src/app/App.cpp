/**
 * @file App.cpp
 * @brief App 主流程实现：初始化、主循环渲染、资源释放、多项目会话切换
 */

#include "app/App.h"

#include "core/AppContext.h"
#include "core/Project.h"
#include "io/ImageExporter.h"
#include "io/ProjectJsonSerializer.h"
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
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string_view>

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
            const std::string filename = p.filename().string();
            if (filename.size() > std::string(".pxanim.json").size())
            {
                const std::string lowerFilename = [&filename]() {
                    std::string result = filename;
                    std::transform(result.begin(), result.end(), result.begin(),
                                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                    return result;
                }();
                if (lowerFilename.size() >= 12
                    && lowerFilename.rfind(".pxanim.json") == lowerFilename.size() - 12)
                {
                    return filename.substr(0, filename.size() - 12);
                }
            }
            const std::string stem = p.stem().string();
            if (!stem.empty())
                return stem;
        }
        catch (...)
        {
        }
        return "Untitled";
    }

    std::string toLowerCopy(const std::string& value)
    {
        std::string result = value;
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return result;
    }

    bool endsWithInsensitive(const std::string& text, std::string_view suffix)
    {
        if (text.size() < suffix.size())
            return false;
        const std::string lower = toLowerCopy(text);
        return lower.compare(lower.size() - suffix.size(), suffix.size(), suffix.data()) == 0;
    }

    App::ProjectFileFormat detectFormatFromPath(const std::string& path)
    {
        if (endsWithInsensitive(path, ".pxanim.json") || endsWithInsensitive(path, ".json"))
            return App::ProjectFileFormat::Json;
        return App::ProjectFileFormat::Binary;
    }

    bool isSupportedProjectPath(const std::string& path)
    {
        return endsWithInsensitive(path, ".pxanim")
            || endsWithInsensitive(path, ".pxanim.json")
            || endsWithInsensitive(path, ".json");
    }

    std::string normalizeSavePath(const std::string& path, App::ProjectFileFormat preferredFormat)
    {
        if (path.empty())
            return path;

        if (preferredFormat == App::ProjectFileFormat::Json)
        {
            if (endsWithInsensitive(path, ".pxanim.json") || endsWithInsensitive(path, ".json"))
                return path;
            if (endsWithInsensitive(path, ".pxanim"))
                return path + ".json";
            return path + ".pxanim.json";
        }

        // Binary:
        if (endsWithInsensitive(path, ".pxanim"))
            return path;
        if (endsWithInsensitive(path, ".pxanim.json"))
            return path.substr(0, path.size() - 5); // 去掉末尾 ".json" -> ".pxanim"
        if (endsWithInsensitive(path, ".json"))
            return path.substr(0, path.size() - 5) + ".pxanim";
        return path + ".pxanim";
    }

    std::string normalizePngPath(const std::string& path)
    {
        if (path.empty())
            return path;
        if (endsWithInsensitive(path, ".png"))
            return path;
        return path + ".png";
    }

    /**
     * @brief 从文件加载 OpenGL 纹理（用于导出模式图标按钮）。
     */
    unsigned int loadTextureFromFile(const char* path)
    {
        SDL_Surface* surface = IMG_Load(path);
        if (!surface)
            return 0;

        SDL_Surface* rgbaSurface = surface;
        if (surface->format != SDL_PIXELFORMAT_RGBA32)
        {
            rgbaSurface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
            SDL_DestroySurface(surface);
            if (!rgbaSurface)
                return 0;
        }

        unsigned int texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGBA8,
                     rgbaSurface->w,
                     rgbaSurface->h,
                     0,
                     GL_RGBA,
                     GL_UNSIGNED_BYTE,
                     rgbaSurface->pixels);

        SDL_DestroySurface(rgbaSurface);
        return texture;
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
        [this]() { requestSaveAsDialog(ProjectFileFormat::Binary); },
        [this]() { requestSaveAsDialog(ProjectFileFormat::Json); },
        [this]() { requestExportDialog(ExportKind::CurrentFramePng); },
        [this]() { spriteSheetExportPopupRequested_ = true; },
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
        
        if(event.type == SDL_EVENT_DROP_FILE)
        {
            // 只处理主窗口上的拖拽
            if(event.drop.windowID != SDL_GetWindowID(window_))
                continue;

            // SDL 事件里的 data 生命周期只保证事件处理期间有效，这里先拷贝到 std::string。
            const std::string droppedPath = event.drop.data ? event.drop.data : "";
            if (droppedPath.empty())
                continue;

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

    if (menuManager_)
        menuManager_->render();

    // 每帧更新：New Project 弹窗、快捷切换、窗口标题脏标记
    pollDialogResults();
    renderNewProjectPopup();
    renderSpriteSheetExportPopup();
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
    // 先释放导出模式图标纹理，避免 GL 资源泄漏。
    if (spriteSheetRowIconTexture_ != 0)
    {
        glDeleteTextures(1, &spriteSheetRowIconTexture_);
        spriteSheetRowIconTexture_ = 0;
    }
    if (spriteSheetColumnIconTexture_ != 0)
    {
        glDeleteTextures(1, &spriteSheetColumnIconTexture_);
        spriteSheetColumnIconTexture_ = 0;
    }
    if (spriteSheetRowColumnIconTexture_ != 0)
    {
        glDeleteTextures(1, &spriteSheetRowColumnIconTexture_);
        spriteSheetRowColumnIconTexture_ = 0;
    }

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
        {"PixelAnimator JSON Project", "pxanim.json;json"},
        {"All Files", "*"}
    };

    openDialogInFlight_ = true;
    SDL_ShowOpenFileDialog(
        &App::onOpenDialogClosed,
        this,
        window_,
        filters,
        3,
        nullptr,
        false);
}

void App::requestSaveAsDialog(ProjectFileFormat format)
{
    if (saveDialogInFlight_)
        return;

    saveDialogFormat_ = format;

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
    if (activeContext_)
    {
        candidatePath = activeContext_->getProjectFilePath();
        if (candidatePath.empty())
        {
            const Project* project = activeContext_->getProject();
            if (project && !project->getName().empty())
                candidatePath = format == ProjectFileFormat::Json
                    ? (project->getName() + ".pxanim.json")
                    : (project->getName() + ".pxanim");
        }
        else
        {
            // Save As 按当前用户选择的格式预填路径，避免 JSON 模式仍默认二进制扩展名。
            candidatePath = normalizeSavePath(candidatePath, format);
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
        filterCount,
        defaultLocation);
}

void App::requestExportDialog(ExportKind kind)
{
    if (exportDialogInFlight_)
        return;
    if (!activeContext_ || !activeContext_->hasProject())
    {
        showError("No active project to export.");
        return;
    }

    exportDialogKind_ = kind;
    exportDialogSpriteMode_ = spriteSheetExportMode_;
    exportDialogUseSelectedFrames_ = spriteSheetExportUseSelectedFrames_;
    exportDialogColumnsPerRow_ = std::max(1, spriteSheetExportColumnsPerRow_);
    exportDialogUseCustomGroups_ = spriteSheetExportUseCustomGroups_;
    exportDialogGroupSpacing_ = std::max(0, spriteSheetExportGroupSpacing_);
    exportDialogCustomGroups_.clear();
    if (spriteSheetExportUseCustomGroups_)
    {
        // 仅在“分组模式”下解析分组，避免影响原有行/列/网格流程。
        std::string parseError;
        if (!buildResolvedSpriteGroups(exportDialogCustomGroups_, parseError))
        {
            showError(parseError.empty() ? "Invalid custom sprite groups." : parseError);
            return;
        }
    }

    static const SDL_DialogFileFilter filters[] = {
        {"PNG Image", "png"},
        {"All Files", "*"}
    };

    std::string baseName = "export";
    const Project* project = activeContext_->getProject();
    if (project && !project->getName().empty())
        baseName = project->getName();

    std::string defaultPath;
    if (kind == ExportKind::CurrentFramePng)
    {
        defaultPath = baseName + "_frame_"
            + std::to_string(activeContext_->getCurrentFrameIndex() + 1) + ".png";
    }
    else
    {
        // 根据配置模式拼接默认导出文件名，方便用户区分不同布局。
        const char* modeText = "row";
        if (spriteSheetExportUseCustomGroups_)
            modeText = "grouped";
        else if (spriteSheetExportMode_ == SpriteSheetExportMode::Column)
            modeText = "column";
        else if (spriteSheetExportMode_ == SpriteSheetExportMode::RowColumn)
            modeText = "rowcolumn";

        const char* scopeText = spriteSheetExportUseCustomGroups_
            ? "custom"
            : (spriteSheetExportUseSelectedFrames_ ? "selected" : "all");
        defaultPath = baseName + "_spritesheet_" + modeText + "_" + scopeText + ".png";
    }

    exportDialogInFlight_ = true;
    SDL_ShowSaveFileDialog(
        &App::onExportDialogClosed,
        this,
        window_,
        filters,
        2,
        defaultPath.c_str());
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
    app->pendingSaveFormat_ = app->saveDialogFormat_;
}

void SDLCALL App::onExportDialogClosed(void* userdata, const char* const* filelist, int filter)
{
    (void)filter;
    App* app = static_cast<App*>(userdata);
    if (!app)
        return;

    std::lock_guard<std::mutex> guard(app->dialogMutex_);
    app->exportDialogInFlight_ = false;

    if (!filelist)
    {
        app->pendingDialogError_ = SDL_GetError();
        if (app->pendingDialogError_.empty())
            app->pendingDialogError_ = "Export dialog failed.";
        app->pendingDialogErrorReady_ = true;
        return;
    }

    if (!filelist[0])
        return;

    app->pendingExportPath_ = filelist[0];
    app->pendingExportKind_ = app->exportDialogKind_;
    app->pendingExportSpriteMode_ = app->exportDialogSpriteMode_;
    app->pendingExportUseSelectedFrames_ = app->exportDialogUseSelectedFrames_;
    app->pendingExportColumnsPerRow_ = app->exportDialogColumnsPerRow_;
    app->pendingExportUseCustomGroups_ = app->exportDialogUseCustomGroups_;
    app->pendingExportGroupSpacing_ = app->exportDialogGroupSpacing_;
    app->pendingExportCustomGroups_ = app->exportDialogCustomGroups_;
    app->pendingExportReady_ = true;
}

void App::pollDialogResults()
{
    std::string openPath;
    std::string savePath;
    std::string exportPath;
    std::string dialogError;
    ProjectFileFormat saveFormat = ProjectFileFormat::Binary;
    ExportKind exportKind = ExportKind::CurrentFramePng;
    SpriteSheetExportMode exportSpriteMode = SpriteSheetExportMode::Row;
    bool exportUseSelectedFrames = false;
    int exportColumnsPerRow = 4;
    bool exportUseCustomGroups = false;
    int exportGroupSpacing = 8;
    std::vector<SpriteSheetGroupResolved> exportCustomGroups;
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
            saveFormat = pendingSaveFormat_;
        }
        if (pendingDialogErrorReady_)
        {
            dialogError = pendingDialogError_;
            pendingDialogError_.clear();
            pendingDialogErrorReady_ = false;
        }
        if (pendingExportReady_)
        {
            exportPath = pendingExportPath_;
            pendingExportPath_.clear();
            pendingExportReady_ = false;
            exportKind = pendingExportKind_;
            exportSpriteMode = pendingExportSpriteMode_;
            exportUseSelectedFrames = pendingExportUseSelectedFrames_;
            exportColumnsPerRow = pendingExportColumnsPerRow_;
            exportUseCustomGroups = pendingExportUseCustomGroups_;
            exportGroupSpacing = pendingExportGroupSpacing_;
            exportCustomGroups = pendingExportCustomGroups_;
            pendingExportCustomGroups_.clear();
        }
    }

    if (!dialogError.empty())
        showError(dialogError);
    if (!openPath.empty())
        openProjectFromPath(openPath);
    if (!savePath.empty())
        saveActiveProjectAs(savePath, saveFormat);
    if (!exportPath.empty())
        exportToPath(exportPath,
                     exportKind,
                     exportSpriteMode,
                     exportUseSelectedFrames,
                     exportColumnsPerRow,
                     exportUseCustomGroups,
                     exportCustomGroups,
                     exportGroupSpacing);
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
        : ProjectSerializer::save(*project, finalPath, &error);
    if (!ok)
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
        requestSaveAsDialog(ProjectFileFormat::Binary);
        return false;
    }

    return saveProjectAs(activeContext_, path, detectFormatFromPath(path));
}

bool App::saveActiveProjectAs(const std::string& path, ProjectFileFormat preferredFormat)
{
    return saveProjectAs(activeContext_, path, preferredFormat);
}

bool App::exportToPath(const std::string& path,
                       ExportKind kind,
                       SpriteSheetExportMode spriteMode,
                       bool useSelectedFrames,
                       int columnsPerRow,
                       bool useCustomGroups,
                       const std::vector<SpriteSheetGroupResolved>& customGroups,
                       int groupSpacing)
{
    if (!activeContext_ || !activeContext_->hasProject())
    {
        showError("No active project to export.");
        return false;
    }

    Project* project = activeContext_->getProject();
    if (!project)
    {
        showError("No project data to export.");
        return false;
    }

    const std::string finalPath = normalizePngPath(path);
    std::string error;

    if (kind == ExportKind::CurrentFramePng)
    {
        if (!ImageExporter::exportSingleFramePng(*project, activeContext_->getCurrentFrameIndex(), finalPath, &error))
        {
            showError(error.empty() ? "Failed to export current frame." : error);
            return false;
        }
        return true;
    }

    // 分组模式优先：每组可独立行排/列排，再进行组级拼接。
    if (useCustomGroups)
    {
        std::vector<ImageExporter::SpriteGroup> groups;
        groups.reserve(customGroups.size());
        for (const SpriteSheetGroupResolved& customGroup : customGroups)
        {
            ImageExporter::SpriteGroup group;
            group.name = customGroup.name;
            group.frameIndices = customGroup.frameIndices;
            group.layout = customGroup.mode == SpriteSheetExportMode::Column
                ? ImageExporter::SpriteSheetLayout::Column
                : ImageExporter::SpriteSheetLayout::Row;
            groups.push_back(std::move(group));
        }

        if (!ImageExporter::exportGroupedSpriteSheetPng(
                *project,
                groups,
                std::max(0, groupSpacing),
                finalPath,
                &error))
        {
            showError(error.empty() ? "Failed to export grouped sprite sheet." : error);
            return false;
        }
        return true;
    }

    // 传统模式：行 / 列 / 行列网格。
    ImageExporter::SpriteSheetLayout layout = ImageExporter::SpriteSheetLayout::Row;
    if (spriteMode == SpriteSheetExportMode::Column)
        layout = ImageExporter::SpriteSheetLayout::Column;
    else if (spriteMode == SpriteSheetExportMode::RowColumn)
        layout = ImageExporter::SpriteSheetLayout::RowColumn;

    std::vector<int> frameIndices;
    if (useSelectedFrames)
    {
        frameIndices = activeContext_->getSelectedFrameIndices();
        if (frameIndices.empty())
        {
            showError("No selected frames to export.");
            return false;
        }
    }

    if (!ImageExporter::exportSpriteSheetPng(*project,
                                             frameIndices,
                                             layout,
                                             std::max(1, columnsPerRow),
                                             finalPath,
                                             &error))
    {
        showError(error.empty() ? "Failed to export sprite sheet." : error);
        return false;
    }
    return true;
}

bool App::parseFrameListText(const std::string& text,
                             int maxFrameCount,
                             std::vector<int>& outIndices,
                             std::string& outError) const
{
    outIndices.clear();
    outError.clear();

    if (maxFrameCount <= 0)
    {
        outError = "Project has no frames.";
        return false;
    }

    // 支持输入格式：
    // 1) 单值：1, 5, 12
    // 2) 区间：3-7（闭区间）
    // 3) 混合：1,2,5-8,10
    // 分隔符支持逗号/空格/分号，统一先归一化为逗号再解析。
    std::string normalized = text;
    for (char& ch : normalized)
    {
        if (ch == ' ' || ch == ';' || ch == '\t' || ch == '\n' || ch == '\r')
            ch = ',';
    }

    std::vector<bool> used(static_cast<size_t>(maxFrameCount), false);
    std::stringstream ss(normalized);
    std::string token;
    while (std::getline(ss, token, ','))
    {
        if (token.empty())
            continue;

        const size_t dashPos = token.find('-');
        if (dashPos == std::string::npos)
        {
            int value = 0;
            try
            {
                value = std::stoi(token);
            }
            catch (...)
            {
                outError = "Invalid frame token: " + token;
                return false;
            }

            if (value < 1 || value > maxFrameCount)
            {
                outError = "Frame index out of range (1-based): " + std::to_string(value);
                return false;
            }

            const int idx = value - 1;
            if (!used[static_cast<size_t>(idx)])
            {
                outIndices.push_back(idx);
                used[static_cast<size_t>(idx)] = true;
            }
            continue;
        }

        const std::string left = token.substr(0, dashPos);
        const std::string right = token.substr(dashPos + 1);
        int startValue = 0;
        int endValue = 0;
        try
        {
            startValue = std::stoi(left);
            endValue = std::stoi(right);
        }
        catch (...)
        {
            outError = "Invalid frame range token: " + token;
            return false;
        }

        if (startValue < 1 || endValue < 1 || startValue > maxFrameCount || endValue > maxFrameCount)
        {
            outError = "Frame range out of bounds (1-based): " + token;
            return false;
        }

        if (startValue > endValue)
            std::swap(startValue, endValue);

        for (int value = startValue; value <= endValue; ++value)
        {
            const int idx = value - 1;
            if (!used[static_cast<size_t>(idx)])
            {
                outIndices.push_back(idx);
                used[static_cast<size_t>(idx)] = true;
            }
        }
    }

    if (outIndices.empty())
    {
        outError = "Frame list is empty.";
        return false;
    }
    return true;
}

bool App::buildResolvedSpriteGroups(std::vector<SpriteSheetGroupResolved>& outGroups, std::string& outError) const
{
    outGroups.clear();
    outError.clear();

    if (!activeContext_ || !activeContext_->hasProject())
    {
        outError = "No active project to export.";
        return false;
    }

    Project* project = activeContext_->getProject();
    if (!project)
    {
        outError = "No project data to export.";
        return false;
    }

    const int frameCount = project->getFrameCount();
    if (frameCount <= 0)
    {
        outError = "Project has no frames.";
        return false;
    }

    // 自定义分组导出直接沿用“时间轴右键创建”的分组数据，
    // 不再要求用户在导出弹窗重复输入帧号。
    const std::vector<AppContext::FrameGroup>& groups = activeContext_->getFrameGroups();
    if (groups.empty())
    {
        outError = "No frame groups. Please create groups in timeline first.";
        return false;
    }

    for (size_t i = 0; i < groups.size(); ++i)
    {
        const AppContext::FrameGroup& group = groups[i];
        if (group.frameIndices.empty())
            continue;

        SpriteSheetGroupResolved resolved;
        resolved.name = group.name.empty() ? ("Group " + std::to_string(i + 1)) : group.name;
        resolved.mode = SpriteSheetExportMode::Row; // 当前分组导出先默认“组内按行排”
        resolved.frameIndices = group.frameIndices;
        outGroups.push_back(std::move(resolved));
    }

    if (outGroups.empty())
    {
        outError = "No valid frame groups to export.";
        return false;
    }

    return true;
}

void App::renderSpriteSheetExportPopup()
{
    // 菜单点击后只设置请求标志，真正 OpenPopup 放在渲染帧中执行。
    if (spriteSheetExportPopupRequested_)
    {
        ImGui::OpenPopup("Export Sprite Sheet");
        spriteSheetExportPopupRequested_ = false;
    }

    if (!ImGui::BeginPopupModal("Export Sprite Sheet", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    // 首次打开时加载模式图标（row/column/row&column）。
    if (!spriteSheetExportIconsLoaded_)
    {
        const char* rowCandidates[] = {"src/assets/row.png", "../src/assets/row.png", "../../src/assets/row.png"};
        const char* colCandidates[] = {"src/assets/column.png", "../src/assets/column.png", "../../src/assets/column.png"};
        const char* rowColCandidates[] = {"src/assets/row&column.png", "../src/assets/row&column.png", "../../src/assets/row&column.png"};

        for (const char* p : rowCandidates)
        {
            spriteSheetRowIconTexture_ = loadTextureFromFile(p);
            if (spriteSheetRowIconTexture_ != 0)
                break;
        }
        for (const char* p : colCandidates)
        {
            spriteSheetColumnIconTexture_ = loadTextureFromFile(p);
            if (spriteSheetColumnIconTexture_ != 0)
                break;
        }
        for (const char* p : rowColCandidates)
        {
            spriteSheetRowColumnIconTexture_ = loadTextureFromFile(p);
            if (spriteSheetRowColumnIconTexture_ != 0)
                break;
        }
        spriteSheetExportIconsLoaded_ = true;
    }

    ImGui::TextUnformatted("Select sprite sheet mode");
    ImGui::Separator();

    auto drawModeButton = [this](SpriteSheetExportMode mode, unsigned int texture, const char* fallbackLabel) {
        const bool selected = (!spriteSheetExportUseCustomGroups_ && spriteSheetExportMode_ == mode);
        if (selected)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.75f, 0.90f));

        bool clicked = false;
        if (texture != 0)
        {
            clicked = ImGui::ImageButton(
                fallbackLabel,
                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(texture)),
                ImVec2(44.0f, 44.0f));
        }
        else
        {
            clicked = ImGui::Button(fallbackLabel, ImVec2(80.0f, 44.0f));
        }

        if (selected)
            ImGui::PopStyleColor();

        if (clicked)
        {
            spriteSheetExportUseCustomGroups_ = false;
            spriteSheetExportMode_ = mode;
        }
    };

    drawModeButton(SpriteSheetExportMode::Row, spriteSheetRowIconTexture_, "Row");
    ImGui::SameLine();
    drawModeButton(SpriteSheetExportMode::Column, spriteSheetColumnIconTexture_, "Column");
    ImGui::SameLine();
    drawModeButton(SpriteSheetExportMode::RowColumn, spriteSheetRowColumnIconTexture_, "Row+Column");

    ImGui::SameLine();
    // 第四种逻辑模式：分组自定义（不使用图标，使用文本按钮避免增加资源依赖）。
    //
    // 关键修复说明：
    // - 这里必须用“点击前状态”控制 Push/Pop 是否配对；
    // - 不能在 Button 点击后再用 spriteSheetExportUseCustomGroups_ 判断 Pop，
    //   否则当按钮把 false 改成 true 时，会出现“未 Push 却 Pop”的栈失衡，
    //   进而触发 ImGui 的 "Calling PopStyleColor() too many times!" 断言。
    const bool customModeSelectedBeforeClick = spriteSheetExportUseCustomGroups_;
    if (customModeSelectedBeforeClick)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.75f, 0.90f));
    if (ImGui::Button("Custom Groups", ImVec2(130.0f, 44.0f)))
        spriteSheetExportUseCustomGroups_ = true;
    if (customModeSelectedBeforeClick)
        ImGui::PopStyleColor();

    ImGui::Separator();

    if (!spriteSheetExportUseCustomGroups_)
    {
        // 传统模式配置（保持原逻辑）。
        ImGui::TextUnformatted("Export range");
        int range = spriteSheetExportUseSelectedFrames_ ? 1 : 0;
        ImGui::RadioButton("All Frames", &range, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Selected Frames", &range, 1);
        spriteSheetExportUseSelectedFrames_ = (range == 1);

        if (spriteSheetExportMode_ == SpriteSheetExportMode::RowColumn)
        {
            ImGui::TextUnformatted("Grid config");
            ImGui::SetNextItemWidth(120.0f);
            ImGui::InputInt("Columns Per Row", &spriteSheetExportColumnsPerRow_);
            if (spriteSheetExportColumnsPerRow_ < 1)
                spriteSheetExportColumnsPerRow_ = 1;
        }

        // 预估输出尺寸，帮助用户在导出前确认布局结果。
        if (activeContext_ && activeContext_->hasProject())
        {
            Project* project = activeContext_->getProject();
            const int frameW = project->getWidth();
            const int frameH = project->getHeight();
            int frameCount = project->getFrameCount();
            if (spriteSheetExportUseSelectedFrames_)
                frameCount = static_cast<int>(activeContext_->getSelectedFrameIndices().size());

            frameCount = std::max(0, frameCount);
            int outW = 0;
            int outH = 0;
            if (frameCount > 0)
            {
                if (spriteSheetExportMode_ == SpriteSheetExportMode::Row)
                {
                    outW = frameW * frameCount;
                    outH = frameH;
                }
                else if (spriteSheetExportMode_ == SpriteSheetExportMode::Column)
                {
                    outW = frameW;
                    outH = frameH * frameCount;
                }
                else
                {
                    const int cols = std::min(frameCount, std::max(1, spriteSheetExportColumnsPerRow_));
                    const int rows = (frameCount + cols - 1) / cols;
                    outW = frameW * cols;
                    outH = frameH * rows;
                }
            }
            ImGui::Text("Preview: frames=%d, output=%dx%d", frameCount, outW, outH);
        }
    }
    else
    {
        // 分组自定义模式配置（读取时间轴分组）：
        // - 不再手工输入帧号；
        // - 引导用户在时间轴中 Ctrl 多选 + 右键创建分组。
        ImGui::TextUnformatted("Custom group settings");
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputInt("Group Spacing", &spriteSheetExportGroupSpacing_);
        if (spriteSheetExportGroupSpacing_ < 0)
            spriteSheetExportGroupSpacing_ = 0;

        const std::vector<AppContext::FrameGroup>* groups = activeContext_
            ? &activeContext_->getFrameGroups()
            : nullptr;
        if (!groups || groups->empty())
        {
            ImGui::Separator();
            ImGui::TextUnformatted("No timeline groups found.");
            ImGui::TextUnformatted("Create groups in Timeline: Ctrl+Click frames -> Right Click -> Group Selected Frames...");
        }
        else
        {
            ImGui::Separator();
            ImGui::Text("Groups from timeline: %d", static_cast<int>(groups->size()));
            for (size_t i = 0; i < groups->size(); ++i)
            {
                const AppContext::FrameGroup& group = (*groups)[i];
                ImGui::BulletText("%s  (frames: %d)", group.name.c_str(), static_cast<int>(group.frameIndices.size()));
            }
        }
    }

    ImGui::Separator();
    if (ImGui::Button("Export", ImVec2(120.0f, 0.0f)))
    {
        if (!spriteSheetExportUseCustomGroups_
            && spriteSheetExportUseSelectedFrames_
            && (!activeContext_ || activeContext_->getSelectedFrameIndices().empty()))
        {
            showError("No selected frames to export.");
        }
        else if (spriteSheetExportUseCustomGroups_)
        {
            // 先在弹窗内做一次解析校验，错误尽早反馈给用户。
            std::vector<SpriteSheetGroupResolved> resolvedGroups;
            std::string parseError;
            if (!buildResolvedSpriteGroups(resolvedGroups, parseError))
            {
                showError(parseError.empty() ? "Invalid custom groups." : parseError);
            }
            else
            {
                requestExportDialog(ExportKind::SpriteSheetConfiguredPng);
                ImGui::CloseCurrentPopup();
            }
        }
        else
        {
            requestExportDialog(ExportKind::SpriteSheetConfiguredPng);
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
    {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
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
    std::unique_ptr<Project> loadedProject = format == ProjectFileFormat::Json
        ? ProjectJsonSerializer::load(path, &error)
        : ProjectSerializer::load(path, &error);
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
    // 会话初始化时明确为单选第 0 帧，避免出现空选区。
    session.context->setSingleFrameSelection(0, session.project ? session.project->getFrameCount() : 1);
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
    // 新建项目默认选中第 0 帧。
    session.context->setSingleFrameSelection(0, session.project ? session.project->getFrameCount() : 1);
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
