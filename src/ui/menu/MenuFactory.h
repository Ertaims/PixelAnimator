#pragma once

/**
 * @file MenuFactory.h
 * @brief 菜单工厂接口与默认实现
 *
 * 作用：
 * 1. 统一创建 MenuManager 和各顶层菜单（File/Edit/View/Help）
 * 2. 把“菜单对象创建”与“App 使用菜单”解耦，便于后续替换实现
 */

#include "Menu.h"
#include "MenuBase.h"
#include "MenuItem.h"
#include "MenuManager.h"
#include "MenuSeparator.h"
#include "menu_items/Menu_Edit.h"
#include "menu_items/Menu_File.h"
#include "menu_items/Menu_Help.h"
#include "menu_items/Menu_View.h"
#include <functional>

class AppContext;

// 菜单工厂抽象接口：定义“需要创建哪些菜单”
class MenuFactory {
public:
    virtual ~MenuFactory() = default;

    // 创建菜单管理器（负责渲染和持有顶层菜单）
    virtual MenuManager* createMenuManager() = 0;

    // 创建 File 菜单
    // context：当前活动上下文（可为 nullptr，后续再注入）
    // onExitCallback：点击 Exit 时触发
    // onNewProjectCallback：点击 New 时触发
    virtual Menu_File* createFileMenu(MenuManager* manager,
                                      AppContext* context,
                                      const std::function<void()>& onExitCallback,
                                      const std::function<void()>& onNewProjectCallback,
                                      const std::function<void()>& onCloseProjectCallback,
                                      const std::function<void()>& onCloseAllProjectsCallback) = 0;

    // 创建 Edit 菜单（Undo/Redo 等通常依赖 AppContext）
    virtual Menu_Edit* createEditMenu(MenuManager* manager, AppContext* context) = 0;
    // 创建 View 菜单
    virtual Menu_View* createViewMenu(MenuManager* manager) = 0;
    // 创建 Help 菜单
    virtual Menu_Help* createHelpMenu(MenuManager* manager) = 0;
};

// 默认工厂实现：按当前项目结构创建具体菜单对象
class ConcreteMenuFactory : public MenuFactory {
public:
    MenuManager* createMenuManager() override {
        return new MenuManager();
    }

    Menu_File* createFileMenu(MenuManager* manager,
                              AppContext* context,
                              const std::function<void()>& onExitCallback,
                              const std::function<void()>& onNewProjectCallback,
                              const std::function<void()>& onCloseProjectCallback,
                              const std::function<void()>& onCloseAllProjectsCallback) override {
        Menu* fileMenu = manager->addMenu("File");
        Menu_File* menuFile = new Menu_File(
            fileMenu,
            context,
            onExitCallback,
            onNewProjectCallback,
            onCloseProjectCallback,
            onCloseAllProjectsCallback);
        menuFile->initialize();
        return menuFile;
    }

    Menu_Edit* createEditMenu(MenuManager* manager, AppContext* context) override {
        Menu* editMenu = manager->addMenu("Edit");
        Menu_Edit* menuEdit = new Menu_Edit(editMenu, context);
        menuEdit->initialize();
        return menuEdit;
    }

    Menu_View* createViewMenu(MenuManager* manager) override {
        Menu* viewMenu = manager->addMenu("View");
        Menu_View* menuView = new Menu_View(viewMenu);
        menuView->initialize();
        return menuView;
    }

    Menu_Help* createHelpMenu(MenuManager* manager) override {
        Menu* helpMenu = manager->addMenu("Help");
        Menu_Help* menuHelp = new Menu_Help(helpMenu);
        menuHelp->initialize();
        return menuHelp;
    }
};
