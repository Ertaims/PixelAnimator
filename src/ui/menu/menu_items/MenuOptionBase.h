#pragma once

#include "..\Menu.h"

// 菜单选项基类
class MenuOptionBase {
protected:
    Menu* menu; // 对应的菜单

public:
    // 构造函数
    MenuOptionBase(Menu* menu);
    
    // 析构函数
    virtual ~MenuOptionBase() = default;
    
    // 初始化菜单选项
    virtual void initialize() = 0;
    
    // 获取菜单
    Menu* getMenu() const;
};