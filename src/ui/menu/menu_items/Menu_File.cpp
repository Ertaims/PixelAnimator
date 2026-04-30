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
      m_context(context),
      m_onExitCallback(onExitCallback),
      m_onNewProjectCallback(onNewProjectCallback),
      m_onOpenProjectCallback(onOpenProjectCallback),
      m_onSaveProjectCallback(onSaveProjectCallback),
      m_onSaveAsBinaryProjectCallback(onSaveAsBinaryProjectCallback),
      m_onSaveAsJsonProjectCallback(onSaveAsJsonProjectCallback),
      m_onExportCurrentFramePngCallback(onExportCurrentFramePngCallback),
      m_onExportSpriteSheetConfigCallback(onExportSpriteSheetConfigCallback),
      m_onImportSingleFramePngCallback(onImportSingleFramePngCallback),
      m_onImportSpriteSheetPngCallback(onImportSpriteSheetPngCallback),
      m_onOpenRecentProjectByPathCallback(onOpenRecentProjectByPathCallback),
      m_onCloseProjectCallback(onCloseProjectCallback),
      m_onCloseAllProjectsCallback(onCloseAllProjectsCallback) {}

void Menu_File::initialize() {
    MenuItem* newItem = getMenu()->addItem("New...", "Ctrl+N");
    newItem->setCallback([this]() {
        if (m_onNewProjectCallback) {
            m_onNewProjectCallback();
        }
    });

    MenuItem* openItem = getMenu()->addItem("Open...", "Ctrl+O");
    openItem->setCallback([this]() {
        if (m_onOpenProjectCallback) m_onOpenProjectCallback();
    });

    m_openRecentMenu = new Menu("Open Recent");
    getMenu()->addItem("Open Recent", m_openRecentMenu);
    rebuildOpenRecentMenu();

    getMenu()->addSeparator();

    MenuItem* saveItem = getMenu()->addItem("Save", "Ctrl+S");
    saveItem->setCallback([this]() {
        if (m_onSaveProjectCallback) m_onSaveProjectCallback();
    });

    Menu* saveAsMenu = new Menu("Save As...");
    getMenu()->addItem("Save As...", saveAsMenu, "Ctrl+Shift+S");

    MenuItem* saveAsBinaryItem = saveAsMenu->addItem("Binary (*.pxanim)");
    saveAsBinaryItem->setCallback([this]() {
        if (m_onSaveAsBinaryProjectCallback) m_onSaveAsBinaryProjectCallback();
    });

    MenuItem* saveAsJsonItem = saveAsMenu->addItem("JSON (*.pxanim.json)");
    saveAsJsonItem->setCallback([this]() {
        if (m_onSaveAsJsonProjectCallback) m_onSaveAsJsonProjectCallback();
    });

    getMenu()->addSeparator();

    MenuItem* closeItem = getMenu()->addItem("Close", "Ctrl+W");
    closeItem->setCallback([this]() {
        if (m_onCloseProjectCallback) {
            m_onCloseProjectCallback();
        }
    });

    MenuItem* closeAllItem = getMenu()->addItem("Close All", "Ctrl+Shift+W");
    closeAllItem->setCallback([this]() {
        if (m_onCloseAllProjectsCallback) {
            m_onCloseAllProjectsCallback();
        }
    });

    getMenu()->addSeparator();

    Menu* exportMenu = new Menu("Export");
    getMenu()->addItem("Export", exportMenu);

    // 单帧导出保持独立入口，方便快速导出当前帧。
    MenuItem* exportCurrentFramePng = exportMenu->addItem("Current Frame (PNG)...");
    exportCurrentFramePng->setCallback([this]() {
        if (m_onExportCurrentFramePngCallback) m_onExportCurrentFramePngCallback();
    });

    // 精灵图导出合并为“单入口”：
    // 点击后由 App 弹出配置对话框，用户在弹窗里选择行/列/行列模式及其它参数。
    MenuItem* exportSpriteSheetPng = exportMenu->addItem("Sprite Sheet (PNG)...");
    exportSpriteSheetPng->setCallback([this]() {
        if (m_onExportSpriteSheetConfigCallback) m_onExportSpriteSheetConfigCallback();
    });

    Menu* importMenu = new Menu("Import");
    getMenu()->addItem("Import", importMenu);

    // 与导出入口对称：提供单帧导入和精灵图导入两种入口。
    MenuItem* importSingleFramePng = importMenu->addItem("Current Frame (PNG)...");
    importSingleFramePng->setCallback([this]() {
        if (m_onImportSingleFramePngCallback) m_onImportSingleFramePngCallback();
    });

    MenuItem* importSpriteSheetPng = importMenu->addItem("Sprite Sheet (PNG)...");
    importSpriteSheetPng->setCallback([this]() {
        if (m_onImportSpriteSheetPngCallback) m_onImportSpriteSheetPngCallback();
    });

    getMenu()->addSeparator();

    MenuItem* exitItem = getMenu()->addItem("Exit", "Ctrl+Q");
    if (m_onExitCallback) {
        exitItem->setCallback(m_onExitCallback);
    }
}

