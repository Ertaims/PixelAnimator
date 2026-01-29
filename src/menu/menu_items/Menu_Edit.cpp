#include "Menu_Edit.h"

// 构造函数
Menu_Edit::Menu_Edit(Menu* menu)
    : MenuOptionBase(menu) {
}

// 初始化菜单选项
void Menu_Edit::initialize() {
    // 添加菜单项
    getMenu()->addItem("Undo", "Ctrl+Z");
    getMenu()->addItem("Redo", "Ctrl+Y");
    
    // 添加 Undo History 子菜单
    Menu* undoHistoryMenu = new Menu("Undo History");
    getMenu()->addItem("Undo History", undoHistoryMenu);
    
    getMenu()->addSeparator();
    
    getMenu()->addItem("Cut", "Ctrl+X");
    getMenu()->addItem("Copy", "Ctrl+C");
    getMenu()->addItem("Copy Merged", "Ctrl+Shift+C");
    getMenu()->addItem("Paste", "Ctrl+V");
    
    // 添加 Paste Special 子菜单
    Menu* pasteSpecialMenu = new Menu("Paste Special");
    getMenu()->addItem("Paste Special", pasteSpecialMenu);
    
    getMenu()->addSeparator();
    
    getMenu()->addItem("Delete", "Del");
    
    getMenu()->addSeparator();
    
    getMenu()->addItem("Fill", "F");
    getMenu()->addItem("Stroke", "S");
    
    getMenu()->addSeparator();
    
    getMenu()->addItem("Rotate");
    getMenu()->addItem("Flip Horizontal", "Shift+H");
    getMenu()->addItem("Flip Vertical", "Shift+V");
    
    // 添加 Transform 子菜单
    Menu* transformMenu = new Menu("Transform");
    getMenu()->addItem("Transform", transformMenu, "Ctrl+T");
    
    // 添加 Shift 子菜单
    Menu* shiftMenu = new Menu("Shift");
    getMenu()->addItem("Shift", shiftMenu);
    
    getMenu()->addSeparator();
    
    getMenu()->addItem("New Brush", "Ctrl+B");
    getMenu()->addItem("New Sprite From Selection", "Ctrl+Alt+N");
    getMenu()->addItem("Replace Color...", "Shift+R");
    getMenu()->addItem("Invert");
    
    // 添加 Adjustments 子菜单
    Menu* adjustmentsMenu = new Menu("Adjustments");
    getMenu()->addItem("Adjustments", adjustmentsMenu);
    
    // 添加 FX 子菜单
    Menu* fxMenu = new Menu("FX");
    getMenu()->addItem("FX", fxMenu);
    
    getMenu()->addItem("Insert Text");
    
    getMenu()->addSeparator();
    
    getMenu()->addItem("Keyboard Shortcuts...", "Ctrl+Alt+Shift+K");
    getMenu()->addItem("Preferences...", "Ctrl+K");
}
