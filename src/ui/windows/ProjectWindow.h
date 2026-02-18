#ifndef PROJECTWINDOW_H
#define PROJECTWINDOW_H

#include "Window.h"

class AppContext;
class Project;

class ProjectWindow : public Window {
public:
    explicit ProjectWindow(AppContext* context) : Window("Project"), context(context) {}
    
    // 渲染窗口
    void render() override;
    
private:
    void renderLeftPanel(Project* project);
    void renderCanvasPanel(Project* project);
    void renderRightPanel(Project* project);
    void renderTimelinePanel(Project* project);

    AppContext* context = nullptr;
};

#endif // PROJECTWINDOW_H
