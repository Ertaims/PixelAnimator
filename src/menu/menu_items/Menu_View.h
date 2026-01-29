#pragma once

#include "MenuOptionBase.h"

// View 菜单类
class Menu_View : public MenuOptionBase {
public:
    // 构造函数
    Menu_View(Menu* menu);
    
    // 初始化菜单选项
    void initialize() override;
};