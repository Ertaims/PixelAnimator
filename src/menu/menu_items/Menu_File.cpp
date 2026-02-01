#include "Menu_File.h"

// 构造函数
Menu_File::Menu_File(Menu* menu, const std::function<void()>& onExitCallback)
    : MenuOptionBase(menu), onExitCallback(onExitCallback) {
}

void Menu_File::initialize() {
    // 添加菜单项
    getMenu()->addItem("New...", "Ctrl+N");
    getMenu()->addItem("Open...", "Ctrl+O");
    
    // 添加 Open Recent 子菜单
    Menu* openRecentMenu = new Menu("Open Recent");
    getMenu()->addItem("Open Recent", openRecentMenu);
    
    getMenu()->addSeparator();
    
    getMenu()->addItem("Save", "Ctrl+S");
    getMenu()->addItem("Save As...", "Ctrl+Shift+S");
    
    getMenu()->addSeparator();
    
    getMenu()->addItem("Close", "Ctrl+W");
    getMenu()->addItem("Close All", "Ctrl+Shift+W");
    
    getMenu()->addSeparator();
    
    // 添加 Export 子菜单
    Menu* exportMenu = new Menu("Export");
    getMenu()->addItem("Export", exportMenu);
    
    // 添加 Import 子菜单
    Menu* importMenu = new Menu("Import");
    getMenu()->addItem("Import", importMenu);
    
    getMenu()->addSeparator();
    
    // 添加 Exit 菜单项，设置回调函数
    MenuItem* exitItem = getMenu()->addItem("Exit", "Ctrl+Q");
    if (onExitCallback) {
        exitItem->setCallback(onExitCallback);
    }
}

// 设置退出回调函数
void Menu_File::setOnExitCallback(const std::function<void()>& callback) {
    onExitCallback = callback;
}
