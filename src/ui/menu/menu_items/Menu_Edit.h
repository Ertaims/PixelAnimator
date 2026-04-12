#pragma once

#include "MenuOptionBase.h"
#include <functional>

class AppContext;  // 前向声明

/** Edit 菜单：Undo/Redo 等，通过 AppContext 访问 CommandStack。 */
class Menu_Edit : public MenuOptionBase {
public:
    Menu_Edit(Menu* menu, AppContext* context = nullptr);

    void initialize() override;

    void setContext(AppContext* context) { context_ = context; }
    void setOnUndoHistoryRequested(const std::function<void()>& callback) { onUndoHistoryRequested_ = callback; }
    void setOnCutRequested(const std::function<void()>& callback) { onCutRequested_ = callback; }
    void setOnCopyRequested(const std::function<void()>& callback) { onCopyRequested_ = callback; }
    void setOnPasteRequested(const std::function<void()>& callback) { onPasteRequested_ = callback; }

private:
    AppContext* context_ = nullptr;
    std::function<void()> onUndoHistoryRequested_;
    std::function<void()> onCutRequested_;
    std::function<void()> onCopyRequested_;
    std::function<void()> onPasteRequested_;
};
