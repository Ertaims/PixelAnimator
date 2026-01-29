#pragma once

#include "MenuOptionBase.h"

// Frame 菜单类
class Menu_Frame : public MenuOptionBase {
public:
    // 构造函数
    Menu_Frame(Menu* menu);
    
    // 初始化菜单选项
    void initialize() override;
};