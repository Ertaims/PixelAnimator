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
                     const std::function<void()>& onSaveAsProjectCallback,
                     const std::function<void()>& onCloseProjectCallback,
                     const std::function<void()>& onCloseAllProjectsCallback)
    : MenuOptionBase(menu),
      context_(context),
      onExitCallback_(onExitCallback),
      onNewProjectCallback_(onNewProjectCallback),
      onOpenProjectCallback_(onOpenProjectCallback),
      onSaveProjectCallback_(onSaveProjectCallback),
      onSaveAsProjectCallback_(onSaveAsProjectCallback),
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

    MenuItem* saveAsItem = getMenu()->addItem("Save As...", "Ctrl+Shift+S");
    saveAsItem->setCallback([this]() {
        if (onSaveAsProjectCallback_)
            onSaveAsProjectCallback_();
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

void Menu_File::setOnSaveAsProjectCallback(const std::function<void()>& callback) {
    onSaveAsProjectCallback_ = callback;
}

void Menu_File::setOnCloseProjectCallback(const std::function<void()>& callback) {
    onCloseProjectCallback_ = callback;
}

void Menu_File::setOnCloseAllProjectsCallback(const std::function<void()>& callback) {
    onCloseAllProjectsCallback_ = callback;
}
