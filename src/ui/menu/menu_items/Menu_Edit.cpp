#include "Menu_Edit.h"
#include "ui/menu/Menu.h"
#include "ui/menu/MenuItem.h"
#include "core/AppContext.h"

Menu_Edit::Menu_Edit(Menu* menu, AppContext* context)
    : MenuOptionBase(menu), m_context(context) {}

void Menu_Edit::initialize() {
    MenuItem* undoItem = getMenu()->addItem("Undo", "Ctrl+Z");
    undoItem->setCallback([this]() { if (m_context && m_context->canUndo()) m_context->undo(); });

    MenuItem* redoItem = getMenu()->addItem("Redo", "Ctrl+Y");
    redoItem->setCallback([this]() { if (m_context && m_context->canRedo()) m_context->redo(); });

    // Undo History 使用独立弹窗展示，便于显示当前指针与已保存标记，并支持点击跳转。
    MenuItem* undoHistoryItem = getMenu()->addItem("Undo History");
    undoHistoryItem->setCallback([this]() {
        if (m_onUndoHistoryRequested) m_onUndoHistoryRequested();
    });
    
    getMenu()->addSeparator();

    MenuItem* cutItem = getMenu()->addItem("Cut", "Ctrl+X");
    cutItem->setCallback([this]() {
        if (m_onCutRequested) m_onCutRequested();
    });

    MenuItem* copyItem = getMenu()->addItem("Copy", "Ctrl+C");
    copyItem->setCallback([this]() {
        if (m_onCopyRequested) m_onCopyRequested();
    });

    MenuItem* pasteItem = getMenu()->addItem("Paste", "Ctrl+V");
    pasteItem->setCallback([this]() {
        if (m_onPasteRequested) m_onPasteRequested();
    });
    
    getMenu()->addSeparator();
    
    MenuItem* deleteItem = getMenu()->addItem("Delete", "Del");
    deleteItem->setCallback([this]() {
        if (m_onDeleteRequested) m_onDeleteRequested();
    });
    
    getMenu()->addSeparator();
    
    // Rotate 使用子菜单暴露常用角度；实际像素处理交给 commands::RotateCommand，
    // 这里仅负责把用户选择的旋转方向传给 App 层。
    Menu* rotateMenu = new Menu("Rotate");
    getMenu()->addItem("Rotate", rotateMenu);

    MenuItem* rotate90CwItem = rotateMenu->addItem("90 CW");
    rotate90CwItem->setCallback([this]() {
        if (m_onRotateRequested) m_onRotateRequested(commands::RotationAngle::Clockwise90);
    });

    MenuItem* rotate90CcwItem = rotateMenu->addItem("90 CCW");
    rotate90CcwItem->setCallback([this]() {
        if (m_onRotateRequested) m_onRotateRequested(commands::RotationAngle::CounterClockwise90);
    });

    MenuItem* rotate180Item = rotateMenu->addItem("180");
    rotate180Item->setCallback([this]() {
        if (m_onRotateRequested) m_onRotateRequested(commands::RotationAngle::Rotate180);
    });

    MenuItem* flipHorizontalItem = getMenu()->addItem("Flip Horizontal", "Shift+H");
    flipHorizontalItem->setCallback([this]() {
        if (m_onFlipRequested) m_onFlipRequested(commands::FlipDirection::Horizontal);
    });

    MenuItem* flipVerticalItem = getMenu()->addItem("Flip Vertical", "Shift+V");
    flipVerticalItem->setCallback([this]() {
        if (m_onFlipRequested) m_onFlipRequested(commands::FlipDirection::Vertical);
    });

}

