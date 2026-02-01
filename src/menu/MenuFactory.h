#pragma once

// 包含所有菜单相关的头文件
#include "MenuBase.h"
#include "MenuItem.h"
#include "Menu.h"
#include "MenuSeparator.h"
#include "MenuManager.h"
#include "menu_items/Menu_File.h"
#include "menu_items/Menu_Edit.h"
#include "menu_items/Menu_Sprite.h"
#include "menu_items/Menu_Layer.h"
#include "menu_items/Menu_Frame.h"
#include "menu_items/Menu_Select.h"
#include "menu_items/Menu_View.h"
#include "menu_items/Menu_Help.h"

// 菜单工厂抽象基类
class MenuFactory {
public:
    virtual ~MenuFactory() = default;
    
    // 创建菜单管理器
    virtual MenuManager* createMenuManager() = 0;
    
    // 创建文件菜单
    virtual Menu_File* createFileMenu(MenuManager* manager, const std::function<void()>& onExitCallback) = 0;
    
    // 创建编辑菜单
    virtual Menu_Edit* createEditMenu(MenuManager* manager) = 0;
    
    // 创建Sprite菜单
    virtual Menu_Sprite* createSpriteMenu(MenuManager* manager) = 0;
    
    // 创建Layer菜单
    virtual Menu_Layer* createLayerMenu(MenuManager* manager) = 0;
    
    // 创建Frame菜单
    virtual Menu_Frame* createFrameMenu(MenuManager* manager) = 0;
    
    // 创建Select菜单
    virtual Menu_Select* createSelectMenu(MenuManager* manager) = 0;
    
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
    
    Menu_File* createFileMenu(MenuManager* manager, const std::function<void()>& onExitCallback) override {
        Menu* fileMenu = manager->addMenu("File");
        Menu_File* menuFile = new Menu_File(fileMenu, onExitCallback);
        menuFile->initialize();
        return menuFile;
    }
    
    Menu_Edit* createEditMenu(MenuManager* manager) override {
        Menu* editMenu = manager->addMenu("Edit");
        Menu_Edit* menuEdit = new Menu_Edit(editMenu);
        menuEdit->initialize();
        return menuEdit;
    }
    
    Menu_Sprite* createSpriteMenu(MenuManager* manager) override {
        Menu* spriteMenu = manager->addMenu("Sprite");
        Menu_Sprite* menuSprite = new Menu_Sprite(spriteMenu);
        menuSprite->initialize();
        return menuSprite;
    }
    
    Menu_Layer* createLayerMenu(MenuManager* manager) override {
        Menu* layerMenu = manager->addMenu("Layer");
        Menu_Layer* menuLayer = new Menu_Layer(layerMenu);
        menuLayer->initialize();
        return menuLayer;
    }
    
    Menu_Frame* createFrameMenu(MenuManager* manager) override {
        Menu* frameMenu = manager->addMenu("Frame");
        Menu_Frame* menuFrame = new Menu_Frame(frameMenu);
        menuFrame->initialize();
        return menuFrame;
    }
    
    Menu_Select* createSelectMenu(MenuManager* manager) override {
        Menu* selectMenu = manager->addMenu("Select");
        Menu_Select* menuSelect = new Menu_Select(selectMenu);
        menuSelect->initialize();
        return menuSelect;
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