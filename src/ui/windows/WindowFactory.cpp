#include "WindowFactory.h"

HomeWindow* WindowFactory::createHomeWindow() {
    HomeWindow* homeWindow = new HomeWindow();
    windows.push_back(homeWindow);
    return homeWindow;
}

ProjectWindow* WindowFactory::createProjectWindow() {
    ProjectWindow* projectWindow = new ProjectWindow();
    windows.push_back(projectWindow);
    return projectWindow;
}

void WindowFactory::cleanup() {
    for (Window* window : windows) {
        delete window;
    }
    windows.clear();
}
