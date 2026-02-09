#include "Menu.h"
#include "MenuItem.h"
#include "MenuSeparator.h"
#include "imgui.h"

// 构造函数
Menu::Menu(const std::string& name, const std::string& shortcut, bool enabled)
    : MenuBase(name, shortcut, enabled) {}

// 析构函数
Menu::~Menu() {
    for (MenuBase* item : menuItems) {
        delete item;
    }
}

// 添加菜单项
MenuItem* Menu::addItem(const std::string& name, const std::string& shortcut, bool* isChecked, bool enabled) {
    MenuItem* item = new MenuItem(name, shortcut, isChecked, enabled);
    menuItems.push_back(item);
    return item;
}

MenuItem* Menu::addItem(const std::string& name, Menu* subMenu, const std::string& shortcut, bool enabled) {
    MenuItem* item = new MenuItem(name, subMenu, shortcut, enabled);
    menuItems.push_back(item);
    return item;
}

// 添加子菜单
Menu* Menu::addSubMenu(const std::string& name, const std::string& shortcut, bool enabled) {
    Menu* subMenu = new Menu(name, shortcut, enabled);
    menuItems.push_back(new MenuItem(name, subMenu, shortcut, enabled));
    return subMenu;
}

// 添加分隔线
void Menu::addSeparator() {
    menuItems.push_back(new MenuSeparator());
}

// 渲染菜单
void Menu::render() {
    if (!isEnabled()) {
        return;
    }
    
    for (MenuBase* item : menuItems) {
        item->render();
    }
}