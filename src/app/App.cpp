/**
 * @file App.cpp
 * @brief App 主流程实现：初始化、主循环渲染、资源释放、多项目会话切换
 */

#include "app/App.h"

#include "core/AppContext.h"
#include "core/Project.h"
#include "io/ImageExporter.h"
#include "io/ImageImporter.h"
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
#include <fstream>
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
            if (!stem.empty()) return stem;
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
        if (text.size() < suffix.size()) return false;
        const std::string lower = toLowerCopy(text);
        return lower.compare(lower.size() - suffix.size(), suffix.size(), suffix.data()) == 0;
    }

    App::ProjectFileFormat detectFormatFromPath(const std::string& path)
    {
        if (endsWithInsensitive(path, ".pxanim.json") || endsWithInsensitive(path, ".json")) return App::ProjectFileFormat::Json;
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
        if (path.empty()) return path;

        if (preferredFormat == App::ProjectFileFormat::Json)
        {
            if (endsWithInsensitive(path, ".pxanim.json") || endsWithInsensitive(path, ".json")) return path;
            if (endsWithInsensitive(path, ".pxanim")) return path + ".json";
            return path + ".pxanim.json";
        }

        // Binary:
        if (endsWithInsensitive(path, ".pxanim")) return path;
        if (endsWithInsensitive(path, ".pxanim.json")) return path.substr(0, path.size() - 5); // 去掉末尾 ".json" -> ".pxanim"
        if (endsWithInsensitive(path, ".json")) return path.substr(0, path.size() - 5) + ".pxanim";
        return path + ".pxanim";
    }

    std::string normalizePngPath(const std::string& path)
    {
        if (path.empty()) return path;
        if (endsWithInsensitive(path, ".png")) return path;
        return path + ".png";
    }

    /**
     * @brief 从文件加载 OpenGL 纹理（用于导出模式图标按钮）。
     */
    unsigned int loadTextureFromFile(const char* path)
    {
        SDL_Surface* surface = IMG_Load(path);
        if (!surface) return 0;

        SDL_Surface* rgbaSurface = surface;
        if (surface->format != SDL_PIXELFORMAT_RGBA32)
        {
            rgbaSurface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
            SDL_DestroySurface(surface);
            if (!rgbaSurface) return 0;
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

    /**
     * @brief 导入自动分组时使用的高区分度调色板。
     *
     * 说明：
     * - 与时间轴分组视觉方案保持一致风格；
     * - 按组索引循环取色，避免所有组显示成同一种颜色。
     */
    uint32_t importGroupColorByIndex(size_t index)
    {
        static const uint32_t kPalette[] = {
            0x5EA1FFFFu, // 天蓝
            0x6ED6A0FFu, // 薄荷绿
            0xF2B566FFu, // 橙黄
            0xC98CFFFFu, // 紫粉
            0x82D8F4FFu, // 青蓝
            0xF48AA1FFu, // 珊瑚粉
            0xA2D56DFFu, // 黄绿
            0xF4D36AFFu  // 暖黄
        };
        return kPalette[index % (sizeof(kPalette) / sizeof(kPalette[0]))];
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
        [this]() { requestImportDialog(ImportKind::CurrentFramePng); },
        [this]() { requestImportDialog(ImportKind::SpriteSheetPng); },
        [this](const std::string& path) { openProjectFromPath(path); },
        [this]() { closeProjectByContext(activeContext_); },
        [this]() { closeAllProjects(); });
    refreshRecentProjectsMenu();

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
        if (event.type == SDL_EVENT_QUIT) done_ = true;

        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED
            && event.window.windowID == SDL_GetWindowID(window_))
            done_ = true;
        
        if (event.type == SDL_EVENT_DROP_FILE)
        {
            // 只处理主窗口上的拖拽
            if (event.drop.windowID != SDL_GetWindowID(window_)) continue;

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

    if (menuManager_) menuManager_->render();

    // 每帧更新：New Project 弹窗、快捷切换、窗口标题脏标记
    pollDialogResults();
    renderNewProjectPopup();
    renderSpriteSheetExportPopup();
    renderSpriteSheetImportPopup();
    renderErrorPopup();
    handleFileMenuShortcuts();
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
    // 退出前持久化 Recent 列表，确保本次会话更新不会丢失。
    saveRecentProjectPaths();

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
    if (spriteSheetImportPreviewTexture_ != 0)
    {
        glDeleteTextures(1, &spriteSheetImportPreviewTexture_);
        spriteSheetImportPreviewTexture_ = 0;
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
    if (dockLayoutInitialized_) return;

    ImGuiID dockspaceId = ImGui::GetID("MyDockSpace");
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

    for (const ProjectSession& session : projectSessions_)
    {
        if (session.window) ImGui::DockBuilderDockWindow(session.windowLabel.c_str(), dockspaceId);
    }

    ImGui::DockBuilderFinish(dockspaceId);
    dockLayoutInitialized_ = true;
}

void App::setActiveContext(AppContext* context)
{
    // 统一切换“当前活动上下文”，菜单命令也同步切换目标
    activeContext_ = context;
    if (fileMenu_) fileMenu_->setContext(activeContext_);
    if (editMenu_) editMenu_->setContext(activeContext_);
}

int App::findSessionIndexByContext(const AppContext* context) const
{
    if (!context) return -1;

    for (int i = 0; i < static_cast<int>(projectSessions_.size()); ++i)
    {
        if (projectSessions_[static_cast<size_t>(i)].context.get() == context) return i;
    }
    return -1;
}

void App::closeProjectByContext(AppContext* context)
{
    const int index = findSessionIndexByContext(context);
    if (index < 0) return;

    ProjectSession& session = projectSessions_[static_cast<size_t>(index)];
    Window* windowToDestroy = session.window;
    if (windowToDestroy) WindowFactory::getInstance().destroyWindow(windowToDestroy);

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
        if (session.window) WindowFactory::getInstance().destroyWindow(session.window);
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
    if (projectSessions_.size() < 2) return;

    ImGuiIO& io = ImGui::GetIO();
    if (!io.KeyCtrl) return;
    if (!ImGui::IsKeyPressed(ImGuiKey_Tab, false)) return;

    int currentIndex = findSessionIndexByContext(activeContext_);
    if (currentIndex < 0) currentIndex = 0;

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

    if (!ImGui::BeginPopupModal("New Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

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
    if (newProjectWidth_ < 1) newProjectWidth_ = 1;
    if (newProjectHeight_ < 1) newProjectHeight_ = 1;
    if (newProjectFrameCount_ < 1) newProjectFrameCount_ = 1;

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
    if (openDialogInFlight_) return;

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
    if (saveDialogInFlight_) return;

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
    if (exportDialogInFlight_) return;
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
    if (project && !project->getName().empty()) baseName = project->getName();

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
        if (spriteSheetExportUseCustomGroups_) modeText = "grouped";
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

void App::requestImportDialog(ImportKind kind)
{
    if (importDialogInFlight_) return;
    if (!activeContext_ || !activeContext_->hasProject())
    {
        showError("No active project to import into.");
        return;
    }

    importDialogKind_ = kind;
    importDialogSpriteSheetRowMajor_ = spriteSheetImportRowMajor_;
    static const SDL_DialogFileFilter filters[] = {
        {"PNG Image", "png"},
        {"All Files", "*"}
    };

    importDialogInFlight_ = true;
    SDL_ShowOpenFileDialog(
        &App::onImportDialogClosed,
        this,
        window_,
        filters,
        2,
        nullptr,
        false);
}

void SDLCALL App::onOpenDialogClosed(void* userdata, const char* const* filelist, int filter)
{
    (void)filter;
    App* app = static_cast<App*>(userdata);
    if (!app) return;

    std::lock_guard<std::mutex> guard(app->dialogMutex_);
    app->openDialogInFlight_ = false;

    if (!filelist)
    {
        app->pendingDialogError_ = SDL_GetError();
        if (app->pendingDialogError_.empty()) app->pendingDialogError_ = "Open dialog failed.";
        app->pendingDialogErrorReady_ = true;
        return;
    }

    if (!filelist[0]) return; // 用户取消

    app->pendingOpenPath_ = filelist[0];
    app->pendingOpenReady_ = true;
}

void SDLCALL App::onSaveDialogClosed(void* userdata, const char* const* filelist, int filter)
{
    (void)filter;
    App* app = static_cast<App*>(userdata);
    if (!app) return;

    std::lock_guard<std::mutex> guard(app->dialogMutex_);
    app->saveDialogInFlight_ = false;

    if (!filelist)
    {
        app->pendingDialogError_ = SDL_GetError();
        if (app->pendingDialogError_.empty()) app->pendingDialogError_ = "Save dialog failed.";
        app->pendingDialogErrorReady_ = true;
        return;
    }

    if (!filelist[0]) return; // 用户取消

    app->pendingSavePath_ = filelist[0];
    app->pendingSaveReady_ = true;
    app->pendingSaveFormat_ = app->saveDialogFormat_;
}

void SDLCALL App::onExportDialogClosed(void* userdata, const char* const* filelist, int filter)
{
    (void)filter;
    App* app = static_cast<App*>(userdata);
    if (!app) return;

    std::lock_guard<std::mutex> guard(app->dialogMutex_);
    app->exportDialogInFlight_ = false;

    if (!filelist)
    {
        app->pendingDialogError_ = SDL_GetError();
        if (app->pendingDialogError_.empty()) app->pendingDialogError_ = "Export dialog failed.";
        app->pendingDialogErrorReady_ = true;
        return;
    }

    if (!filelist[0]) return;

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

void SDLCALL App::onImportDialogClosed(void* userdata, const char* const* filelist, int filter)
{
    (void)filter;
    App* app = static_cast<App*>(userdata);
    if (!app) return;

    std::lock_guard<std::mutex> guard(app->dialogMutex_);
    app->importDialogInFlight_ = false;

    if (!filelist)
    {
        app->pendingDialogError_ = SDL_GetError();
        if (app->pendingDialogError_.empty()) app->pendingDialogError_ = "Import dialog failed.";
        app->pendingDialogErrorReady_ = true;
        return;
    }

    if (!filelist[0]) return;

    app->pendingImportPath_ = filelist[0];
    app->pendingImportKind_ = app->importDialogKind_;
    app->pendingImportSpriteSheetRowMajor_ = app->importDialogSpriteSheetRowMajor_;
    app->pendingImportReady_ = true;
}

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
        if (pendingImportReady_)
        {
            importPath = pendingImportPath_;
            pendingImportPath_.clear();
            pendingImportReady_ = false;
            importKind = pendingImportKind_;
            importSpriteSheetRowMajor = pendingImportSpriteSheetRowMajor_;
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
    if (!pendingErrorMessage_.empty())
    {
        ImGui::OpenPopup("Error");
    }

    if (!ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

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

void App::handleFileMenuShortcuts()
{
    ImGuiIO& io = ImGui::GetIO();

    // 正在编辑文本时不处理全局文件快捷键，避免与输入冲突。
    if (io.WantTextInput) return;
    if (!io.KeyCtrl) return;

    // 优先处理带 Shift 的组合，避免与 Ctrl+S / Ctrl+W 冲突。
    if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S, false))
    {
        // Ctrl+Shift+S：Save As。
        // 若当前项目已有路径，则沿用当前格式（Binary/Json）；否则默认 Binary。
        ProjectFileFormat format = ProjectFileFormat::Binary;
        if (activeContext_ && !activeContext_->getProjectFilePath().empty()) format = detectFormatFromPath(activeContext_->getProjectFilePath());
        requestSaveAsDialog(format);
        return;
    }
    if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_W, false))
    {
        // Ctrl+Shift+W：Close All
        closeAllProjects();
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_N, false))
    {
        // Ctrl+N：New Project
        newProjectPopupRequested_ = true;
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_O, false))
    {
        // Ctrl+O：Open Project
        requestOpenProjectDialog();
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_S, false))
    {
        // Ctrl+S：Save
        saveActiveProject();
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_W, false))
    {
        // Ctrl+W：Close
        closeProjectByContext(activeContext_);
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Q, false))
    {
        // Ctrl+Q：Exit
        done_ = true;
        return;
    }
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
    addRecentProjectPath(finalPath);
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
    if (spriteMode == SpriteSheetExportMode::Column) layout = ImageExporter::SpriteSheetLayout::Column;
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

