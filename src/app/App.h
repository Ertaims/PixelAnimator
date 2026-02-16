/**
 * @file App.h
 * @brief 应用程序主类，负责生命周期与主循环
 *
 * 职责：
 * - Init()：初始化 SDL、OpenGL、ImGui，创建菜单与窗口，持有 AppContext / MenuManager / WindowFactory。
 * - Run()：主循环（事件处理 → ImGui 帧 → 渲染），直到用户退出。
 * - Shutdown()：释放 ImGui、OpenGL 上下文、窗口，清理菜单与窗口工厂，退出 SDL。
 *
 * main.cpp 仅负责构造 App、调用 Init/Run/Shutdown，保持入口简洁。
 */

#pragma once

#include "../core/AppContext.h"
#include "imgui.h"
#include <SDL3/SDL.h>
#include <memory>

class MenuManager;
class ProjectWindow;
class Project;

class App
{
public:
    App();
    ~App();

    /** 禁止拷贝与赋值 */
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    /**
     * @brief 初始化 SDL、窗口、OpenGL、ImGui，并创建菜单与窗口
     * @return true 成功，false 失败（调用方应 return 1）
     */
    bool init();

    // 主循环：处理事件、渲染 UI，直到用户请求退出
    void run();

    // 释放资源：ImGui、OpenGL 上下文、窗口、菜单与窗口实例，SDL_Quit
    void shutdown();

    // 获取全局编辑器上下文，供菜单、窗口、命令读写状态 
    AppContext& getContext() { return context_; }

    // 只读访问
    const AppContext& getContext() const { return context_; }

private:
    // 处理本帧的 SDL 事件（含 ImGui 输入、退出请求）
    void processEvents();

    // 渲染一帧：菜单栏、DockSpace、各子窗口、Present
    void renderFrame();

    // 初始化 OpenGL 上下文与窗口（在 SDL 已初始化后调用）
    bool createWindowAndContext();

    // 初始化 ImGui 上下文、样式与后端（在窗口与 GL 上下文已创建后调用）
    bool initImGui();

    // 创建菜单栏与各窗口（在 ImGui 已初始化后调用）
    void createMenuAndWindows();

    // 初始化默认面板停靠布局（只执行一次）
    void setupDefaultDockLayout();

    // -------------------------------------------------------------------------
    // 平台与渲染状态
    // -------------------------------------------------------------------------
    SDL_Window* window_ = nullptr;
    SDL_GLContext glContext_ = nullptr;
    const char* glslVersion_ = "#version 330";
    float mainScale_ = 1.0f;
    ImVec4 clearColor_ = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    /** 主循环退出标志（由事件或菜单 Exit 设置） */
    bool done_ = false;

    // -------------------------------------------------------------------------
    // 编辑器状态与 UI
    // -------------------------------------------------------------------------
    AppContext context_;
    MenuManager* menuManager_ = nullptr;
    ProjectWindow* projectWindow_ = nullptr;
    std::unique_ptr<Project> activeProject_;
    bool dockLayoutInitialized_ = false;
};
