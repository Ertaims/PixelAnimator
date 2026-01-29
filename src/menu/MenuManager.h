#pragma once

#include "Menu.h"
#include <vector>

// 菜单管理器类
class MenuManager {
private:
    std::vector<Menu*> menus; // 菜单列表

public:
    // 构造函数
    MenuManager();
    
    // 析构函数
    ~MenuManager();
    
    // 添加菜单
    Menu* addMenu(const std::string& name, const std::string& shortcut = "", bool enabled = true);
    
    // 渲染所有菜单
    void render();
};