#include "Menu_Sprite.h"

// 构造函数
Menu_Sprite::Menu_Sprite(Menu* menu)
    : MenuOptionBase(menu) {
}

// 初始化菜单选项
void Menu_Sprite::initialize() {
    // 添加菜单项
    getMenu()->addItem("Properties...", "Ctrl+P");
    
    // 添加 Color Mode 子菜单
    Menu* colorModeMenu = new Menu("Color Mode");
    getMenu()->addItem("Color Mode", colorModeMenu);
    
    getMenu()->addSeparator();
    
    getMenu()->addItem("Duplicate...", "Ctrl+Alt+D");
    getMenu()->addItem("Canvas Size...", "Ctrl+Alt+C");
    getMenu()->addItem("Rotate Canvas", "C");
    
    getMenu()->addSeparator();
    
    getMenu()->addItem("Crop");
    getMenu()->addItem("Trim");
}
