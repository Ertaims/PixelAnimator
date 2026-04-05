/**
 * @file AppContext.cpp
 * @brief AppContext 成员实现（与视图、命令栈相关的逻辑放在此处，避免头文件依赖过重）
 */

#include "AppContext.h"

#include <algorithm>
#include <unordered_set>

namespace
{
// 计算“把 fromIndex 移动到 toIndex”后，任意旧索引 oldIndex 映射到的新索引。
int remapIndexAfterMove(int oldIndex, int fromIndex, int toIndex)
{
    if (fromIndex == toIndex)
        return oldIndex;

    if (fromIndex < toIndex)
    {
        // 右移：
        // - 源位置 from -> to
        // - (from, to] 区间整体左移 1
        if (oldIndex == fromIndex)
            return toIndex;
        if (oldIndex > fromIndex && oldIndex <= toIndex)
            return oldIndex - 1;
        return oldIndex;
    }

    // 左移：
    // - 源位置 from -> to
    // - [to, from) 区间整体右移 1
    if (oldIndex == fromIndex)
        return toIndex;
    if (oldIndex >= toIndex && oldIndex < fromIndex)
        return oldIndex + 1;
    return oldIndex;
}
} // namespace

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

    // 帧数量变化后，同步清理分组中的越界帧，避免时间轴高亮访问非法索引。
    sanitizeFrameGroups(frameCount);
}

void AppContext::addFrameGroup(const std::string& groupName,
                               const std::vector<int>& frameIndices,
                               int frameCount,
                               uint32_t colorRGBA)
{
    if (frameCount <= 0 || frameIndices.empty())
        return;

    // 1) 过滤越界并去重，保留输入顺序。
    std::vector<int> filtered;
    filtered.reserve(frameIndices.size());
    std::unordered_set<int> seen;
    for (int idx : frameIndices)
    {
        if (idx < 0 || idx >= frameCount)
            continue;
        if (seen.insert(idx).second)
            filtered.push_back(idx);
    }
    if (filtered.empty())
        return;

    // 2) 从旧分组中剔除“将被新分组接管”的帧，保证同一帧仅属于一个分组。
    for (FrameGroup& group : frameGroups_)
    {
        group.frameIndices.erase(
            std::remove_if(group.frameIndices.begin(),
                           group.frameIndices.end(),
                           [&filtered](int idx) {
                               return std::find(filtered.begin(), filtered.end(), idx) != filtered.end();
                           }),
            group.frameIndices.end());
    }

    // 3) 删除已经为空的旧分组。
    frameGroups_.erase(
        std::remove_if(frameGroups_.begin(),
                       frameGroups_.end(),
                       [](const FrameGroup& group) { return group.frameIndices.empty(); }),
        frameGroups_.end());

    // 4) 添加新分组。
    FrameGroup newGroup;
    newGroup.name = groupName.empty() ? ("Group " + std::to_string(frameGroups_.size() + 1)) : groupName;
    newGroup.frameIndices = std::move(filtered);
    newGroup.colorRGBA = colorRGBA;
    frameGroups_.push_back(std::move(newGroup));
}

void AppContext::sanitizeFrameGroups(int frameCount)
{
    if (frameCount <= 0)
    {
        frameGroups_.clear();
        return;
    }

    for (FrameGroup& group : frameGroups_)
    {
        group.frameIndices.erase(
            std::remove_if(group.frameIndices.begin(),
                           group.frameIndices.end(),
                           [frameCount](int idx) { return idx < 0 || idx >= frameCount; }),
            group.frameIndices.end());
    }

    frameGroups_.erase(
        std::remove_if(frameGroups_.begin(),
                       frameGroups_.end(),
                       [](const FrameGroup& group) { return group.frameIndices.empty(); }),
        frameGroups_.end());
}

