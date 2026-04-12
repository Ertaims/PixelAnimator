#include "Menu_Edit.h"
#include "ui/menu/Menu.h"
#include "ui/menu/MenuItem.h"
#include "core/AppContext.h"

Menu_Edit::Menu_Edit(Menu* menu, AppContext* context)
    : MenuOptionBase(menu), context_(context) {}

void Menu_Edit::initialize() {
    MenuItem* undoItem = getMenu()->addItem("Undo", "Ctrl+Z");
    undoItem->setCallback([this]() { if (context_ && context_->canUndo()) context_->undo(); });

    MenuItem* redoItem = getMenu()->addItem("Redo", "Ctrl+Y");
    redoItem->setCallback([this]() { if (context_ && context_->canRedo()) context_->redo(); });

    // Undo History 使用独立弹窗展示，便于显示当前指针与已保存标记，并支持点击跳转。
    MenuItem* undoHistoryItem = getMenu()->addItem("Undo History");
    undoHistoryItem->setCallback([this]() {
        if (onUndoHistoryRequested_) onUndoHistoryRequested_();
    });
    
    getMenu()->addSeparator();

    MenuItem* cutItem = getMenu()->addItem("Cut", "Ctrl+X");
    cutItem->setCallback([this]() {
        if (onCutRequested_) onCutRequested_();
    });

    MenuItem* copyItem = getMenu()->addItem("Copy", "Ctrl+C");
    copyItem->setCallback([this]() {
        if (onCopyRequested_) onCopyRequested_();
    });

    getMenu()->addItem("Copy Merged", "Ctrl+Shift+C");

    MenuItem* pasteItem = getMenu()->addItem("Paste", "Ctrl+V");
    pasteItem->setCallback([this]() {
        if (onPasteRequested_) onPasteRequested_();
    });
    
    // 添加 Paste Special 子菜单
    Menu* pasteSpecialMenu = new Menu("Paste Special");
    getMenu()->addItem("Paste Special", pasteSpecialMenu);
    
    getMenu()->addSeparator();
    
    getMenu()->addItem("Delete", "Del");
    
    getMenu()->addSeparator();
    
    getMenu()->addItem("Fill", "F");
    getMenu()->addItem("Stroke", "S");
    
    getMenu()->addSeparator();
    
    getMenu()->addItem("Rotate");
    getMenu()->addItem("Flip Horizontal", "Shift+H");
    getMenu()->addItem("Flip Vertical", "Shift+V");
    
    // 添加 Transform 子菜单
    Menu* transformMenu = new Menu("Transform");
    getMenu()->addItem("Transform", transformMenu, "Ctrl+T");
    
    // 添加 Shift 子菜单
    Menu* shiftMenu = new Menu("Shift");
    getMenu()->addItem("Shift", shiftMenu);
    
    getMenu()->addSeparator();
    
    getMenu()->addItem("New Brush", "Ctrl+B");
    getMenu()->addItem("New Sprite From Selection", "Ctrl+Alt+N");
    getMenu()->addItem("Replace Color...", "Shift+R");
    getMenu()->addItem("Invert");
    
    // 添加 Adjustments 子菜单
    Menu* adjustmentsMenu = new Menu("Adjustments");
    getMenu()->addItem("Adjustments", adjustmentsMenu);
    
    // 添加 FX 子菜单
    Menu* fxMenu = new Menu("FX");
    getMenu()->addItem("FX", fxMenu);
    
    getMenu()->addItem("Insert Text");
    
    getMenu()->addSeparator();
    
    getMenu()->addItem("Keyboard Shortcuts...", "Ctrl+Alt+Shift+K");
    getMenu()->addItem("Preferences...", "Ctrl+K");
}
