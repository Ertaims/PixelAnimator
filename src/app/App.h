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
#include "commands/PixelClipboardCommands.h"
#include <SDL3/SDL.h>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class MenuManager;
class Menu_File;
class Menu_Edit;
class ProjectWindow;
class Project;
class AppContext;
class SDL_Surface;

class App
{
public:
    enum class ProjectFileFormat
    {
        Binary,
        Json
    };

    enum class ExportKind
    {
        CurrentFramePng,
        SpriteSheetConfiguredPng
    };

    enum class ImportKind
    {
        CurrentFramePng,
        SpriteSheetPng
    };

    /**
     * @brief 精灵图导入策略。
     */
    enum class SpriteSheetImportStrategy
    {
        AppendAfterCurrent = 0, // 追加到当前帧后
        ReplaceAllFrames,       // 替换当前项目所有帧
        NewProject              // 导入为新项目
    };

    /**
     * @brief 精灵图导入后自动分组策略。
     */
    enum class SpriteSheetImportGrouping
    {
        None = 0, // 不自动建组
        ByRow,    // 每一行切片建一个组
        ByColumn  // 每一列切片建一个组
    };

    /**
     * @brief 精灵图导出模式（对应弹窗中的三种布局）。
     */
    enum class SpriteSheetExportMode
    {
        Row = 0,      // 所有帧一行
        Column,       // 所有帧一列
        RowColumn     // 按“每行列数”自动换行
    };

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
     * @brief 精灵图“自定义分组”在弹窗中的编辑态数据。
     *
     * 说明：
     * - frameListText 使用文本输入，便于用户快速录入，如 "1,2,3,8"；
     * - mode 表示该组内部布局（行排或列排）；
     * - name 用于导出配置识别与后续扩展（当前不强制唯一）。
     */
    struct SpriteSheetGroupDraft
    {
        std::string name = "Group";
        SpriteSheetExportMode mode = SpriteSheetExportMode::Row;
        std::string frameListText = "1";
    };

    /**
     * @brief 精灵图“自定义分组”在导出执行前的解析结果。
     *
     * 说明：
     * - frameIndices 为 0-based 帧索引；
     * - 由弹窗录入文本解析得到，确保导出层拿到的是可直接使用的结构。
     */
    struct SpriteSheetGroupResolved
    {
        std::string name;
        SpriteSheetExportMode mode = SpriteSheetExportMode::Row;
        std::vector<int> frameIndices;
    };

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
    // 事件处理
    void processEvents();
    // 渲染
    void renderFrame();
    // 创建窗口和上下文
    bool createWindowAndContext();
    // 初始化 ImGui
    bool initImGui();
    // 创建菜单和项目窗口
    void createMenuAndWindows();
    // 设置默认的 Dock Layout
    void setupDefaultDockLayout();

    // ---------------- New Project 弹窗 ----------------
    // 渲染新建窗口弹窗
    void renderNewProjectPopup();
    // 渲染错误弹窗
    void renderErrorPopup();
    // 渲染精灵图导出配置弹窗
    void renderSpriteSheetExportPopup();
    // 轮询结果
    void pollDialogResults();
    // 创建打开项目弹窗
    void requestOpenProjectDialog();
    // 创建保存项目弹窗
    void requestSaveAsDialog(ProjectFileFormat format);
    // 创建导出图片弹窗
    void requestExportDialog(ExportKind kind);
    // 创建导入图片弹窗
    void requestImportDialog(ImportKind kind);
    // 创建新项目
    void createNewProject(int width,
                          int height,
                          int frameCount = 1,
                          uint32_t fillColor = 0x00000000,
                          bool checkerboardBackground = true);
    // 另存项目
    bool saveProjectAs(AppContext* context, const std::string& path, ProjectFileFormat preferredFormat);
    // 保存当前项目
    bool saveActiveProject();
    // 另存当前项目
    bool saveActiveProjectAs(const std::string& path, ProjectFileFormat preferredFormat);
    // 导出当前项目
    bool exportToPath(const std::string& path,
                      ExportKind kind,
                      SpriteSheetExportMode spriteMode,
                      bool useSelectedFrames,
                      int columnsPerRow,
                      bool useCustomGroups,
                      const std::vector<SpriteSheetGroupResolved>& customGroups,
                      int groupSpacing);
    // 从路径导入项目
    bool importFromPath(const std::string& path,
                        ImportKind kind,
                        bool spriteSheetRowMajor);
    // 渲染精灵图导入弹窗
    void renderSpriteSheetImportPopup();
    // 解析帧列表文本
    bool parseFrameListText(const std::string& text,
                            int maxFrameCount,
                            std::vector<int>& outIndices,
                            std::string& outError) const;
    // 构建解析结果
    bool buildResolvedSpriteGroups(std::vector<SpriteSheetGroupResolved>& outGroups, std::string& outError) const;
    // 打开项目
    bool openProjectFromPath(const std::string& path);
    // 创建会话
    void createSessionFromProject(std::unique_ptr<Project> project, const std::string& projectPath);
    // 显示错误
    void showError(const std::string& message);
    void addRecentProjectPath(const std::string& path);
    void refreshRecentProjectsMenu();
    std::string getRecentProjectsStoragePath() const;
    void loadRecentProjectPaths();
    void saveRecentProjectPaths() const;
    // 窗口事件处理
    static void SDLCALL onOpenDialogClosed(void* userdata, const char* const* filelist, int filter);
    // 保存项目弹窗
    static void SDLCALL onSaveDialogClosed(void* userdata, const char* const* filelist, int filter);
    static void SDLCALL onExportDialogClosed(void* userdata, const char* const* filelist, int filter);
    static void SDLCALL onImportDialogClosed(void* userdata, const char* const* filelist, int filter);

