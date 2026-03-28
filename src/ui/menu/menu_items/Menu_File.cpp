#include "Menu_File.h"

#include "core/AppContext.h"
#include "ui/menu/Menu.h"
#include "ui/menu/MenuItem.h"

Menu_File::Menu_File(Menu* menu,
                     AppContext* context,
                     const std::function<void()>& onExitCallback,
                     const std::function<void()>& onNewProjectCallback,
                     const std::function<void()>& onOpenProjectCallback,
                     const std::function<void()>& onSaveProjectCallback,
                     const std::function<void()>& onSaveAsBinaryProjectCallback,
                     const std::function<void()>& onSaveAsJsonProjectCallback,
                     const std::function<void()>& onExportCurrentFramePngCallback,
                     const std::function<void()>& onExportSpriteSheetRowAllPngCallback,
                     const std::function<void()>& onExportSpriteSheetRowSelectedPngCallback,
                     const std::function<void()>& onExportSpriteSheetColumnAllPngCallback,
                     const std::function<void()>& onExportSpriteSheetColumnSelectedPngCallback,
                     const std::function<void()>& onCloseProjectCallback,
                     const std::function<void()>& onCloseAllProjectsCallback)
    : MenuOptionBase(menu),
      context_(context),
      onExitCallback_(onExitCallback),
      onNewProjectCallback_(onNewProjectCallback),
      onOpenProjectCallback_(onOpenProjectCallback),
      onSaveProjectCallback_(onSaveProjectCallback),
      onSaveAsBinaryProjectCallback_(onSaveAsBinaryProjectCallback),
      onSaveAsJsonProjectCallback_(onSaveAsJsonProjectCallback),
      onExportCurrentFramePngCallback_(onExportCurrentFramePngCallback),
      onExportSpriteSheetRowAllPngCallback_(onExportSpriteSheetRowAllPngCallback),
      onExportSpriteSheetRowSelectedPngCallback_(onExportSpriteSheetRowSelectedPngCallback),
      onExportSpriteSheetColumnAllPngCallback_(onExportSpriteSheetColumnAllPngCallback),
      onExportSpriteSheetColumnSelectedPngCallback_(onExportSpriteSheetColumnSelectedPngCallback),
      onCloseProjectCallback_(onCloseProjectCallback),
      onCloseAllProjectsCallback_(onCloseAllProjectsCallback) {}

