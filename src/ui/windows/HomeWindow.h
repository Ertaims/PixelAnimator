#ifndef HOMEWINDOW_H
#define HOMEWINDOW_H

#include "Window.h"

class HomeWindow : public Window {
public:
    HomeWindow() : Window("Home") {}
    
    // 渲染窗口
    void render() override;
    
    // 快速操作回调函数类型
    using ActionCallback = void(*)();
    
    // 设置新建项目回调
    void setNewProjectCallback(ActionCallback callback) { newProjectCallback = callback; }
    
    // 设置打开项目回调
    void setOpenProjectCallback(ActionCallback callback) { openProjectCallback = callback; }
    
    // 设置保存项目回调
    void setSaveProjectCallback(ActionCallback callback) { saveProjectCallback = callback; }
    
    // 设置导出动画回调
    void setExportAnimationCallback(ActionCallback callback) { exportAnimationCallback = callback; }
    
    // 设置教程回调
    void setTutorialCallback(ActionCallback callback) { tutorialCallback = callback; }
    
private:
    // 回调函数
    ActionCallback newProjectCallback = nullptr;
    ActionCallback openProjectCallback = nullptr;
    ActionCallback saveProjectCallback = nullptr;
    ActionCallback exportAnimationCallback = nullptr;
    ActionCallback tutorialCallback = nullptr;
};

#endif // HOMEWINDOW_H
