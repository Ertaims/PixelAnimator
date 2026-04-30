#pragma once

#include "MenuOptionBase.h"
#include "commands/FlipCommand.h"
#include "commands/RotateCommand.h"
#include <functional>

class AppContext;  // 前向声明

/** Edit 菜单：Undo/Redo 等，通过 AppContext 访问 CommandStack。 */
class Menu_Edit : public MenuOptionBase {
public:
    Menu_Edit(Menu* menu, AppContext* context = nullptr);

    void initialize() override;

    void setContext(AppContext* context) { m_context = context; }
    void setOnUndoHistoryRequested(const std::function<void()>& callback) { m_onUndoHistoryRequested = callback; }
    void setOnCutRequested(const std::function<void()>& callback) { m_onCutRequested = callback; }
    void setOnCopyRequested(const std::function<void()>& callback) { m_onCopyRequested = callback; }
    void setOnPasteRequested(const std::function<void()>& callback) { m_onPasteRequested = callback; }
    void setOnDeleteRequested(const std::function<void()>& callback) { m_onDeleteRequested = callback; }
    void setOnRotateRequested(const std::function<void(commands::RotationAngle)>& callback) { m_onRotateRequested = callback; }
    void setOnFlipRequested(const std::function<void(commands::FlipDirection)>& callback) { m_onFlipRequested = callback; }

private:
    AppContext* m_context = nullptr;
    std::function<void()> m_onUndoHistoryRequested;
    std::function<void()> m_onCutRequested;
    std::function<void()> m_onCopyRequested;
    std::function<void()> m_onPasteRequested;
    std::function<void()> m_onDeleteRequested;
    std::function<void(commands::RotationAngle)> m_onRotateRequested;
    std::function<void(commands::FlipDirection)> m_onFlipRequested;
};