void AppContext::onFrameInserted(int insertedFrameIndex, int anchorFrameIndex, int frameCount)
{
    if (insertedFrameIndex < 0)
        return;

    // 1) 先定位“参照帧所属分组”（使用插入前索引语义）。
    int targetGroupIndex = -1;
    int anchorPosInGroup = -1;
    for (size_t gi = 0; gi < frameGroups_.size(); ++gi)
    {
        std::vector<int>& indices = frameGroups_[gi].frameIndices;
        auto it = std::find(indices.begin(), indices.end(), anchorFrameIndex);
        if (it != indices.end())
        {
            targetGroupIndex = static_cast<int>(gi);
            anchorPosInGroup = static_cast<int>(std::distance(indices.begin(), it));
            break;
        }
    }

    // 2) 所有受影响索引统一后移（>= insertedFrameIndex 的成员 +1）。
    for (FrameGroup& group : frameGroups_)
    {
        for (int& idx : group.frameIndices)
        {
            if (idx >= insertedFrameIndex)
                ++idx;
        }
    }

    // 3) 若参照帧在某个分组中，则把新帧并入该分组，位置紧跟参照帧之后。
    if (targetGroupIndex >= 0 && targetGroupIndex < static_cast<int>(frameGroups_.size()))
    {
        FrameGroup& targetGroup = frameGroups_[static_cast<size_t>(targetGroupIndex)];

        const bool alreadyExists = std::find(targetGroup.frameIndices.begin(),
                                             targetGroup.frameIndices.end(),
                                             insertedFrameIndex) != targetGroup.frameIndices.end();
        if (!alreadyExists)
        {
            const int insertPos = std::clamp(anchorPosInGroup + 1,
                                             0,
                                             static_cast<int>(targetGroup.frameIndices.size()));
            targetGroup.frameIndices.insert(targetGroup.frameIndices.begin() + insertPos, insertedFrameIndex);
        }
    }

    // 4) 统一做一次边界/空组清理。
    sanitizeFrameGroups(frameCount);
}

void AppContext::onFrameRemoved(int removedFrameIndex, int frameCount)
{
    if (removedFrameIndex < 0)
        return;

    for (FrameGroup& group : frameGroups_)
    {
        // 删除被移除的帧。
        group.frameIndices.erase(
            std::remove(group.frameIndices.begin(), group.frameIndices.end(), removedFrameIndex),
            group.frameIndices.end());

        // 删除点之后的索引整体前移。
        for (int& idx : group.frameIndices)
        {
            if (idx > removedFrameIndex)
                --idx;
        }
    }

    sanitizeFrameGroups(frameCount);
}

void AppContext::onFrameMoved(int fromIndex, int toIndex, int frameCount)
{
    if (fromIndex == toIndex)
        return;

    // 1) 更新选区索引，使“选中的帧对象”在换序后仍被选中。
    for (int& idx : selectedFrameIndices_)
    {
        idx = remapIndexAfterMove(idx, fromIndex, toIndex);
    }
    currentFrameIndex_ = remapIndexAfterMove(currentFrameIndex_, fromIndex, toIndex);

    // 2) 更新每个分组内的帧索引。
    for (FrameGroup& group : frameGroups_)
    {
        for (int& idx : group.frameIndices)
            idx = remapIndexAfterMove(idx, fromIndex, toIndex);

        // 按时间轴顺序排序，保证组内顺序与当前帧顺序一致。
        std::sort(group.frameIndices.begin(), group.frameIndices.end());
    }

    // 3) 统一清理边界与空组。
    sanitizeFrameSelection(frameCount, currentFrameIndex_);
}

void AppContext::renameFrameGroup(int groupIndex, const std::string& newName)
{
    if (groupIndex < 0 || groupIndex >= static_cast<int>(frameGroups_.size()))
        return;
    if (newName.empty())
        return;
    frameGroups_[static_cast<size_t>(groupIndex)].name = newName;
}

void AppContext::removeFrameGroup(int groupIndex)
{
    if (groupIndex < 0 || groupIndex >= static_cast<int>(frameGroups_.size()))
        return;
    frameGroups_.erase(frameGroups_.begin() + groupIndex);
}

void AppContext::clearFrameGroups()
{
    frameGroups_.clear();
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
