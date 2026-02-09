#include "MenuOptionBase.h"

// 构造函数
MenuOptionBase::MenuOptionBase(Menu* menu)
    : menu(menu) {
}

// 获取菜单
Menu* MenuOptionBase::getMenu() const {
    return menu;
}
