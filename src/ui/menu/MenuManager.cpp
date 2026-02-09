#include "MenuManager.h"
#include "Menu.h"
#include "imgui.h"

// 构造函数
MenuManager::MenuManager() {}

// 析构函数
MenuManager::~MenuManager() 
{
    for (Menu* menu : menus) 
    {
        delete menu;
    }
}

// 添加菜单
Menu* MenuManager::addMenu(const std::string& name, const std::string& shortcut, bool enabled) 
{
    Menu* menu = new Menu(name, shortcut, enabled);
    menus.push_back(menu);
    return menu;
}

// 渲染所有菜单
void MenuManager::render()
{
    if (ImGui::BeginMainMenuBar()) 
    {
        for (Menu* menu : menus) 
        {
            if (menu->isEnabled()) 
            {
                if (ImGui::BeginMenu(menu->getName().c_str(), menu->isEnabled())) 
                {
                    menu->render();
                    ImGui::EndMenu();
                }
            }
        }
        ImGui::EndMainMenuBar();
    }
}