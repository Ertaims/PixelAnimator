/**
 * @file AppContext.cpp
 * @brief AppContext 成员实现（与视图、命令栈相关的逻辑放在此处，避免头文件依赖过重）
 */

#include "AppContext.h"

#include <algorithm>

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

void AppContext::setSingleFrameSelection(int frameIndex, int frameCount)
{
    // 帧数非法时，兜底到单帧 0，避免后续访问出现空选区。
    if (frameCount <= 0)
    {
        currentFrameIndex_ = 0;
        selectedFrameIndices_.assign(1, 0);
        return;
    }

    // 把输入索引夹到合法范围。
    const int clamped = std::clamp(frameIndex, 0, frameCount - 1);
    currentFrameIndex_ = clamped;

    // 单选语义：选区里只保留当前帧。
    selectedFrameIndices_.assign(1, clamped);
}


void AppContext::toggleFrameSelection(int frameIndex, int frameCount)
{
    if (frameCount <= 0)
        return;

    const int clamped = std::clamp(frameIndex, 0, frameCount - 1);
    auto it = std::find(selectedFrameIndices_.begin(), selectedFrameIndices_.end(), clamped);
    if (it == selectedFrameIndices_.end())
    {
        // 未选中：追加到选区末尾，不改变主帧顺序。
        selectedFrameIndices_.push_back(clamped);
    }
    else
    {
        // 已选中：尝试取消。为了避免空选区，若当前仅一项则保持不变。
        if (selectedFrameIndices_.size() > 1)
            selectedFrameIndices_.erase(it);
    }

    // 每次切换后做一次统一校正，保证主帧与 currentFrameIndex_ 一致。
    sanitizeFrameSelection(frameCount, currentFrameIndex_);
}

void AppContext::sanitizeFrameSelection(int frameCount, int fallbackIndex)
{
    if (frameCount <= 0)
    {
        currentFrameIndex_ = 0;
        selectedFrameIndices_.assign(1, 0);
        return;
    }

    // 先移除越界项（例如删帧后遗留的旧索引）。
    selectedFrameIndices_.erase(
        std::remove_if(
            selectedFrameIndices_.begin(),
            selectedFrameIndices_.end(),
            [frameCount](int idx) { return idx < 0 || idx >= frameCount; }),
        selectedFrameIndices_.end());

    // 不允许空选区：为空时回退到 fallbackIndex。
    if (selectedFrameIndices_.empty())
    {
        const int clampedFallback = std::clamp(fallbackIndex, 0, frameCount - 1);
        selectedFrameIndices_.push_back(clampedFallback);
    }

    // 主帧始终是第一个选中帧；画布显示与 currentFrameIndex_ 对齐。
    currentFrameIndex_ = selectedFrameIndices_.front();
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
