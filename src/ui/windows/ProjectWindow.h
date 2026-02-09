#ifndef PROJECTWINDOW_H
#define PROJECTWINDOW_H

#include "Window.h"

class ProjectWindow : public Window {
public:
    ProjectWindow() : Window("Project") {} 
    
    // 渲染窗口
    void render() override;
    
    // 设置项目名称
    void setProjectName(const char* name) { projectName = name; }
    
    // 获取项目名称
    const char* getProjectName() const { return projectName; }
    
    // 设置项目尺寸
    void setProjectSize(int width, int height) {
        this->width = width;
        this->height = height;
    }
    
    // 获取项目尺寸
    void getProjectSize(int& width, int& height) const {
        width = this->width;
        height = this->height;
    }
    
private:
    const char* projectName = "Untitled";
    int width = 128;
    int height = 128;
};

#endif // PROJECTWINDOW_H
