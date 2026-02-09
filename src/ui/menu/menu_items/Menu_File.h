#pragma once

#include "MenuOptionBase.h"
#include <functional>

class AppContext;  // 前向声明，避免 menu 层依赖 core 头文件

/**
 * File 菜单：新建/打开/保存/导出/退出等，统一通过 AppContext 操作项目状态。
 * 构造时传入 AppContext*，各菜单项回调中调用 context 的相应接口。
 */
class Menu_File : public MenuOptionBase {
public:
    /**
     * @param menu 所属的 File 菜单对象
     * @param context 编辑器上下文，用于 New/Open/Save 等操作（可为 nullptr，则仅 Exit 有效）
     * @param onExitCallback 点击 Exit 时的回调（通常设置主循环退出）
     */
    Menu_File(Menu* menu, AppContext* context, const std::function<void()>& onExitCallback = nullptr);

    void initialize() override;

    void setOnExitCallback(const std::function<void()>& callback);
    void setContext(AppContext* context) { context_ = context; }

private:
    AppContext* context_ = nullptr;
    std::function<void()> onExitCallback_;
};
