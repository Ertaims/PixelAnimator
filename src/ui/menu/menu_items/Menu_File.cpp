#include "Menu_File.h"

#include "core/AppContext.h"
#include "ui/menu/Menu.h"
#include "ui/menu/MenuItem.h"

Menu_File::Menu_File(Menu* menu,
                     AppContext* context,
                     const std::function<void()>& onExitCallback,
                     const std::function<void()>& onNewProjectCallback)
    : MenuOptionBase(menu),
      context_(context),
      onExitCallback_(onExitCallback),
      onNewProjectCallback_(onNewProjectCallback) {}

void Menu_File::initialize() {
    MenuItem* newItem = getMenu()->addItem("New...", "Ctrl+N");
    newItem->setCallback([this]() {
        if (onNewProjectCallback_) {
            onNewProjectCallback_();
        }
    });

    MenuItem* openItem = getMenu()->addItem("Open...", "Ctrl+O");
    openItem->setCallback([this]() {
        if (context_) {
            // TODO: Open file dialog and deserialize project.
        }
    });

    Menu* openRecentMenu = new Menu("Open Recent");
    getMenu()->addItem("Open Recent", openRecentMenu);

    getMenu()->addSeparator();

    MenuItem* saveItem = getMenu()->addItem("Save", "Ctrl+S");
    saveItem->setCallback([this]() {
        if (context_ && context_->hasProject()) {
            // TODO: Save to existing file path or fall back to Save As.
        }
    });

    MenuItem* saveAsItem = getMenu()->addItem("Save As...", "Ctrl+Shift+S");
    saveAsItem->setCallback([this]() {
        if (context_) {
            // TODO: Save file dialog and serialize project.
        }
    });

    getMenu()->addSeparator();

    getMenu()->addItem("Close", "Ctrl+W");
    getMenu()->addItem("Close All", "Ctrl+Shift+W");

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
