#include "WindowFactory.h"

ProjectWindow* WindowFactory::createProjectWindow(AppContext* context) {
    ProjectWindow* projectWindow = new ProjectWindow(context);
    windows.push_back(projectWindow);
    return projectWindow;
}

void WindowFactory::cleanup() {
    for (Window* window : windows) {
        delete window;
    }
    windows.clear();
}
