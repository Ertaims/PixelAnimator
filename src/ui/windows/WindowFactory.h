#ifndef WINDOWFACTORY_H
#define WINDOWFACTORY_H

#include "ProjectWindow.h"
#include "Window.h"
#include <functional>
#include <string>
#include <vector>

class AppContext;

class WindowFactory {
public:
    static WindowFactory& getInstance() {
        static WindowFactory instance;
        return instance;
    }

    ProjectWindow* createProjectWindow(AppContext* context,
                                       const std::string& windowLabel,
                                       const std::function<void(AppContext*)>& onFocused = {});

    std::vector<Window*>& getWindows() { return windows; }

    // 释放一个窗口并从列表移除
    void destroyWindow(Window* target);

    // 释放所有窗口
    void cleanup();

private:
    WindowFactory() = default;
    WindowFactory(const WindowFactory&) = delete;
    WindowFactory& operator=(const WindowFactory&) = delete;

    std::vector<Window*> windows;
};

#endif // WINDOWFACTORY_H
