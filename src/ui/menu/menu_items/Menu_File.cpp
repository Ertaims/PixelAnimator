#include "Menu_File.h"

#include "core/AppContext.h"
#include "ui/menu/Menu.h"
#include "ui/menu/MenuItem.h"

#include <filesystem>

Menu_File::Menu_File(Menu* menu,
                     AppContext* context,
                     const std::function<void()>& onExitCallback,
                     const std::function<void()>& onNewProjectCallback,
                     const std::function<void()>& onOpenProjectCallback,
                     const std::function<void()>& onSaveProjectCallback,
                     const std::function<void()>& onSaveAsBinaryProjectCallback,
                     const std::function<void()>& onSaveAsJsonProjectCallback,
                     const std::function<void()>& onExportCurrentFramePngCallback,
                     const std::function<void()>& onExportSpriteSheetConfigCallback,
                     const std::function<void()>& onImportSingleFramePngCallback,
                     const std::function<void()>& onImportSpriteSheetPngCallback,
                     const std::function<void(const std::string&)>& onOpenRecentProjectByPathCallback,
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
      onExportSpriteSheetConfigCallback_(onExportSpriteSheetConfigCallback),
      onImportSingleFramePngCallback_(onImportSingleFramePngCallback),
      onImportSpriteSheetPngCallback_(onImportSpriteSheetPngCallback),
      onOpenRecentProjectByPathCallback_(onOpenRecentProjectByPathCallback),
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
        if (onOpenProjectCallback_) onOpenProjectCallback_();
    });

    openRecentMenu_ = new Menu("Open Recent");
    getMenu()->addItem("Open Recent", openRecentMenu_);
    rebuildOpenRecentMenu();

    getMenu()->addSeparator();

    MenuItem* saveItem = getMenu()->addItem("Save", "Ctrl+S");
    saveItem->setCallback([this]() {
        if (onSaveProjectCallback_) onSaveProjectCallback_();
    });

    Menu* saveAsMenu = new Menu("Save As...");
    getMenu()->addItem("Save As...", saveAsMenu, "Ctrl+Shift+S");

    MenuItem* saveAsBinaryItem = saveAsMenu->addItem("Binary (*.pxanim)");
    saveAsBinaryItem->setCallback([this]() {
        if (onSaveAsBinaryProjectCallback_) onSaveAsBinaryProjectCallback_();
    });

    MenuItem* saveAsJsonItem = saveAsMenu->addItem("JSON (*.pxanim.json)");
    saveAsJsonItem->setCallback([this]() {
        if (onSaveAsJsonProjectCallback_) onSaveAsJsonProjectCallback_();
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

    // 单帧导出保持独立入口，方便快速导出当前帧。
    MenuItem* exportCurrentFramePng = exportMenu->addItem("Current Frame (PNG)...");
    exportCurrentFramePng->setCallback([this]() {
        if (onExportCurrentFramePngCallback_) onExportCurrentFramePngCallback_();
    });

    // 精灵图导出合并为“单入口”：
    // 点击后由 App 弹出配置对话框，用户在弹窗里选择行/列/行列模式及其它参数。
    MenuItem* exportSpriteSheetPng = exportMenu->addItem("Sprite Sheet (PNG)...");
    exportSpriteSheetPng->setCallback([this]() {
        if (onExportSpriteSheetConfigCallback_) onExportSpriteSheetConfigCallback_();
    });

    Menu* importMenu = new Menu("Import");
    getMenu()->addItem("Import", importMenu);

    // 与导出入口对称：提供单帧导入和精灵图导入两种入口。
    MenuItem* importSingleFramePng = importMenu->addItem("Current Frame (PNG)...");
    importSingleFramePng->setCallback([this]() {
        if (onImportSingleFramePngCallback_) onImportSingleFramePngCallback_();
    });

    MenuItem* importSpriteSheetPng = importMenu->addItem("Sprite Sheet (PNG)...");
    importSpriteSheetPng->setCallback([this]() {
        if (onImportSpriteSheetPngCallback_) onImportSpriteSheetPngCallback_();
    });

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

void Menu_File::setOnExportSpriteSheetConfigCallback(const std::function<void()>& callback) {
    onExportSpriteSheetConfigCallback_ = callback;
}

void Menu_File::setOnImportSingleFramePngCallback(const std::function<void()>& callback) {
    onImportSingleFramePngCallback_ = callback;
}

void Menu_File::setOnImportSpriteSheetPngCallback(const std::function<void()>& callback) {
    onImportSpriteSheetPngCallback_ = callback;
}

void Menu_File::setOnOpenRecentProjectByPathCallback(const std::function<void(const std::string&)>& callback)
{
    onOpenRecentProjectByPathCallback_ = callback;
    rebuildOpenRecentMenu();
}

void Menu_File::setOnCloseProjectCallback(const std::function<void()>& callback) {
    onCloseProjectCallback_ = callback;
}

void Menu_File::setOnCloseAllProjectsCallback(const std::function<void()>& callback) {
    onCloseAllProjectsCallback_ = callback;
}

void Menu_File::setRecentProjectPaths(const std::vector<std::string>& paths)
{
    recentProjectPaths_ = paths;
    rebuildOpenRecentMenu();
}

void Menu_File::rebuildOpenRecentMenu()
{
    if (!openRecentMenu_) return;

    openRecentMenu_->clearItems();
    if (recentProjectPaths_.empty())
    {
        // 空列表时给一个禁用占位项，避免子菜单看起来“点开无内容”。
        openRecentMenu_->addItem("(Empty)", "", nullptr, false);
        return;
    }

    for (size_t i = 0; i < recentProjectPaths_.size(); ++i)
    {
        const std::string& path = recentProjectPaths_[i];
        std::string filename = path;
        try
        {
            filename = std::filesystem::path(path).filename().string();
            if (filename.empty()) filename = path;
        }
        catch (...)
        {
        }

        // 菜单显示“序号 + 文件名”，并把完整路径放在快捷键列中，兼顾可读性和信息量。
        const std::string label = std::to_string(i + 1) + ". " + filename;
        MenuItem* item = openRecentMenu_->addItem(label, path);
        item->setCallback([this, path]() {
            if (onOpenRecentProjectByPathCallback_) onOpenRecentProjectByPathCallback_(path);
        });
    }
}
