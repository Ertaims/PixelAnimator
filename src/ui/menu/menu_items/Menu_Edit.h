#pragma once

#include "MenuOptionBase.h"

class AppContext;  // 前向声明

/** Edit 菜单：Undo/Redo 等，通过 AppContext 访问 CommandStack。 */
class Menu_Edit : public MenuOptionBase {
public:
    Menu_Edit(Menu* menu, AppContext* context = nullptr);

    void initialize() override;

    void setContext(AppContext* context) { context_ = context; }

private:
    AppContext* context_ = nullptr;
};