void Menu_File::initialize() {
    MenuItem* newItem = getMenu()->addItem("New...", "Ctrl+N");
    newItem->setCallback([this]() {
        if (onNewProjectCallback_) {
            onNewProjectCallback_();
        }
    });

    MenuItem* openItem = getMenu()->addItem("Open...", "Ctrl+O");
    openItem->setCallback([this]() {
        if (onOpenProjectCallback_)
            onOpenProjectCallback_();
    });

    Menu* openRecentMenu = new Menu("Open Recent");
    getMenu()->addItem("Open Recent", openRecentMenu);

    getMenu()->addSeparator();

    MenuItem* saveItem = getMenu()->addItem("Save", "Ctrl+S");
    saveItem->setCallback([this]() {
        if (onSaveProjectCallback_)
            onSaveProjectCallback_();
    });

    Menu* saveAsMenu = new Menu("Save As...");
    getMenu()->addItem("Save As...", saveAsMenu, "Ctrl+Shift+S");

    MenuItem* saveAsBinaryItem = saveAsMenu->addItem("Binary (*.pxanim)");
    saveAsBinaryItem->setCallback([this]() {
        if (onSaveAsBinaryProjectCallback_)
            onSaveAsBinaryProjectCallback_();
    });

    MenuItem* saveAsJsonItem = saveAsMenu->addItem("JSON (*.pxanim.json)");
    saveAsJsonItem->setCallback([this]() {
        if (onSaveAsJsonProjectCallback_)
            onSaveAsJsonProjectCallback_();
    });

    getMenu()->addSeparator();

    MenuItem* closeItem = getMenu()->addItem("Close", "Ctrl+W");
    closeItem->setCallback([this]() {
        if (onCloseProjectCallback_) {
            onCloseProjectCallback_();
        }
    });

    MenuItem* closeAllItem = getMenu()->addItem("Close All", "Ctrl+Shift+W");
    closeAllItem->setCallback([this]() {
        if (onCloseAllProjectsCallback_) {
            onCloseAllProjectsCallback_();
        }
    });

    getMenu()->addSeparator();

    Menu* exportMenu = new Menu("Export");
    getMenu()->addItem("Export", exportMenu);
    MenuItem* exportCurrentFramePng = exportMenu->addItem("Current Frame (PNG)...");
    exportCurrentFramePng->setCallback([this]() {
        if (onExportCurrentFramePngCallback_)
            onExportCurrentFramePngCallback_();
    });

    MenuItem* exportSpriteSheetRowAllPng = exportMenu->addItem("Sprite Sheet Row - All Frames (PNG)...");
    exportSpriteSheetRowAllPng->setCallback([this]() {
        if (onExportSpriteSheetRowAllPngCallback_)
            onExportSpriteSheetRowAllPngCallback_();
    });

    MenuItem* exportSpriteSheetRowSelectedPng = exportMenu->addItem("Sprite Sheet Row - Selected Frames (PNG)...");
    exportSpriteSheetRowSelectedPng->setCallback([this]() {
        if (onExportSpriteSheetRowSelectedPngCallback_)
            onExportSpriteSheetRowSelectedPngCallback_();
    });

    MenuItem* exportSpriteSheetColumnAllPng = exportMenu->addItem("Sprite Sheet Column - All Frames (PNG)...");
    exportSpriteSheetColumnAllPng->setCallback([this]() {
        if (onExportSpriteSheetColumnAllPngCallback_)
            onExportSpriteSheetColumnAllPngCallback_();
    });

    MenuItem* exportSpriteSheetColumnSelectedPng = exportMenu->addItem("Sprite Sheet Column - Selected Frames (PNG)...");
    exportSpriteSheetColumnSelectedPng->setCallback([this]() {
        if (onExportSpriteSheetColumnSelectedPngCallback_)
            onExportSpriteSheetColumnSelectedPngCallback_();
    });

    Menu* importMenu = new Menu("Import");
    getMenu()->addItem("Import", importMenu);

    getMenu()->addSeparator();

    MenuItem* exitItem = getMenu()->addItem("Exit", "Ctrl+Q");
    if (onExitCallback_) {
        exitItem->setCallback(onExitCallback_);
    }
}

void Menu_File::setOnExitCallback(const std::function<void()>& callback) {
    onExitCallback_ = callback;
}

void Menu_File::setOnNewProjectCallback(const std::function<void()>& callback) {
    onNewProjectCallback_ = callback;
}

void Menu_File::setOnOpenProjectCallback(const std::function<void()>& callback) {
    onOpenProjectCallback_ = callback;
}

void Menu_File::setOnSaveProjectCallback(const std::function<void()>& callback) {
    onSaveProjectCallback_ = callback;
}

void Menu_File::setOnSaveAsBinaryProjectCallback(const std::function<void()>& callback) {
    onSaveAsBinaryProjectCallback_ = callback;
}

void Menu_File::setOnSaveAsJsonProjectCallback(const std::function<void()>& callback) {
    onSaveAsJsonProjectCallback_ = callback;
}

void Menu_File::setOnExportCurrentFramePngCallback(const std::function<void()>& callback) {
    onExportCurrentFramePngCallback_ = callback;
}

void Menu_File::setOnExportSpriteSheetRowAllPngCallback(const std::function<void()>& callback) {
    onExportSpriteSheetRowAllPngCallback_ = callback;
}

void Menu_File::setOnExportSpriteSheetRowSelectedPngCallback(const std::function<void()>& callback) {
    onExportSpriteSheetRowSelectedPngCallback_ = callback;
}

void Menu_File::setOnExportSpriteSheetColumnAllPngCallback(const std::function<void()>& callback) {
    onExportSpriteSheetColumnAllPngCallback_ = callback;
}

void Menu_File::setOnExportSpriteSheetColumnSelectedPngCallback(const std::function<void()>& callback) {
    onExportSpriteSheetColumnSelectedPngCallback_ = callback;
}

void Menu_File::setOnCloseProjectCallback(const std::function<void()>& callback) {
    onCloseProjectCallback_ = callback;
}

void Menu_File::setOnCloseAllProjectsCallback(const std::function<void()>& callback) {
    onCloseAllProjectsCallback_ = callback;
}