bool App::importFromPath(const std::string& path, ImportKind kind, bool spriteSheetRowMajor)
{
    if (!activeContext_ || !activeContext_->hasProject())
    {
        showError("No active project to import into.");
        return false;
    }

    Project* project = activeContext_->getProject();
    if (!project)
    {
        showError("No project data to import into.");
        return false;
    }

    if (path.empty())
    {
        showError("Import path is empty.");
        return false;
    }

    if (kind == ImportKind::CurrentFramePng)
    {
        // 单帧导入：直接写入当前主帧，尺寸必须与画布一致。
        std::string error;
        const int targetFrame = activeContext_->getCurrentFrameIndex();
        if (!ImageImporter::importSingleFramePng(*project, targetFrame, path, &error))
        {
            showError(error.empty() ? "Failed to import current frame." : error);
            return false;
        }
        activeContext_->setProjectDirty(true);
        return true;
    }

    // 精灵图导入先进入配置弹窗，允许用户配置切片尺寸、导入策略、自动分组。
    spriteSheetImportPendingPath_ = path;
    spriteSheetImportRowMajor_ = spriteSheetRowMajor;
    spriteSheetImportUseGridCountMode_ = false;
    spriteSheetImportUseCustomSlice_ = false;
    spriteSheetImportStrategy_ = SpriteSheetImportStrategy::AppendAfterCurrent;
    spriteSheetImportGrouping_ = SpriteSheetImportGrouping::None;
    spriteSheetImportPreviewWidth_ = 0;
    spriteSheetImportPreviewHeight_ = 0;
    spriteSheetImportPreviewColumns_ = 0;
    spriteSheetImportPreviewRows_ = 0;
    spriteSheetImportPreviewFrames_ = 0;

    SDL_Surface* surface = IMG_Load(path.c_str());
    if (!surface)
    {
        showError(std::string("Failed to load sprite sheet: ") + SDL_GetError());
        return false;
    }
    spriteSheetImportPreviewWidth_ = surface->w;
    spriteSheetImportPreviewHeight_ = surface->h;
    SDL_DestroySurface(surface);

    // 载入整张精灵图纹理，用于弹窗中的切片缩图预览。
    // 每次重新选择导入文件时都会重建纹理，避免旧图残留。
    if (spriteSheetImportPreviewTexture_ != 0)
    {
        glDeleteTextures(1, &spriteSheetImportPreviewTexture_);
        spriteSheetImportPreviewTexture_ = 0;
    }
    spriteSheetImportPreviewTexture_ = loadTextureFromFile(path.c_str());
    spriteSheetImportTileSelected_.clear();

    // 默认切片尺寸跟随当前画布尺寸，用户可在弹窗中改为自定义值。
    spriteSheetImportSliceWidth_ = std::max(1, project->getWidth());
    spriteSheetImportSliceHeight_ = std::max(1, project->getHeight());
    // 默认行列数按“整图 / 画布”估算；若不能整除则回退为 1x1。
    spriteSheetImportGridCols_ = 1;
    spriteSheetImportGridRows_ = 1;
    if (spriteSheetImportPreviewWidth_ % spriteSheetImportSliceWidth_ == 0) spriteSheetImportGridCols_ = std::max(1, spriteSheetImportPreviewWidth_ / spriteSheetImportSliceWidth_);
    if (spriteSheetImportPreviewHeight_ % spriteSheetImportSliceHeight_ == 0) spriteSheetImportGridRows_ = std::max(1, spriteSheetImportPreviewHeight_ / spriteSheetImportSliceHeight_);

    if (spriteSheetImportPreviewWidth_ % spriteSheetImportSliceWidth_ == 0
        && spriteSheetImportPreviewHeight_ % spriteSheetImportSliceHeight_ == 0)
    {
        spriteSheetImportPreviewColumns_ = spriteSheetImportPreviewWidth_ / spriteSheetImportSliceWidth_;
        spriteSheetImportPreviewRows_ = spriteSheetImportPreviewHeight_ / spriteSheetImportSliceHeight_;
        spriteSheetImportPreviewFrames_ = spriteSheetImportPreviewColumns_ * spriteSheetImportPreviewRows_;
    }

    spriteSheetImportPopupRequested_ = true;
    return true;
}

