#ifndef PROJECTWINDOW_H
#define PROJECTWINDOW_H

#include "Window.h"
#include <functional>
#include <string>

class AppContext;
class Project;

class ProjectWindow : public Window {
public:
    ProjectWindow(AppContext* context,
                  const std::string& windowLabel,
                  const std::function<void(AppContext*)>& onFocused = {})
        : Window("ProjectWindow"), context(context), windowLabel_(windowLabel), onFocused_(onFocused) {}

    // 渲染窗口
    void render() override;

    const char* getWindowLabel() const { return windowLabel_.c_str(); }

private:
    void renderLeftPanel(Project* project);
    void renderCanvasPanel(Project* project);
    void renderRightPanel(Project* project);
    void renderTimelinePanel(Project* project);

    AppContext* context = nullptr;
    std::string windowLabel_;
    std::function<void(AppContext*)> onFocused_;
};

#endif // PROJECTWINDOW_H
