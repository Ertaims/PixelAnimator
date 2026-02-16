#ifndef PROJECTWINDOW_H
#define PROJECTWINDOW_H

#include "Window.h"

class AppContext;

class ProjectWindow : public Window {
public:
    explicit ProjectWindow(AppContext* context) : Window("Project"), context(context) {}
    
    // 渲染窗口
    void render() override;
    
private:
    AppContext* context = nullptr;
};

#endif // PROJECTWINDOW_H
