#include "Menu_Layer.h"

// 构造函数
Menu_Layer::Menu_Layer(Menu* menu)
    : MenuOptionBase(menu) {
}

// 初始化菜单选项
void Menu_Layer::initialize() {
    // 添加菜单项
    getMenu()->addItem("Properties...", "F2");
    getMenu()->addItem("Visible", nullptr, "Shift+X");
    getMenu()->addItem("Lock Layers");
    getMenu()->addItem("Open Group", "Shift+E");
    
    getMenu()->addSeparator();
    
    // 添加 New 子菜单
    Menu* newLayerMenu = new Menu("New");
    getMenu()->addItem("New...", newLayerMenu);
    
    getMenu()->addItem("Delete Layer");
    
    // 添加 Convert To 子菜单
    Menu* convertToMenu = new Menu("Convert To");
    getMenu()->addItem("Convert To...", convertToMenu);
    
    getMenu()->addSeparator();
    
    getMenu()->addItem("Duplicate");
    getMenu()->addItem("Flatten Down");
    getMenu()->addItem("Flatten Visible");
}
