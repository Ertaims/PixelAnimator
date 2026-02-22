#include "WindowFactory.h"

ProjectWindow* WindowFactory::createProjectWindow(AppContext* context,
                                                  const std::string& windowLabel,
                                                  const std::function<void(AppContext*)>& onFocused) {
    ProjectWindow* projectWindow = new ProjectWindow(context, windowLabel, onFocused);
    windows.push_back(projectWindow);
    return projectWindow;
}

void WindowFactory::cleanup() {
    for (Window* window : windows) {
        delete window;
    }
    windows.clear();
}
