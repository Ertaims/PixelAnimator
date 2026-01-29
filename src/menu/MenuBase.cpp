#include "MenuBase.h"

// 构造函数
MenuBase::MenuBase(const std::string& name, const std::string& shortcut, bool enabled)
    : name(name), shortcut(shortcut), enabled(enabled) {}

// 获取菜单名称
const std::string& MenuBase::getName() const {
    return name;
}

// 获取快捷键
const std::string& MenuBase::getShortcut() const {
    return shortcut;
}

// 设置是否启用
void MenuBase::setEnabled(bool enabled) {
    this->enabled = enabled;
}

// 获取是否启用
bool MenuBase::isEnabled() const {
    return enabled;
}