    // ---------------- 活跃上下文与会话管理 ----------------
    void setActiveContext(AppContext* context);                         // 设置当前活跃窗口
    int findSessionIndexByContext(const AppContext* context) const;     // 查找指定上下文对应的会话索引
    void closeProjectByContext(AppContext* context);                    // 关闭指定上下文的项目
    void closeAllProjects();                                            // 关闭所有项目
    void refreshWindowLabels();                                         // 根据 dirty 状态更新窗口标题 *
    void handleProjectSwitchShortcut();                                 // Ctrl+Tab / Ctrl+Shift+Tab
    void handleFileMenuShortcuts();                                     // File 菜单全局快捷键
    void handleEditMenuShortcuts();                                     // Edit 菜单全局快捷键
    void handleToolShortcuts();                                         // 工具切换快捷键
    void renderUndoHistoryPopup();                                      // Undo History 弹窗
    bool executeCutSelection();                                         // Cut（有像素选区时生效）
    bool executeCopySelection();                                        // Copy（有像素选区时生效）
    bool executePasteSelection();                                       // Paste（有像素选区时生效）

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
    std::vector<std::string> recentProjectPaths_;
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

    // ---------------- Sprite Sheet 导出弹窗状态 ----------------
    bool spriteSheetExportPopupRequested_ = false;
    SpriteSheetExportMode spriteSheetExportMode_ = SpriteSheetExportMode::Row;
    bool spriteSheetExportUseSelectedFrames_ = false;
    int spriteSheetExportColumnsPerRow_ = 4;
    bool spriteSheetExportUseCustomGroups_ = false;
    int spriteSheetExportGroupSpacing_ = 8;
    std::vector<SpriteSheetGroupDraft> spriteSheetExportGroups_;
    bool spriteSheetExportIconsLoaded_ = false;
    unsigned int spriteSheetRowIconTexture_ = 0;
    unsigned int spriteSheetColumnIconTexture_ = 0;
    unsigned int spriteSheetRowColumnIconTexture_ = 0;

    // ---------------- Sprite Sheet 导入弹窗状态 ----------------
    bool spriteSheetImportPopupRequested_ = false;
    std::string spriteSheetImportPendingPath_;
    bool spriteSheetImportRowMajor_ = true;
    bool spriteSheetImportUseGridCountMode_ = false;
    bool spriteSheetImportUseCustomSlice_ = false;
    int spriteSheetImportSliceWidth_ = 16;
    int spriteSheetImportSliceHeight_ = 16;
    int spriteSheetImportGridRows_ = 1;
    int spriteSheetImportGridCols_ = 1;
    SpriteSheetImportStrategy spriteSheetImportStrategy_ = SpriteSheetImportStrategy::AppendAfterCurrent;
    SpriteSheetImportGrouping spriteSheetImportGrouping_ = SpriteSheetImportGrouping::None;
    int spriteSheetImportPreviewWidth_ = 0;
    int spriteSheetImportPreviewHeight_ = 0;
    int spriteSheetImportPreviewColumns_ = 0;
    int spriteSheetImportPreviewRows_ = 0;
    int spriteSheetImportPreviewFrames_ = 0;
    unsigned int spriteSheetImportPreviewTexture_ = 0;
    std::vector<uint8_t> spriteSheetImportTileSelected_;

    // ---------------- Open/Save 最小可用弹窗状态 ----------------
    bool openDialogInFlight_ = false;
    bool saveDialogInFlight_ = false;
    bool exportDialogInFlight_ = false;
    bool importDialogInFlight_ = false;
    std::mutex dialogMutex_;
    std::string pendingOpenPath_;
    std::string pendingSavePath_;
    std::string pendingDialogError_;
    std::string pendingExportPath_;
    std::string pendingImportPath_;
    bool pendingOpenReady_ = false;
    bool pendingSaveReady_ = false;
    bool pendingExportReady_ = false;
    bool pendingImportReady_ = false;
    ProjectFileFormat pendingSaveFormat_ = ProjectFileFormat::Binary;
    ProjectFileFormat saveDialogFormat_ = ProjectFileFormat::Binary;
    ExportKind pendingExportKind_ = ExportKind::CurrentFramePng;
    ExportKind exportDialogKind_ = ExportKind::CurrentFramePng;
    ImportKind pendingImportKind_ = ImportKind::CurrentFramePng;
    ImportKind importDialogKind_ = ImportKind::CurrentFramePng;
    SpriteSheetExportMode pendingExportSpriteMode_ = SpriteSheetExportMode::Row;
    SpriteSheetExportMode exportDialogSpriteMode_ = SpriteSheetExportMode::Row;
    bool pendingExportUseSelectedFrames_ = false;
    bool exportDialogUseSelectedFrames_ = false;
    int pendingExportColumnsPerRow_ = 4;
    int exportDialogColumnsPerRow_ = 4;
    bool pendingExportUseCustomGroups_ = false;
    bool exportDialogUseCustomGroups_ = false;
    int pendingExportGroupSpacing_ = 8;
    int exportDialogGroupSpacing_ = 8;
    std::vector<SpriteSheetGroupResolved> pendingExportCustomGroups_;
    std::vector<SpriteSheetGroupResolved> exportDialogCustomGroups_;
    bool pendingImportSpriteSheetRowMajor_ = true;
    bool importDialogSpriteSheetRowMajor_ = true;
    bool pendingDialogErrorReady_ = false;
    std::string pendingErrorMessage_;
    bool undoHistoryPopupRequested_ = false;
    commands::PixelClipboardData pixelClipboard_;
};
