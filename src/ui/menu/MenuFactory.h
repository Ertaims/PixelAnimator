#pragma once

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

class MenuFactory {
public:
    virtual ~MenuFactory() = default;

    virtual MenuManager* createMenuManager() = 0;

    virtual Menu_File* createFileMenu(MenuManager* manager,
                                      AppContext* context,
                                      const std::function<void()>& onExitCallback,
                                      const std::function<void()>& onNewProjectCallback) = 0;

    virtual Menu_Edit* createEditMenu(MenuManager* manager, AppContext* context) = 0;
    virtual Menu_View* createViewMenu(MenuManager* manager) = 0;
    virtual Menu_Help* createHelpMenu(MenuManager* manager) = 0;
};

class ConcreteMenuFactory : public MenuFactory {
public:
    MenuManager* createMenuManager() override {
        return new MenuManager();
    }

    Menu_File* createFileMenu(MenuManager* manager,
                              AppContext* context,
                              const std::function<void()>& onExitCallback,
                              const std::function<void()>& onNewProjectCallback) override {
        Menu* fileMenu = manager->addMenu("File");
        Menu_File* menuFile = new Menu_File(fileMenu, context, onExitCallback, onNewProjectCallback);
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
