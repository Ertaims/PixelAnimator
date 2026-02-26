/**
 * @file App.h
 * @brief 应用主类：负责初始化、主循环、窗口/菜单管理以及多项目会话管理
 *
 * 说明：
 * - App 是程序入口层的“总控”对象。
 * - 每个项目窗口对应一个独立 ProjectSession（项目数据 + 编辑上下文 + 窗口实例）。
 * - activeContext_ 始终指向“当前活跃窗口”的上下文，菜单命令也跟随它切换。
 */

#pragma once

#include "imgui.h"
#include <SDL3/SDL.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class MenuManager;
class Menu_File;
class Menu_Edit;
class ProjectWindow;
class Project;
class AppContext;

class App
{
public:
    App();
    ~App();

    // App 不允许拷贝，避免窗口/GL 上下文重复持有
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    // 初始化 SDL/OpenGL/ImGui，并创建菜单和默认项目窗口
    bool init();
    // 进入主循环，直到 done_ = true
    void run();
    // 按顺序释放资源
    void shutdown();

    // 访问“当前活跃项目”的上下文
    AppContext& getContext();
    const AppContext& getContext() const;

private:
    /**
     * @brief 一个项目会话对应一个独立窗口
     *
     * project: 业务数据（画布尺寸、帧序列、像素）
     * context: 编辑状态（当前帧、工具、缩放、脏标记等）
     * window:  该项目对应的 ProjectWindow
     */
    struct ProjectSession
    {
        std::unique_ptr<Project> project;
        std::unique_ptr<AppContext> context;
        ProjectWindow* window = nullptr;

        // 用于窗口标题和唯一 ImGui ID
        int projectId = 0;
        std::string windowBaseTitle; // 例如 "Project 2"
        std::string windowLabel;     // 例如 "Project 2*###ProjectWindow_2"
    };

    // ---------------- 主流程拆分 ----------------
    void processEvents();
    void renderFrame();
    bool createWindowAndContext();
    bool initImGui();
    void createMenuAndWindows();
    void setupDefaultDockLayout();

    // ---------------- New Project 弹窗 ----------------
    void renderNewProjectPopup();
    void createNewProject(int width,
                          int height,
                          int frameCount = 1,
                          uint32_t fillColor = 0x00000000,
                          bool checkerboardBackground = true);

    // ---------------- 活跃上下文与会话管理 ----------------
    void setActiveContext(AppContext* context);                         // 设置当前活跃窗口
    int findSessionIndexByContext(const AppContext* context) const;     // 查找指定上下文对应的会话索引
    void closeProjectByContext(AppContext* context);                    // 关闭指定上下文的项目
    void closeAllProjects();                                            // 关闭所有项目
    void refreshWindowLabels();                                         // 根据 dirty 状态更新窗口标题 *
    void handleProjectSwitchShortcut();                                 // Ctrl+Tab / Ctrl+Shift+Tab

    // ---------------- 平台与渲染状态 ----------------
    SDL_Window* window_ = nullptr;
    SDL_GLContext glContext_ = nullptr;
    const char* glslVersion_ = "#version 330";
    float mainScale_ = 1.0f;
    ImVec4 clearColor_ = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    bool done_ = false;

    // ---------------- 菜单与项目容器 ----------------
    MenuManager* menuManager_ = nullptr;
    Menu_File* fileMenu_ = nullptr;
    Menu_Edit* editMenu_ = nullptr;
    std::vector<ProjectSession> projectSessions_;
    AppContext* activeContext_ = nullptr;

    // ---------------- Dock 布局与编号 ----------------
    bool dockLayoutInitialized_ = false;
    int nextProjectId_ = 1;

    // ---------------- New Project UI 状态 ----------------
    bool newProjectPopupRequested_ = false;
    int newProjectWidth_ = 16;
    int newProjectHeight_ = 16;
    int newProjectFrameCount_ = 1;
    ImVec4 newProjectBgColor_ = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    int newProjectCanvasBgMode_ = 0; // 0=Checkerboard, 1=White
};
