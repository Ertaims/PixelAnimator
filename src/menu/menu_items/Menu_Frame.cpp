#include "Menu_Frame.h"

// 构造函数
Menu_Frame::Menu_Frame(Menu* menu)
    : MenuOptionBase(menu) {
}

// 初始化菜单选项
void Menu_Frame::initialize() {
    // 添加菜单项
    getMenu()->addItem("Frame Properties...", "P");
    getMenu()->addItem("Cel Properties...", "Alt+C");
    
    getMenu()->addSeparator();
    
    getMenu()->addItem("New Frame", "Alt+N");
    getMenu()->addItem("New Empty Frame", "Alt+B");
    getMenu()->addItem("Duplicate Cel(s)", "Alt+D");
    getMenu()->addItem("Duplicate Linked Cel(s)", "Alt+M");
    getMenu()->addItem("Delete Frame", "Alt+F");
    
    getMenu()->addSeparator();
    
    // 添加 Playback 子菜单
    Menu* playbackMenu = new Menu("Playback");
    getMenu()->addItem("Playback", playbackMenu);
    
    // 添加 Tags 子菜单
    Menu* tagsMenu = new Menu("Tags");
    getMenu()->addItem("Tags", tagsMenu);
    
    // 添加 Jump to 子菜单
    Menu* jumpToMenu = new Menu("Jump to");
    getMenu()->addItem("Jump to", jumpToMenu);
    
    getMenu()->addSeparator();
    
    getMenu()->addItem("Constant Frame Rate");
    getMenu()->addItem("Reverse Frames", "Alt+I");
}