void Menu_File::setOnExitCallback(const std::function<void()>& callback) {
    m_onExitCallback = callback;
}

void Menu_File::setOnNewProjectCallback(const std::function<void()>& callback) {
    m_onNewProjectCallback = callback;
}

void Menu_File::setOnOpenProjectCallback(const std::function<void()>& callback) {
    m_onOpenProjectCallback = callback;
}

void Menu_File::setOnSaveProjectCallback(const std::function<void()>& callback) {
    m_onSaveProjectCallback = callback;
}

void Menu_File::setOnSaveAsBinaryProjectCallback(const std::function<void()>& callback) {
    m_onSaveAsBinaryProjectCallback = callback;
}

void Menu_File::setOnSaveAsJsonProjectCallback(const std::function<void()>& callback) {
    m_onSaveAsJsonProjectCallback = callback;
}

void Menu_File::setOnExportCurrentFramePngCallback(const std::function<void()>& callback) {
    m_onExportCurrentFramePngCallback = callback;
}

void Menu_File::setOnExportSpriteSheetConfigCallback(const std::function<void()>& callback) {
    m_onExportSpriteSheetConfigCallback = callback;
}

void Menu_File::setOnImportSingleFramePngCallback(const std::function<void()>& callback) {
    m_onImportSingleFramePngCallback = callback;
}

void Menu_File::setOnImportSpriteSheetPngCallback(const std::function<void()>& callback) {
    m_onImportSpriteSheetPngCallback = callback;
}

void Menu_File::setOnOpenRecentProjectByPathCallback(const std::function<void(const std::string&)>& callback)
{
    m_onOpenRecentProjectByPathCallback = callback;
    rebuildOpenRecentMenu();
}

void Menu_File::setOnCloseProjectCallback(const std::function<void()>& callback) {
    m_onCloseProjectCallback = callback;
}

void Menu_File::setOnCloseAllProjectsCallback(const std::function<void()>& callback) {
    m_onCloseAllProjectsCallback = callback;
}

void Menu_File::setRecentProjectPaths(const std::vector<std::string>& paths)
{
    m_recentProjectPaths = paths;
    rebuildOpenRecentMenu();
}

void Menu_File::rebuildOpenRecentMenu()
{
    if (!m_openRecentMenu) return;

    m_openRecentMenu->clearItems();
    if (m_recentProjectPaths.empty())
    {
        // 空列表时给一个禁用占位项，避免子菜单看起来“点开无内容”。
        m_openRecentMenu->addItem("(Empty)", "", nullptr, false);
        return;
    }

    for (size_t i = 0; i < m_recentProjectPaths.size(); ++i)
    {
        const std::string& path = m_recentProjectPaths[i];
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
        MenuItem* item = m_openRecentMenu->addItem(label, path);
        item->setCallback([this, path]() {
            if (m_onOpenRecentProjectByPathCallback) m_onOpenRecentProjectByPathCallback(path);
        });
    }
}

