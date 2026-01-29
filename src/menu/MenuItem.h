#pragma once

#include "MenuBase.h"
#include <functional>

// 前向声明
class Menu;

// 菜单项类
class MenuItem : public MenuBase {
private:
    bool* isChecked;                    // 是否选中（可选）
    std::function<void()> callback;     // 点击回调函数
    Menu* subMenu;                      // 子菜单（如果有）

public:
    // 构造函数
    MenuItem(const std::string& name, const std::string& shortcut = "", bool* isChecked = nullptr, bool enabled = true);
    MenuItem(const std::string& name, Menu* subMenu, const std::string& shortcut = "", bool enabled = true);
    
    // 设置回调函数
    void setCallback(const std::function<void()>& callback);
    
    // 获取子菜单
    Menu* getSubMenu() const;
    
    // 渲染菜单项
    void render() override;
};