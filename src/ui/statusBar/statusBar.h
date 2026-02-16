#pragma once

class AppContext;

/**
 * @brief 编辑器底部状态栏
 *
 * 仅负责显示信息，不修改任何状态。
 */
class statusBar
{
public:
    // 在主 UI 绘制阶段调用
    static void draw(const AppContext& ctx);
};
