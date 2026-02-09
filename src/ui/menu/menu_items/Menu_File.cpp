#include "Menu_File.h"
#include "ui/menu/Menu.h"
#include "ui/menu/MenuItem.h"
#include "core/AppContext.h"

Menu_File::Menu_File(Menu* menu, AppContext* context, const std::function<void()>& onExitCallback)
    : MenuOptionBase(menu), context_(context), onExitCallback_(onExitCallback) {}

void Menu_File::initialize() {
    // New：清空当前项目状态，相当于“新建”（具体创建 Project 实例可后续在命令/对话框中做）
    MenuItem* newItem = getMenu()->addItem("New...", "Ctrl+N");
    newItem->setCallback([this]() {
        if (context_) {
            context_->setProject(nullptr);
            context_->setProjectFilePath("");
            context_->setProjectDirty(false);
            context_->setCurrentAnimationIndex(0);
            context_->setCurrentFrameIndex(0);
            // TODO: 弹出新建项目对话框（画布尺寸、名称等），创建 Project 后 context_->setProject(project)
        }
    });

    MenuItem* openItem = getMenu()->addItem("Open...", "Ctrl+O");
    openItem->setCallback([this]() {
        if (context_) {
            // TODO: 弹出文件选择对话框，加载 .pxanim 后 context_->setProject(...); setProjectFilePath(path)
        }
    });

    Menu* openRecentMenu = new Menu("Open Recent");
    getMenu()->addItem("Open Recent", openRecentMenu);

    getMenu()->addSeparator();

    MenuItem* saveItem = getMenu()->addItem("Save", "Ctrl+S");
    saveItem->setCallback([this]() {
        if (context_ && context_->hasProject()) {
            // TODO: 若有 projectFilePath_ 则保存，否则等价于 Save As
        }
    });

    MenuItem* saveAsItem = getMenu()->addItem("Save As...", "Ctrl+Shift+S");
    saveAsItem->setCallback([this]() {
        if (context_) {
            // TODO: 弹出保存对话框，序列化项目到指定路径，context_->setProjectFilePath(path); setProjectDirty(false)
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
