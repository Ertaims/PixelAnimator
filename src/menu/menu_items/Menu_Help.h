#pragma once

#include "MenuOptionBase.h"

// Help 菜单类
class Menu_Help : public MenuOptionBase {
public:
    // 构造函数
    Menu_Help(Menu* menu);
    
    // 初始化菜单选项
    void initialize() override;
};