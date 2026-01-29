#pragma once

#include "MenuOptionBase.h"

// Sprite 菜单类
class Menu_Sprite : public MenuOptionBase {
public:
    // 构造函数
    Menu_Sprite(Menu* menu);
    
    // 初始化菜单选项
    void initialize() override;
};