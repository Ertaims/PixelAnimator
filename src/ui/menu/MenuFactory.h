#pragma once

#include "MenuBase.h"
#include "MenuItem.h"
#include "Menu.h"
#include "MenuSeparator.h"
#include "MenuManager.h"
#include "menu_items/Menu_File.h"
#include "menu_items/Menu_Edit.h"
#include "menu_items/Menu_View.h"
#include "menu_items/Menu_Help.h"
#include <functional>

class AppContext;  // 前向声明，菜单通过 context 执行 New/Open/Save/Undo/Redo 等

// 菜单工厂抽象基类
class MenuFactory {
public:
    virtual ~MenuFactory() = default;

    virtual MenuManager* createMenuManager() = 0;

    /** 创建 File 菜单；context 用于 New/Open/Save 等，onExitCallback 为 Exit 时调用 */
    virtual Menu_File* createFileMenu(MenuManager* manager, AppContext* context, const std::function<void()>& onExitCallback) = 0;

    /** 创建 Edit 菜单；context 用于 Undo/Redo */
    virtual Menu_Edit* createEditMenu(MenuManager* manager, AppContext* context) = 0;
    
    // 创建View菜单
    virtual Menu_View* createViewMenu(MenuManager* manager) = 0;
    
    // 创建Help菜单
    virtual Menu_Help* createHelpMenu(MenuManager* manager) = 0;
};

// 具体菜单工厂类
class ConcreteMenuFactory : public MenuFactory {
public:
    MenuManager* createMenuManager() override {
        return new MenuManager();
    }
    
    Menu_File* createFileMenu(MenuManager* manager, AppContext* context, const std::function<void()>& onExitCallback) override {
        Menu* fileMenu = manager->addMenu("File");
        Menu_File* menuFile = new Menu_File(fileMenu, context, onExitCallback);
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