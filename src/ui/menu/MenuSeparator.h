#pragma once

#include "MenuBase.h"

// 菜单分隔线类
class MenuSeparator : public MenuBase {
public:
    // 构造函数
    MenuSeparator();
    
    // 渲染分隔线
    void render() override;
};