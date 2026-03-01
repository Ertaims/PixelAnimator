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
    std::function<void()> onExitCallback_;
    std::function<void()> onNewProjectCallback_;
    std::function<void()> onOpenProjectCallback_;
    std::function<void()> onSaveProjectCallback_;
    std::function<void()> onSaveAsProjectCallback_;
    std::function<void()> onCloseProjectCallback_;
    std::function<void()> onCloseAllProjectsCallback_;
};
