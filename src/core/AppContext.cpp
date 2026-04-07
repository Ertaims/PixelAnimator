/**
 * @file AppContext.cpp
 * @brief AppContext 成员实现（与视图、命令栈相关的逻辑放在此处，避免头文件依赖过重）
 */

#include "AppContext.h"

#include <algorithm>
#include <cmath>
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

// 把一个矩形按画布边界做裁剪；若裁剪后为空则返回 false。
bool clampRectToCanvas(AppContext::PixelRect& rect, int canvasWidth, int canvasHeight)
{
    if (canvasWidth <= 0 || canvasHeight <= 0)
        return false;

    const int x0 = std::clamp(rect.x, 0, canvasWidth - 1);
    const int y0 = std::clamp(rect.y, 0, canvasHeight - 1);
    const int x1 = std::clamp(rect.x + rect.width - 1, 0, canvasWidth - 1);
    const int y1 = std::clamp(rect.y + rect.height - 1, 0, canvasHeight - 1);
    if (x1 < x0 || y1 < y0)
        return false;

    rect.x = x0;
    rect.y = y0;
    rect.width = x1 - x0 + 1;
    rect.height = y1 - y0 + 1;
    return true;
}

bool containsSelectedPixel(const std::vector<uint8_t>& mask)
{
    return std::find(mask.begin(), mask.end(), static_cast<uint8_t>(1)) != mask.end();
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
    static const int allowed[] = {1, 2, 4, 8, 16, 32, 64, 128, 256};
    for (int z : allowed) {
        if (z == zoom) {
            canvasZoom_ = z;
            return;
        }
    }
    // 非法值忽略，或 clamp 到最近
}

void AppContext::ensurePixelSelectionCanvasSize(int canvasWidth, int canvasHeight)
{
    if (canvasWidth <= 0 || canvasHeight <= 0)
    {
        pixelSelectionCanvasWidth_ = 0;
        pixelSelectionCanvasHeight_ = 0;
        pixelSelectionMask_.clear();
        pixelSelectionHasAny_ = false;
        return;
    }

    if (pixelSelectionCanvasWidth_ == canvasWidth
        && pixelSelectionCanvasHeight_ == canvasHeight
        && pixelSelectionMask_.size() == static_cast<size_t>(canvasWidth) * static_cast<size_t>(canvasHeight))
    {
        return;
    }

    pixelSelectionCanvasWidth_ = canvasWidth;
    pixelSelectionCanvasHeight_ = canvasHeight;
    pixelSelectionMask_.assign(static_cast<size_t>(canvasWidth) * static_cast<size_t>(canvasHeight), static_cast<uint8_t>(0));
    pixelSelectionHasAny_ = false;
}

bool AppContext::hasPixelSelection() const
{
    return pixelSelectionHasAny_;
}

void AppContext::clearPixelSelection()
{
    std::fill(pixelSelectionMask_.begin(), pixelSelectionMask_.end(), static_cast<uint8_t>(0));
    pixelSelectionHasAny_ = false;
}

bool AppContext::isPixelSelected(int x, int y, int canvasWidth, int canvasHeight) const
{
    if (x < 0 || y < 0 || x >= canvasWidth || y >= canvasHeight)
        return false;
    if (canvasWidth <= 0 || canvasHeight <= 0)
        return false;
    if (pixelSelectionCanvasWidth_ != canvasWidth || pixelSelectionCanvasHeight_ != canvasHeight)
        return false;
    if (pixelSelectionMask_.empty())
        return false;

    const size_t index = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth) + static_cast<size_t>(x);
    if (index >= pixelSelectionMask_.size())
        return false;
    return pixelSelectionMask_[index] != 0;
}

bool AppContext::canEditPixel(int x, int y, int canvasWidth, int canvasHeight) const
{
    if (!hasPixelSelection())
        return true;
    return isPixelSelected(x, y, canvasWidth, canvasHeight);
}

bool AppContext::applyRectPixelSelection(int x0,
                                         int y0,
                                         int x1,
                                         int y1,
                                         int canvasWidth,
                                         int canvasHeight,
                                         PixelSelectionOp op)
{
    ensurePixelSelectionCanvasSize(canvasWidth, canvasHeight);
    if (pixelSelectionMask_.empty())
        return false;

    PixelRect rect;
    rect.x = std::min(x0, x1);
    rect.y = std::min(y0, y1);
    rect.width = std::abs(x1 - x0) + 1;
    rect.height = std::abs(y1 - y0) + 1;
    if (!clampRectToCanvas(rect, canvasWidth, canvasHeight))
        return false;

    const std::vector<uint8_t> beforeMask = pixelSelectionMask_;
    if (op == PixelSelectionOp::Replace)
        std::fill(pixelSelectionMask_.begin(), pixelSelectionMask_.end(), static_cast<uint8_t>(0));

    for (int py = rect.y; py < rect.y + rect.height; ++py)
    {
        const size_t rowOffset = static_cast<size_t>(py) * static_cast<size_t>(canvasWidth);
        for (int px = rect.x; px < rect.x + rect.width; ++px)
        {
            const size_t index = rowOffset + static_cast<size_t>(px);
            if (op == PixelSelectionOp::Remove)
                pixelSelectionMask_[index] = 0;
            else
                pixelSelectionMask_[index] = 1;
        }
    }

    pixelSelectionHasAny_ = containsSelectedPixel(pixelSelectionMask_);
    return pixelSelectionMask_ != beforeMask;
}

