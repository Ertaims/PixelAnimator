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
    // 单例模式获取工厂实例
    static WindowFactory& getInstance() {
        static WindowFactory instance;
        return instance;
    }

    // 创建项目窗口
    ProjectWindow* createProjectWindow(AppContext* context,
                                       const std::string& windowLabel,
                                       const std::function<void(AppContext*)>& onFocused = {});

    // 获取所有窗口
    std::vector<Window*>& getWindows() { return windows; }

    // 清理所有窗口
    void cleanup();

private:
    // 私有构造函数，防止外部实例化
    WindowFactory() = default;

    // 禁止拷贝构造和赋值操作
    WindowFactory(const WindowFactory&) = delete;
    WindowFactory& operator=(const WindowFactory&) = delete;

    // 存储所有创建的窗口
    std::vector<Window*> windows;
};

#endif // WINDOWFACTORY_H
