#pragma once

#include "MenuOptionBase.h"

// Select 菜单类
class Menu_Select : public MenuOptionBase {
public:
    // 构造函数
    Menu_Select(Menu* menu);
    
    // 初始化菜单选项
    void initialize() override;
};