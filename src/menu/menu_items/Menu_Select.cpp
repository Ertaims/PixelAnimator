#include "Menu_Select.h"

// 构造函数
Menu_Select::Menu_Select(Menu* menu)
    : MenuOptionBase(menu) {
}

// 初始化菜单选项
void Menu_Select::initialize() {
    // 添加菜单项
    getMenu()->addItem("All", "Ctrl+A");
    getMenu()->addItem("Deselect", "Esc");
    getMenu()->addItem("Reselect", "Ctrl+Shift+D");
    getMenu()->addItem("Inverse", "Ctrl+Shift+I");
    
    getMenu()->addSeparator();
    
    getMenu()->addItem("Color Range");
    
    // 添加 Modify 子菜单
    Menu* modifyMenu = new Menu("Modify");
    getMenu()->addItem("Modify", modifyMenu);
    
    getMenu()->addSeparator();
    
    getMenu()->addItem("Load from MSK file");
    getMenu()->addItem("Save to MSK file");
}
