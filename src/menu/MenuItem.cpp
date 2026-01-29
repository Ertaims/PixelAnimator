#include "MenuItem.h"
#include "Menu.h"
#include "imgui.h"

// 构造函数
MenuItem::MenuItem(const std::string& name, const std::string& shortcut, bool* isChecked, bool enabled)
    : MenuBase(name, shortcut, enabled), isChecked(isChecked), subMenu(nullptr) {}

MenuItem::MenuItem(const std::string& name, Menu* subMenu, const std::string& shortcut, bool enabled)
    : MenuBase(name, shortcut, enabled), isChecked(nullptr), subMenu(subMenu) {}

// 设置回调函数
void MenuItem::setCallback(const std::function<void()>& callback) {
    this->callback = callback;
}

// 获取子菜单
Menu* MenuItem::getSubMenu() const {
    return subMenu;
}

// 渲染菜单项
void MenuItem::render() {
    if (!isEnabled()) {
        return;
    }
    
    if (subMenu) {
        if (ImGui::BeginMenu(getName().c_str(), isEnabled())) {
            subMenu->render();
            ImGui::EndMenu();
        }
    } else {
        bool isItemClicked = false;
        if (isChecked) {
            isItemClicked = ImGui::MenuItem(getName().c_str(), getShortcut().c_str(), isChecked, isEnabled());
        } else {
            isItemClicked = ImGui::MenuItem(getName().c_str(), getShortcut().c_str(), nullptr, isEnabled());
        }
        
        if (isItemClicked && callback) {
            callback();
        }
    }
}