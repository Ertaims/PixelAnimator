/**
 * @file AppContext.cpp
 * @brief AppContext 成员实现（与视图、命令栈相关的逻辑放在此处，避免头文件依赖过重）
 */

#include "AppContext.h"

// 后续实现 CommandStack 后在此包含，并取消下方 TODO 注释
// #include "CommandStack.h"

AppContext::AppContext() = default;

AppContext::~AppContext() = default;

void AppContext::setBrushSize(int size)
{
    // 限制在合理范围，避免非法值导致绘制异常
    if (size < 1) size = 1;
    if (size > 32) size = 32;
    brushSize_ = size;
}

void AppContext::setCanvasZoom(int zoom)
{
    static const int allowed[] = {1, 2, 4, 8, 16, 32};
    for (int z : allowed) {
        if (z == zoom) {
            canvasZoom_ = z;
            return;
        }
    }
    // 非法值忽略，或 clamp 到最近
}

bool AppContext::canUndo() const
{
    // TODO: 实现 CommandStack 后改为 return commandStack_ && commandStack_->canUndo();
    return false;
}

bool AppContext::canRedo() const
{
    // TODO: 实现 CommandStack 后改为 return commandStack_ && commandStack_->canRedo();
    return false;
}

void AppContext::undo()
{
    // TODO: 实现 CommandStack 后取消注释：
    // if (commandStack_) commandStack_->undo();
}

void AppContext::redo()
{
    // TODO: 实现 CommandStack 后取消注释：
    // if (commandStack_) commandStack_->redo();
}