void App::renderSpriteSheetImportPopup()
{
    if (spriteSheetImportPopupRequested_)
    {
        ImGui::OpenPopup("Import Sprite Sheet");
        spriteSheetImportPopupRequested_ = false;
    }

    // 让弹窗尺寸可随窗口空间自适应：
    // - 出现时给一个较舒适的初始尺寸；
    // - 同时设置最小/最大尺寸约束，避免过小挤压或过大超出可视区域。
    const ImVec2 workSize = ImGui::GetMainViewport()->WorkSize;
    const ImVec2 minPopupSize(620.0f, 500.0f);
    const ImVec2 maxPopupSize(std::max(minPopupSize.x, workSize.x * 0.95f),
                              std::max(minPopupSize.y, workSize.y * 0.95f));
    const ImVec2 initialPopupSize(std::min(maxPopupSize.x, 860.0f),
                                  std::min(maxPopupSize.y, 760.0f));
    ImGui::SetNextWindowSizeConstraints(minPopupSize, maxPopupSize);
    ImGui::SetNextWindowSize(initialPopupSize, ImGuiCond_Appearing);

    if (!ImGui::BeginPopupModal("Import Sprite Sheet", nullptr, ImGuiWindowFlags_None)) return;

    ImGui::TextUnformatted("Traversal Order");
    int order = spriteSheetImportRowMajor_ ? 0 : 1;
    ImGui::RadioButton("Row-major (Left->Right, Top->Bottom)", &order, 0);
    ImGui::RadioButton("Column-major (Top->Bottom, Left->Right)", &order, 1);
    spriteSheetImportRowMajor_ = (order == 0);

    ImGui::Separator();
    ImGui::TextUnformatted("Slice Size");
    // 切片配置提供两种方式：
    // 按“每帧尺寸”输入（Width/Height）
    // 按“行列数量”输入（Rows/Columns），自动计算每帧尺寸
    int sliceMode = spriteSheetImportUseGridCountMode_ ? 1 : 0;
    ImGui::RadioButton("By Frame Size", &sliceMode, 0);
    ImGui::SameLine();
    ImGui::RadioButton("By Rows / Columns", &sliceMode, 1);
    spriteSheetImportUseGridCountMode_ = (sliceMode == 1);

    if (!spriteSheetImportUseGridCountMode_)
    {
        ImGui::Checkbox("Use Custom Slice Size", &spriteSheetImportUseCustomSlice_);
        if (spriteSheetImportUseCustomSlice_)
        {
            ImGui::SetNextItemWidth(120.0f);
            ImGui::InputInt("Slice Width", &spriteSheetImportSliceWidth_);
            ImGui::SetNextItemWidth(120.0f);
            ImGui::InputInt("Slice Height", &spriteSheetImportSliceHeight_);
            spriteSheetImportSliceWidth_ = std::max(1, spriteSheetImportSliceWidth_);
            spriteSheetImportSliceHeight_ = std::max(1, spriteSheetImportSliceHeight_);
        }
        else if (activeContext_ && activeContext_->hasProject())
        {
            Project* project = activeContext_->getProject();
            spriteSheetImportSliceWidth_ = std::max(1, project->getWidth());
            spriteSheetImportSliceHeight_ = std::max(1, project->getHeight());
        }
    }
    else
    {
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputInt("Rows", &spriteSheetImportGridRows_);
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputInt("Columns", &spriteSheetImportGridCols_);
        spriteSheetImportGridRows_ = std::max(1, spriteSheetImportGridRows_);
        spriteSheetImportGridCols_ = std::max(1, spriteSheetImportGridCols_);
    }

    // 统一计算“本次将实际使用”的切片尺寸：
    // - 按尺寸模式：直接使用 Slice Width/Height
    // - 按行列模式：由整图尺寸 / 行列数自动推导
    int effectiveSliceWidth = spriteSheetImportSliceWidth_;
    int effectiveSliceHeight = spriteSheetImportSliceHeight_;
    bool effectiveSliceValid = false;
    if (spriteSheetImportUseGridCountMode_)
    {
        if (spriteSheetImportGridCols_ > 0
            && spriteSheetImportGridRows_ > 0
            && spriteSheetImportPreviewWidth_ > 0
            && spriteSheetImportPreviewHeight_ > 0
            && spriteSheetImportPreviewWidth_ % spriteSheetImportGridCols_ == 0
            && spriteSheetImportPreviewHeight_ % spriteSheetImportGridRows_ == 0)
        {
            effectiveSliceWidth = spriteSheetImportPreviewWidth_ / spriteSheetImportGridCols_;
            effectiveSliceHeight = spriteSheetImportPreviewHeight_ / spriteSheetImportGridRows_;
            effectiveSliceValid = true;
        }
    }
    else
    {
        if (effectiveSliceWidth > 0
            && effectiveSliceHeight > 0
            && spriteSheetImportPreviewWidth_ > 0
            && spriteSheetImportPreviewHeight_ > 0
            && spriteSheetImportPreviewWidth_ % effectiveSliceWidth == 0
            && spriteSheetImportPreviewHeight_ % effectiveSliceHeight == 0)
        {
            effectiveSliceValid = true;
        }
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Import Strategy");
    int strategy = static_cast<int>(spriteSheetImportStrategy_);
    ImGui::RadioButton("Append After Current", &strategy, static_cast<int>(SpriteSheetImportStrategy::AppendAfterCurrent));
    ImGui::RadioButton("Replace All Frames", &strategy, static_cast<int>(SpriteSheetImportStrategy::ReplaceAllFrames));
    ImGui::RadioButton("Import As New Project", &strategy, static_cast<int>(SpriteSheetImportStrategy::NewProject));
    spriteSheetImportStrategy_ = static_cast<SpriteSheetImportStrategy>(strategy);

    ImGui::Separator();
    ImGui::TextUnformatted("Auto Group Imported Frames");
    int grouping = static_cast<int>(spriteSheetImportGrouping_);
    ImGui::RadioButton("None", &grouping, static_cast<int>(SpriteSheetImportGrouping::None));
    ImGui::RadioButton("Group By Row", &grouping, static_cast<int>(SpriteSheetImportGrouping::ByRow));
    ImGui::RadioButton("Group By Column", &grouping, static_cast<int>(SpriteSheetImportGrouping::ByColumn));
    spriteSheetImportGrouping_ = static_cast<SpriteSheetImportGrouping>(grouping);

    ImGui::Separator();
    ImGui::Text("Sheet: %dx%d", spriteSheetImportPreviewWidth_, spriteSheetImportPreviewHeight_);
    ImGui::Text("Slice: %dx%d", effectiveSliceWidth, effectiveSliceHeight);
    if (effectiveSliceValid)
    {
        spriteSheetImportPreviewColumns_ = spriteSheetImportPreviewWidth_ / effectiveSliceWidth;
        spriteSheetImportPreviewRows_ = spriteSheetImportPreviewHeight_ / effectiveSliceHeight;
        spriteSheetImportPreviewFrames_ = spriteSheetImportPreviewColumns_ * spriteSheetImportPreviewRows_;
        ImGui::Text("Detected Grid: %d x %d (frames=%d)",
                    spriteSheetImportPreviewColumns_,
                    spriteSheetImportPreviewRows_,
                    spriteSheetImportPreviewFrames_);
    }
    else
    {
        ImGui::TextUnformatted("Detected Grid: invalid (sheet size not divisible by current slice config)");
    }

    // ---------------- 切片缩图预览 + 选择性导入 ----------------
    // 规则：
    // 默认全选；
    // 点击缩图可切换该切片是否导入；
    // 提供全选/清空/反选快捷按钮。
    int selectedTileCount = 0;
    if (effectiveSliceValid && spriteSheetImportPreviewFrames_ > 0)
    {
        if (spriteSheetImportTileSelected_.size() != static_cast<size_t>(spriteSheetImportPreviewFrames_))
        {
            spriteSheetImportTileSelected_.assign(static_cast<size_t>(spriteSheetImportPreviewFrames_), 1);
        }

        for (uint8_t flag : spriteSheetImportTileSelected_)
        {
            if (flag != 0) ++selectedTileCount;
        }

        ImGui::Separator();
        ImGui::Text("Selectable Tiles: %d / %d", selectedTileCount, spriteSheetImportPreviewFrames_);
        if (ImGui::Button("Select All"))
        {
            std::fill(spriteSheetImportTileSelected_.begin(), spriteSheetImportTileSelected_.end(), static_cast<uint8_t>(1));
            selectedTileCount = spriteSheetImportPreviewFrames_;
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear"))
        {
            std::fill(spriteSheetImportTileSelected_.begin(), spriteSheetImportTileSelected_.end(), static_cast<uint8_t>(0));
            selectedTileCount = 0;
        }
        ImGui::SameLine();
        if (ImGui::Button("Invert"))
        {
            for (uint8_t& flag : spriteSheetImportTileSelected_)
                flag = flag == 0 ? 1 : 0;
            selectedTileCount = 0;
            for (uint8_t flag : spriteSheetImportTileSelected_)
            {
                if (flag != 0) ++selectedTileCount;
            }
        }

        // 预览区高度根据当前弹窗可用空间动态分配，避免窗口拉伸时布局僵硬。
        const float footerReserve = ImGui::GetFrameHeightWithSpacing() + 20.0f; // 预留 Import/Cancel 区域
        const float previewHeight = std::clamp(ImGui::GetContentRegionAvail().y - footerReserve, 160.0f, 420.0f);
        ImGui::BeginChild("##SpriteSheetTilePreview",
                          ImVec2(0.0f, previewHeight),
                          true,
                          ImGuiWindowFlags_HorizontalScrollbar);
        const float tilePreviewSize = 52.0f;
        const float cardWidth = 74.0f; // 卡片宽度（包含缩图与编号），用于统一对齐
        const float spacingX = ImGui::GetStyle().ItemSpacing.x;
        const float availWidth = ImGui::GetContentRegionAvail().x;
        int tilesPerRow = static_cast<int>((availWidth + spacingX) / (cardWidth + spacingX));
        tilesPerRow = std::max(1, tilesPerRow);

        // 预览布局策略：
        // - 按行列模式：严格按用户输入列数显示；
        // - 其它模式：按可用宽度自适应列数。
        if (spriteSheetImportUseGridCountMode_ && effectiveSliceValid) tilesPerRow = std::max(1, spriteSheetImportGridCols_);

        tilesPerRow = std::min(tilesPerRow, spriteSheetImportPreviewFrames_);

        if (ImGui::BeginTable("##SpriteSheetTileTable",
                              tilesPerRow,
                              ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_SizingFixedFit))
        {
            for (int tileIndex = 0; tileIndex < spriteSheetImportPreviewFrames_; ++tileIndex)
            {
                if (tileIndex % tilesPerRow == 0) ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(tileIndex % tilesPerRow);

                // tileIndex 为“预览顺序索引”，映射到源图中的行列用于 UV 裁剪。
                const int sourceRow = tileIndex / spriteSheetImportPreviewColumns_;
                const int sourceCol = tileIndex % spriteSheetImportPreviewColumns_;

                ImGui::PushID(tileIndex);
                const bool selected = spriteSheetImportTileSelected_[static_cast<size_t>(tileIndex)] != 0;
                if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.75f, 0.90f));

                bool clicked = false;
                const float imageOffsetX = std::max(0.0f, (cardWidth - tilePreviewSize) * 0.5f);
                const float baseX = ImGui::GetCursorPosX();
                ImGui::SetCursorPosX(baseX + imageOffsetX);

                if (spriteSheetImportPreviewTexture_ != 0)
                {
                    const ImVec2 uv0(
                        static_cast<float>(sourceCol * effectiveSliceWidth) / static_cast<float>(spriteSheetImportPreviewWidth_),
                        static_cast<float>(sourceRow * effectiveSliceHeight) / static_cast<float>(spriteSheetImportPreviewHeight_));
                    const ImVec2 uv1(
                        static_cast<float>((sourceCol + 1) * effectiveSliceWidth) / static_cast<float>(spriteSheetImportPreviewWidth_),
                        static_cast<float>((sourceRow + 1) * effectiveSliceHeight) / static_cast<float>(spriteSheetImportPreviewHeight_));
                    clicked = ImGui::ImageButton("##tile",
                                                 reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(spriteSheetImportPreviewTexture_)),
                                                 ImVec2(tilePreviewSize, tilePreviewSize),
                                                 uv0,
                                                 uv1);
                }
                else
                {
                    clicked = ImGui::Button("Tile", ImVec2(tilePreviewSize, tilePreviewSize));
                }

                if (selected) ImGui::PopStyleColor();

                if (clicked)
                {
                    uint8_t& flag = spriteSheetImportTileSelected_[static_cast<size_t>(tileIndex)];
                    flag = flag == 0 ? 1 : 0;
                }

                char label[16] = {};
                std::snprintf(label, sizeof(label), "#%d", tileIndex + 1);
                const float labelWidth = ImGui::CalcTextSize(label).x;
                ImGui::SetCursorPosX(baseX + std::max(0.0f, (cardWidth - labelWidth) * 0.5f));
                ImGui::TextUnformatted(label);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();

        // 缩图点击后重新统计一次，保证下方 Import 校验读到最新值。
        selectedTileCount = 0;
        for (uint8_t flag : spriteSheetImportTileSelected_)
        {
            if (flag != 0) ++selectedTileCount;
        }
    }

    ImGui::Separator();
    if (ImGui::Button("Import", ImVec2(120.0f, 0.0f)))
    {
        if (!effectiveSliceValid)
        {
            showError("Invalid slice config. Please check slice size or rows/columns.");
            ImGui::EndPopup();
            return;
        }
        if (spriteSheetImportPreviewFrames_ > 0 && selectedTileCount <= 0)
        {
            showError("No tiles selected to import.");
            ImGui::EndPopup();
            return;
        }

        if (!activeContext_ || !activeContext_->hasProject())
        {
            showError("No active project to import into.");
        }
        else
        {
            Project* project = activeContext_->getProject();
            std::string error;
            ImageImporter::SpriteSheetSliceResult sliceResult;
            if (!ImageImporter::sliceSpriteSheetPng(spriteSheetImportPendingPath_,
                                                    effectiveSliceWidth,
                                                    effectiveSliceHeight,
                                                    spriteSheetImportRowMajor_,
                                                    sliceResult,
                                                    &error))
            {
                showError(error.empty() ? "Failed to import sprite sheet." : error);
            }
            else
            {
                // 先根据“切片选择状态”过滤导入帧。
                // filteredIndexMap 记录“过滤后帧”对应的原始切片索引，后续分组需要用到。
                std::vector<std::vector<uint32_t>> filteredFrames;
                std::vector<int> filteredTileRows;
                std::vector<int> filteredTileCols;
                filteredFrames.reserve(sliceResult.frames.size());
                filteredTileRows.reserve(sliceResult.frames.size());
                filteredTileCols.reserve(sliceResult.frames.size());

                for (size_t i = 0; i < sliceResult.frames.size(); ++i)
                {
                    // 选择状态以“物理网格位置（row,col）”为准，而非切片遍历顺序。
                    // 这样在 column-major 模式下，预览勾选与实际导入也能一一对应。
                    const int physicalIndex = sliceResult.tileRows[i] * spriteSheetImportPreviewColumns_
                        + sliceResult.tileCols[i];
                    const bool selected = physicalIndex >= 0
                        && physicalIndex < static_cast<int>(spriteSheetImportTileSelected_.size())
                        ? spriteSheetImportTileSelected_[static_cast<size_t>(physicalIndex)] != 0
                        : true;
                    if (!selected) continue;

                    filteredFrames.push_back(sliceResult.frames[i]);
                    filteredTileRows.push_back(sliceResult.tileRows[i]);
                    filteredTileCols.push_back(sliceResult.tileCols[i]);
                }

                if (filteredFrames.empty())
                {
                    showError("No tiles selected to import.");
                    spriteSheetImportPendingPath_.clear();
                    ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                    return;
                }

                AppContext* targetContext = activeContext_;
                Project* targetProject = project;
                int firstImportedIndex = 0;

                // 策略 1：追加到当前帧后。
                if (spriteSheetImportStrategy_ == SpriteSheetImportStrategy::AppendAfterCurrent)
                {
                    const int insertAfter = targetContext->getCurrentFrameIndex();
                    int anchor = insertAfter;
                    for (size_t i = 0; i < filteredFrames.size(); ++i)
                    {
                        targetProject->insertFrameAfter(anchor, 0x00000000);
                        ++anchor;
                        targetProject->getFrame(anchor).pixels = filteredFrames[i];
                        targetContext->onFrameInserted(anchor, -1, targetProject->getFrameCount());
                    }
                    firstImportedIndex = insertAfter + 1;
                }

                // 策略 2：替换当前项目全部帧（并把画布尺寸切到导入切片尺寸）。
                if (spriteSheetImportStrategy_ == SpriteSheetImportStrategy::ReplaceAllFrames)
                {
                    targetProject->resizeCanvas(effectiveSliceWidth, effectiveSliceHeight, 0x00000000);
                    targetProject->setFrameCount(static_cast<int>(filteredFrames.size()), 0x00000000);
                    for (size_t i = 0; i < filteredFrames.size(); ++i)
                        targetProject->getFrame(static_cast<int>(i)).pixels = filteredFrames[i];
                    targetContext->clearFrameGroups();
                    firstImportedIndex = 0;
                }

                // 策略 3：导入为新项目。
                if (spriteSheetImportStrategy_ == SpriteSheetImportStrategy::NewProject)
                {
                    std::unique_ptr<Project> newProject = std::make_unique<Project>(
                        effectiveSliceWidth,
                        effectiveSliceHeight,
                        static_cast<int>(filteredFrames.size()),
                        0x00000000);
                    newProject->setName(projectNameFromPath(spriteSheetImportPendingPath_));
                    for (size_t i = 0; i < filteredFrames.size(); ++i)
                        newProject->getFrame(static_cast<int>(i)).pixels = filteredFrames[i];

                    createSessionFromProject(std::move(newProject), "");
                    targetContext = activeContext_;
                    targetProject = targetContext ? targetContext->getProject() : nullptr;
                    firstImportedIndex = 0;
                }

                if (!targetContext || !targetProject)
                {
                    showError("Failed to apply imported frames.");
                    spriteSheetImportPendingPath_.clear();
                    ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                    return;
                }

                // 自动分组（可选）：
                // - ByRow：同一切片行建一个分组
                // - ByColumn：同一切片列建一个分组
                if (spriteSheetImportGrouping_ != SpriteSheetImportGrouping::None)
                {
                    targetContext->clearFrameGroups();

                    if (spriteSheetImportGrouping_ == SpriteSheetImportGrouping::ByRow)
                    {
                        for (int row = 0; row < sliceResult.rows; ++row)
                        {
                            std::vector<int> groupFrames;
                            for (size_t i = 0; i < filteredFrames.size(); ++i)
                            {
                                if (filteredTileRows[i] == row) groupFrames.push_back(firstImportedIndex + static_cast<int>(i));
                            }
                            if (!groupFrames.empty())
                            {
                                // 每个组按序号分配不同颜色，保证时间轴上肉眼可区分。
                                const uint32_t groupColor = importGroupColorByIndex(static_cast<size_t>(row));
                                targetContext->addFrameGroup("Row " + std::to_string(row + 1),
                                                             groupFrames,
                                                             targetProject->getFrameCount(),
                                                             groupColor);
                            }
                        }
                    }
                    else
                    {
                        for (int col = 0; col < sliceResult.columns; ++col)
                        {
                            std::vector<int> groupFrames;
                            for (size_t i = 0; i < filteredFrames.size(); ++i)
                            {
                                if (filteredTileCols[i] == col) groupFrames.push_back(firstImportedIndex + static_cast<int>(i));
                            }
                            if (!groupFrames.empty())
                            {
                                // 列分组同样按列号映射颜色，避免“所有组同色”。
                                const uint32_t groupColor = importGroupColorByIndex(static_cast<size_t>(col));
                                targetContext->addFrameGroup("Column " + std::to_string(col + 1),
                                                             groupFrames,
                                                             targetProject->getFrameCount(),
                                                             groupColor);
                            }
                        }
                    }
                }

                targetContext->setSingleFrameSelection(firstImportedIndex, targetProject->getFrameCount());
                targetContext->setProjectDirty(true);
                spriteSheetImportPendingPath_.clear();
                ImGui::CloseCurrentPopup();
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
    {
        spriteSheetImportPendingPath_.clear();
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
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
    // 单值：1, 5, 12
    // 区间：3-7（闭区间）
    // 混合：1,2,5-8,10
    // 分隔符支持逗号/空格/分号，统一先归一化为逗号再解析。
    std::string normalized = text;
    for (char& ch : normalized)
    {
        if (ch == ' ' || ch == ';' || ch == '\t' || ch == '\n' || ch == '\r') ch = ',';
    }

    std::vector<bool> used(static_cast<size_t>(maxFrameCount), false);
    std::stringstream ss(normalized);
    std::string token;
    while (std::getline(ss, token, ','))
    {
        if (token.empty()) continue;

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

        if (startValue > endValue) std::swap(startValue, endValue);

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
        if (group.frameIndices.empty()) continue;

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

    if (!ImGui::BeginPopupModal("Export Sprite Sheet", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

    // 首次打开时加载模式图标（row/column/row&column）。
    if (!spriteSheetExportIconsLoaded_)
    {
        const char* rowCandidates[] = {"src/assets/row.png", "../src/assets/row.png", "../../src/assets/row.png"};
        const char* colCandidates[] = {"src/assets/column.png", "../src/assets/column.png", "../../src/assets/column.png"};
        const char* rowColCandidates[] = {"src/assets/row&column.png", "../src/assets/row&column.png", "../../src/assets/row&column.png"};

        for (const char* p : rowCandidates)
        {
            spriteSheetRowIconTexture_ = loadTextureFromFile(p);
            if (spriteSheetRowIconTexture_ != 0) break;
        }
        for (const char* p : colCandidates)
        {
            spriteSheetColumnIconTexture_ = loadTextureFromFile(p);
            if (spriteSheetColumnIconTexture_ != 0) break;
        }
        for (const char* p : rowColCandidates)
        {
            spriteSheetRowColumnIconTexture_ = loadTextureFromFile(p);
            if (spriteSheetRowColumnIconTexture_ != 0) break;
        }
        spriteSheetExportIconsLoaded_ = true;
    }

    ImGui::TextUnformatted("Select sprite sheet mode");
    ImGui::Separator();

    auto drawModeButton = [this](SpriteSheetExportMode mode, unsigned int texture, const char* fallbackLabel) {
        const bool selected = (!spriteSheetExportUseCustomGroups_ && spriteSheetExportMode_ == mode);
        if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.75f, 0.90f));

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

        if (selected) ImGui::PopStyleColor();

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
    if (customModeSelectedBeforeClick) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.75f, 0.90f));
    if (ImGui::Button("Custom Groups", ImVec2(130.0f, 44.0f))) spriteSheetExportUseCustomGroups_ = true;
    if (customModeSelectedBeforeClick) ImGui::PopStyleColor();

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
            if (spriteSheetExportColumnsPerRow_ < 1) spriteSheetExportColumnsPerRow_ = 1;
        }

        // 预估输出尺寸，帮助用户在导出前确认布局结果。
        if (activeContext_ && activeContext_->hasProject())
        {
            Project* project = activeContext_->getProject();
            const int frameW = project->getWidth();
            const int frameH = project->getHeight();
            int frameCount = project->getFrameCount();
            if (spriteSheetExportUseSelectedFrames_) frameCount = static_cast<int>(activeContext_->getSelectedFrameIndices().size());

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
        if (spriteSheetExportGroupSpacing_ < 0) spriteSheetExportGroupSpacing_ = 0;

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

    if (loadedProject->getName().empty()) loadedProject->setName(projectNameFromPath(path));

    createSessionFromProject(std::move(loadedProject), path);
    addRecentProjectPath(path);
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

void App::addRecentProjectPath(const std::string& path)
{
    if (path.empty()) return;

    // 仅记录可再次打开的项目文件，避免把 PNG 导入/导出路径混入 Open Recent。
    if (!isSupportedProjectPath(path)) return;

    const std::string lowerPath = toLowerCopy(path);
    recentProjectPaths_.erase(
        std::remove_if(recentProjectPaths_.begin(),
                       recentProjectPaths_.end(),
                       [&lowerPath](const std::string& item) {
                           return toLowerCopy(item) == lowerPath;
                       }),
        recentProjectPaths_.end());

    recentProjectPaths_.insert(recentProjectPaths_.begin(), path);
    static constexpr size_t kRecentLimit = 12;
    if (recentProjectPaths_.size() > kRecentLimit) recentProjectPaths_.resize(kRecentLimit);

    refreshRecentProjectsMenu();
    // 每次 Recent 变更后立即落盘，避免异常退出导致数据丢失。
    saveRecentProjectPaths();
}

void App::refreshRecentProjectsMenu()
{
    if (fileMenu_) fileMenu_->setRecentProjectPaths(recentProjectPaths_);
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
    recentProjectPaths_.clear();
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
            recentProjectPaths_.begin(),
            recentProjectPaths_.end(),
            [&lowerLine](const std::string& item) { return toLowerCopy(item) == lowerLine; });
        if (existed) continue;

        recentProjectPaths_.push_back(line);
        if (recentProjectPaths_.size() >= kRecentLimit) break;
    }
}

void App::saveRecentProjectPaths() const
{
    const std::string storagePath = getRecentProjectsStoragePath();
    std::ofstream output(storagePath, std::ios::trunc);
    if (!output.is_open()) return;

    for (const std::string& path : recentProjectPaths_)
    {
        if (path.empty()) continue;
        output << path << '\n';
    }
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

