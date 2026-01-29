#include "Menu_View.h"

// 构造函数
Menu_View::Menu_View(Menu* menu)
    : MenuOptionBase(menu) {
}

// 初始化菜单选项
void Menu_View::initialize() {
    // 添加菜单项
    getMenu()->addItem("Duplicate View");
    getMenu()->addItem("Workspace Layout", "Shift+W");
    getMenu()->addItem("Run Command", "Ctrl+Space");
    getMenu()->addItem("Extras", nullptr, "Ctrl+H");
    getMenu()->addItem("Show");
    
    getMenu()->addSeparator();
    
    getMenu()->addItem("Grid");
    
    // 添加 Tiled Mode 子菜单
    Menu* tiledModeMenu = new Menu("Tiled Mode");
    getMenu()->addItem("Tiled Mode", tiledModeMenu);
    
    getMenu()->addItem("Symmetry Options");
    
    getMenu()->addSeparator();
    
    getMenu()->addItem("Set Loop Section", "F2");
    getMenu()->addItem("Show Onion Skin", "F3");
    getMenu()->addItem("Timeline", nullptr, "Tab");
    getMenu()->addItem("Preview");
    
    getMenu()->addSeparator();
    
    getMenu()->addItem("Advanced Mode", "Ctrl+F");
    getMenu()->addItem("Full Screen", "F11");
    getMenu()->addItem("Full Screen Preview", "F8");
    getMenu()->addItem("Home");
    
    getMenu()->addSeparator();
    
    getMenu()->addItem("Refresh & Reload Theme", "F5");
}
