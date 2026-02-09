#pragma once

#include "MenuBase.h"
#include "MenuItem.h"
#include <vector>

// 菜单类
class Menu : public MenuBase {
private:
    std::vector<MenuBase*> menuItems; // 菜单项列表

public:
    // 构造函数
    Menu(const std::string& name, const std::string& shortcut = "", bool enabled = true);
    
    // 析构函数
    ~Menu();
    
    // 添加菜单项
    MenuItem* addItem(const std::string& name, const std::string& shortcut = "", bool* isChecked = nullptr, bool enabled = true);
    MenuItem* addItem(const std::string& name, Menu* subMenu, const std::string& shortcut = "", bool enabled = true);
    
    // 添加子菜单
    Menu* addSubMenu(const std::string& name, const std::string& shortcut = "", bool enabled = true);
    
    // 添加分隔线
    void addSeparator();
    
    // 渲染菜单
    void render() override;
};