#ifndef WINDOW_H
#define WINDOW_H

#include "imgui.h"

class Window {
public:
    virtual ~Window() = default;
    
    // 渲染窗口
    virtual void render() = 0;
    
    // 检查窗口是否可见
    bool isVisible() const { return visible; }
    
    // 设置窗口可见性
    void setVisible(bool visible) { this->visible = visible; }
    
    // 获取窗口名称
    const char* getName() const { return name; }
    
protected:
    Window(const char* name) : name(name), visible(true) {}
    
    const char* name;
    bool visible;
};

#endif // WINDOW_H
