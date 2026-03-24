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
              const std::function<void()>& onSaveAsProjectCallback = nullptr,
              const std::function<void()>& onCloseProjectCallback = nullptr,
              const std::function<void()>& onCloseAllProjectsCallback = nullptr);

    void initialize() override;

    void setOnExitCallback(const std::function<void()>& callback);
    void setOnNewProjectCallback(const std::function<void()>& callback);
    void setOnOpenProjectCallback(const std::function<void()>& callback);
    void setOnSaveProjectCallback(const std::function<void()>& callback);
    void setOnSaveAsProjectCallback(const std::function<void()>& callback);
    void setOnCloseProjectCallback(const std::function<void()>& callback);
    void setOnCloseAllProjectsCallback(const std::function<void()>& callback);
    void setContext(AppContext* context) { context_ = context; }

private:
    AppContext* context_ = nullptr;

    std::function<void()> onExitCallback_;                  // Exit回调函数
    std::function<void()> onNewProjectCallback_;            // 新建项目回调函数
    std::function<void()> onOpenProjectCallback_;           // 打开项目回调函数
    std::function<void()> onSaveProjectCallback_;           // 保存项目回调函数
    std::function<void()> onSaveAsProjectCallback_;         // 另存为项目回调函数
    std::function<void()> onCloseProjectCallback_;          // 关闭项目回调函数
    std::function<void()> onCloseAllProjectsCallback_;      // 关闭所有项目回调函数
};
