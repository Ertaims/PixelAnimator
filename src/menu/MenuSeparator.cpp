#include "MenuSeparator.h"
#include "imgui.h"

// 构造函数
MenuSeparator::MenuSeparator()
    : MenuBase("", "", true) {}

// 渲染分隔线
void MenuSeparator::render() {
    if (!isEnabled()) {
        return;
    }
    ImGui::Separator();
}