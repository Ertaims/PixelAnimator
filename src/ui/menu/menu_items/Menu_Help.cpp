#include "Menu_Help.h"

// 构造函数
Menu_Help::Menu_Help(Menu* menu)
    : MenuOptionBase(menu) {
}

// 初始化菜单选项
void Menu_Help::initialize() {
    // 添加菜单项
    getMenu()->addItem("Readme");
    getMenu()->addItem("Quick Reference");
    getMenu()->addItem("Documentation");
    getMenu()->addItem("Tutorial");
    getMenu()->addItem("Release Notes");
    getMenu()->addItem("Twitter");
    
    getMenu()->addSeparator();
    
    getMenu()->addItem("About");
}
