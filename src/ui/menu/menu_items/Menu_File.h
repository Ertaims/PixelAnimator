#pragma once

#include "MenuOptionBase.h"
#include <functional>

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
              const std::function<void()>& onExportSpriteSheetRowAllPngCallback = nullptr,
              const std::function<void()>& onExportSpriteSheetRowSelectedPngCallback = nullptr,
              const std::function<void()>& onExportSpriteSheetColumnAllPngCallback = nullptr,
              const std::function<void()>& onExportSpriteSheetColumnSelectedPngCallback = nullptr,
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
    void setOnExportSpriteSheetRowAllPngCallback(const std::function<void()>& callback);
    void setOnExportSpriteSheetRowSelectedPngCallback(const std::function<void()>& callback);
    void setOnExportSpriteSheetColumnAllPngCallback(const std::function<void()>& callback);
    void setOnExportSpriteSheetColumnSelectedPngCallback(const std::function<void()>& callback);
    void setOnCloseProjectCallback(const std::function<void()>& callback);
    void setOnCloseAllProjectsCallback(const std::function<void()>& callback);
    void setContext(AppContext* context) { context_ = context; }

private:
    AppContext* context_ = nullptr;

    std::function<void()> onExitCallback_;                  // Exit回调函数
    std::function<void()> onNewProjectCallback_;            // 新建项目回调函数
    std::function<void()> onOpenProjectCallback_;           // 打开项目回调函数
    std::function<void()> onSaveProjectCallback_;           // 保存项目回调函数
    std::function<void()> onSaveAsBinaryProjectCallback_;   // 另存为二进制项目回调函数
    std::function<void()> onSaveAsJsonProjectCallback_;     // 另存为 JSON 项目回调函数
    std::function<void()> onExportCurrentFramePngCallback_; // 导出当前帧 PNG 回调函数
    std::function<void()> onExportSpriteSheetRowAllPngCallback_;       // 导出行排（全部帧）PNG 回调函数
    std::function<void()> onExportSpriteSheetRowSelectedPngCallback_;  // 导出行排（选中帧）PNG 回调函数
    std::function<void()> onExportSpriteSheetColumnAllPngCallback_;    // 导出列排（全部帧）PNG 回调函数
    std::function<void()> onExportSpriteSheetColumnSelectedPngCallback_; // 导出列排（选中帧）PNG 回调函数
    std::function<void()> onCloseProjectCallback_;          // 关闭项目回调函数
    std::function<void()> onCloseAllProjectsCallback_;      // 关闭所有项目回调函数
};
