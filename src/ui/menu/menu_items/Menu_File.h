#pragma once

#include "MenuOptionBase.h"
#include <functional>
#include <string>
#include <vector>

class AppContext;

class Menu_File : public MenuOptionBase {
public:
    Menu_File(Menu* menu,
              AppContext* context,
              const std::function<void()>& onExitCallback = nullptr,
              const std::function<void()>& onNewProjectCallback = nullptr,
              const std::function<void()>& onOpenProjectCallback = nullptr,
              const std::function<void()>& onSaveProjectCallback = nullptr,
              const std::function<void()>& onSaveAsBinaryProjectCallback = nullptr,
              const std::function<void()>& onSaveAsJsonProjectCallback = nullptr,
              const std::function<void()>& onExportCurrentFramePngCallback = nullptr,
              const std::function<void()>& onExportSpriteSheetConfigCallback = nullptr,
              const std::function<void()>& onImportSingleFramePngCallback = nullptr,
              const std::function<void()>& onImportSpriteSheetPngCallback = nullptr,
              const std::function<void(const std::string&)>& onOpenRecentProjectByPathCallback = nullptr,
              const std::function<void()>& onCloseProjectCallback = nullptr,
              const std::function<void()>& onCloseAllProjectsCallback = nullptr);

    void initialize() override;

    void setOnExitCallback(const std::function<void()>& callback);
    void setOnNewProjectCallback(const std::function<void()>& callback);
    void setOnOpenProjectCallback(const std::function<void()>& callback);
    void setOnSaveProjectCallback(const std::function<void()>& callback);
    void setOnSaveAsBinaryProjectCallback(const std::function<void()>& callback);
    void setOnSaveAsJsonProjectCallback(const std::function<void()>& callback);
    void setOnExportCurrentFramePngCallback(const std::function<void()>& callback);
    void setOnExportSpriteSheetConfigCallback(const std::function<void()>& callback);
    void setOnImportSingleFramePngCallback(const std::function<void()>& callback);
    void setOnImportSpriteSheetPngCallback(const std::function<void()>& callback);
    void setOnOpenRecentProjectByPathCallback(const std::function<void(const std::string&)>& callback);
    void setOnCloseProjectCallback(const std::function<void()>& callback);
    void setOnCloseAllProjectsCallback(const std::function<void()>& callback);
    void setRecentProjectPaths(const std::vector<std::string>& paths);
    void setContext(AppContext* context) { context_ = context; }

private:
    void rebuildOpenRecentMenu();

    AppContext* context_ = nullptr;
    Menu* openRecentMenu_ = nullptr;
    std::vector<std::string> recentProjectPaths_;

    std::function<void()> onExitCallback_;                  // Exit回调函数
    std::function<void()> onNewProjectCallback_;            // 新建项目回调函数
    std::function<void()> onOpenProjectCallback_;           // 打开项目回调函数
    std::function<void()> onSaveProjectCallback_;           // 保存项目回调函数
    std::function<void()> onSaveAsBinaryProjectCallback_;   // 另存为二进制项目回调函数
    std::function<void()> onSaveAsJsonProjectCallback_;     // 另存为 JSON 项目回调函数
    std::function<void()> onExportCurrentFramePngCallback_; // 导出当前帧 PNG 回调函数
    std::function<void()> onExportSpriteSheetConfigCallback_; // 打开精灵图导出配置弹窗回调函数
    std::function<void()> onImportSingleFramePngCallback_; // 导入单帧 PNG 回调函数
    std::function<void()> onImportSpriteSheetPngCallback_; // 导入精灵图 PNG 回调函数
    std::function<void(const std::string&)> onOpenRecentProjectByPathCallback_; // 按路径打开最近项目
    std::function<void()> onCloseProjectCallback_;          // 关闭项目回调函数
    std::function<void()> onCloseAllProjectsCallback_;      // 关闭所有项目回调函数
};
