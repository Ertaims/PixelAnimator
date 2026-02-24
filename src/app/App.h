/**
 * @file App.h
 * @brief 应用主类：负责生命周期、主循环以及多项目窗口管理
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

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    bool init();
    void run();
    void shutdown();

    AppContext& getContext();
    const AppContext& getContext() const;

private:
    struct ProjectSession
    {
        std::unique_ptr<Project> project;
        std::unique_ptr<AppContext> context;
        ProjectWindow* window = nullptr;
        int projectId = 0;
        std::string windowBaseTitle;
        std::string windowLabel;
    };

    void processEvents();
    void renderFrame();
    bool createWindowAndContext();
    bool initImGui();
    void createMenuAndWindows();
    void setupDefaultDockLayout();
    void renderNewProjectPopup();
    void createNewProject(int width,
                          int height,
                          int frameCount = 1,
                          uint32_t fillColor = 0x00000000,
                          bool checkerboardBackground = true);
    void setActiveContext(AppContext* context);

    // 会话管理
    int findSessionIndexByContext(const AppContext* context) const;
    void closeProjectByContext(AppContext* context);
    void closeAllProjects();
    void refreshWindowLabels();
    void handleProjectSwitchShortcut();

    SDL_Window* window_ = nullptr;
    SDL_GLContext glContext_ = nullptr;
    const char* glslVersion_ = "#version 330";
    float mainScale_ = 1.0f;
    ImVec4 clearColor_ = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    bool done_ = false;

    MenuManager* menuManager_ = nullptr;
    Menu_File* fileMenu_ = nullptr;
    Menu_Edit* editMenu_ = nullptr;
    std::vector<ProjectSession> projectSessions_;
    AppContext* activeContext_ = nullptr;

    bool dockLayoutInitialized_ = false;
    int nextProjectId_ = 1;

    bool newProjectPopupRequested_ = false;
    int newProjectWidth_ = 16;
    int newProjectHeight_ = 16;
    int newProjectFrameCount_ = 1;
    ImVec4 newProjectBgColor_ = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    int newProjectCanvasBgMode_ = 0; // 0=Checkerboard, 1=White
};