bool AppContext::getPixelSelectionBounds(PixelRect& outRect) const
{
    if (pixelSelectionCanvasWidth_ <= 0 || pixelSelectionCanvasHeight_ <= 0 || pixelSelectionMask_.empty())
        return false;

    int minX = pixelSelectionCanvasWidth_;
    int minY = pixelSelectionCanvasHeight_;
    int maxX = -1;
    int maxY = -1;
    bool found = false;

    for (int y = 0; y < pixelSelectionCanvasHeight_; ++y)
    {
        const size_t rowOffset = static_cast<size_t>(y) * static_cast<size_t>(pixelSelectionCanvasWidth_);
        for (int x = 0; x < pixelSelectionCanvasWidth_; ++x)
        {
            const size_t index = rowOffset + static_cast<size_t>(x);
            if (pixelSelectionMask_[index] == 0)
                continue;
            found = true;
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
        }
    }

    if (!found)
        return false;

    outRect.x = minX;
    outRect.y = minY;
    outRect.width = maxX - minX + 1;
    outRect.height = maxY - minY + 1;
    return true;
}

bool AppContext::movePixelSelection(int dx, int dy)
{
    if (pixelSelectionCanvasWidth_ <= 0 || pixelSelectionCanvasHeight_ <= 0 || pixelSelectionMask_.empty())
        return false;
    if (dx == 0 && dy == 0)
        return false;

    const std::vector<uint8_t> beforeMask = pixelSelectionMask_;
    std::vector<uint8_t> movedMask(static_cast<size_t>(pixelSelectionCanvasWidth_) * static_cast<size_t>(pixelSelectionCanvasHeight_),
                                   static_cast<uint8_t>(0));

    for (int y = 0; y < pixelSelectionCanvasHeight_; ++y)
    {
        const size_t rowOffset = static_cast<size_t>(y) * static_cast<size_t>(pixelSelectionCanvasWidth_);
        for (int x = 0; x < pixelSelectionCanvasWidth_; ++x)
        {
            const size_t index = rowOffset + static_cast<size_t>(x);
            if (beforeMask[index] == 0)
                continue;

            const int nx = x + dx;
            const int ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= pixelSelectionCanvasWidth_ || ny >= pixelSelectionCanvasHeight_)
                continue;

            const size_t newIndex = static_cast<size_t>(ny) * static_cast<size_t>(pixelSelectionCanvasWidth_) + static_cast<size_t>(nx);
            movedMask[newIndex] = 1;
        }
    }

    pixelSelectionMask_.swap(movedMask);
    pixelSelectionHasAny_ = containsSelectedPixel(pixelSelectionMask_);
    return pixelSelectionMask_ != beforeMask;
}

bool AppContext::transformPixelSelectionByRect(const PixelRect& fromRect,
                                               const PixelRect& toRect,
                                               bool flipX,
                                               bool flipY)
{
    if (pixelSelectionCanvasWidth_ <= 0 || pixelSelectionCanvasHeight_ <= 0 || pixelSelectionMask_.empty())
        return false;

    PixelRect src = fromRect;
    PixelRect dst = toRect;
    if (!clampRectToCanvas(src, pixelSelectionCanvasWidth_, pixelSelectionCanvasHeight_))
        return false;
    if (!clampRectToCanvas(dst, pixelSelectionCanvasWidth_, pixelSelectionCanvasHeight_))
        return false;
    if (src.width <= 0 || src.height <= 0 || dst.width <= 0 || dst.height <= 0)
        return false;

    const std::vector<uint8_t> beforeMask = pixelSelectionMask_;
    std::vector<uint8_t> transformedMask(
        static_cast<size_t>(pixelSelectionCanvasWidth_) * static_cast<size_t>(pixelSelectionCanvasHeight_),
        static_cast<uint8_t>(0));

    /**
     * 关键修复：
     * - 旧实现：正向映射（遍历源像素 -> 投影到目标），放大时会出现“落点稀疏”，产生空洞。
     * - 新实现：反向采样（遍历目标像素 -> 回查源像素），目标区域每个像素都会被判定一次，
     *   因此不会出现“选区边框内有不可编辑空洞”的问题。
     *
     * 数学上使用 0..1 归一化坐标映射，并用最近邻取整（lround）保证像素对齐。
     */
    for (int dy = dst.y; dy < dst.y + dst.height; ++dy)
    {
        for (int dx = dst.x; dx < dst.x + dst.width; ++dx)
        {
            const float u = (dst.width <= 1)
                ? 0.0f
                : static_cast<float>(dx - dst.x) / static_cast<float>(dst.width - 1);
            const float v = (dst.height <= 1)
                ? 0.0f
                : static_cast<float>(dy - dst.y) / static_cast<float>(dst.height - 1);

            /**
             * 翻转支持：
             * - flipX=true 时，u 沿 X 轴反向采样；
             * - flipY=true 时，v 沿 Y 轴反向采样。
             */
            const float sampleU = flipX ? (1.0f - u) : u;
            const float sampleV = flipY ? (1.0f - v) : v;

            const int sx = src.x + static_cast<int>(std::lround(sampleU * static_cast<float>(src.width - 1)));
            const int sy = src.y + static_cast<int>(std::lround(sampleV * static_cast<float>(src.height - 1)));
            if (sx < 0 || sy < 0 || sx >= pixelSelectionCanvasWidth_ || sy >= pixelSelectionCanvasHeight_)
                continue;

            const size_t srcIndex = static_cast<size_t>(sy) * static_cast<size_t>(pixelSelectionCanvasWidth_) + static_cast<size_t>(sx);
            if (beforeMask[srcIndex] == 0)
                continue;

            const size_t dstIndex = static_cast<size_t>(dy) * static_cast<size_t>(pixelSelectionCanvasWidth_) + static_cast<size_t>(dx);
            transformedMask[dstIndex] = 1;
        }
    }

    pixelSelectionMask_.swap(transformedMask);
    pixelSelectionHasAny_ = containsSelectedPixel(pixelSelectionMask_);
    return pixelSelectionMask_ != beforeMask;
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
