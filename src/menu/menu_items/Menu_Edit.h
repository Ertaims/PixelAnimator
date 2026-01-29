#pragma once

#include "MenuOptionBase.h"

// Edit 菜单类
class Menu_Edit : public MenuOptionBase {
public:
    // 构造函数
    Menu_Edit(Menu* menu);
    
    // 初始化菜单选项
    void initialize() override;
};