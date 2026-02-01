#pragma once

#include "MenuOptionBase.h"
#include <functional>

// File 菜单类
class Menu_File : public MenuOptionBase {
private:
    std::function<void()> onExitCallback; // 退出回调函数

public:
    // 构造函数
    Menu_File(Menu* menu, const std::function<void()>& onExitCallback = nullptr);

    // 初始化菜单项
    void initialize() override;
    
    // 设置退出回调函数
    void setOnExitCallback(const std::function<void()>& callback);
};