#pragma once

#include "MenuOptionBase.h"

// Layer 菜单类
class Menu_Layer : public MenuOptionBase {
public:
    // 构造函数
    Menu_Layer(Menu* menu);
    
    // 初始化菜单选项
    void initialize() override;
};