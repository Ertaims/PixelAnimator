/**
 * @file AppContext.cpp
 * @brief AppContext 成员实现（与视图、命令栈相关的逻辑放在此处，避免头文件依赖过重）
 */

#include "AppContext.h"
#include "Project.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace
{
    // 计算“把 fromIndex 移动到 toIndex”后，任意旧索引 oldIndex 映射到的新索引。
    int remapIndexAfterMove(int oldIndex, int fromIndex, int toIndex)
    {
        if (fromIndex == toIndex) return oldIndex;

        if (fromIndex < toIndex)
        {
            // 右移：
            // - 源位置 from -> to
            // - (from, to] 区间整体左移 1
            if (oldIndex == fromIndex) return toIndex;
            if (oldIndex > fromIndex && oldIndex <= toIndex) return oldIndex - 1;
            return oldIndex;
        }

        // 左移：
        // - 源位置 from -> to
        // - [to, from) 区间整体右移 1
        if (oldIndex == fromIndex) return toIndex;
        if (oldIndex >= toIndex && oldIndex < fromIndex) return oldIndex + 1;
        return oldIndex;
    }

    // 把一个矩形按画布边界做裁剪；若裁剪后为空则返回 false。
    bool clampRectToCanvas(AppContext::PixelRect& rect, int canvasWidth, int canvasHeight)
    {
        if (canvasWidth <= 0 || canvasHeight <= 0) return false;

        const int x0 = std::clamp(rect.x, 0, canvasWidth - 1);
        const int y0 = std::clamp(rect.y, 0, canvasHeight - 1);
        const int x1 = std::clamp(rect.x + rect.width - 1, 0, canvasWidth - 1);
        const int y1 = std::clamp(rect.y + rect.height - 1, 0, canvasHeight - 1);
        if (x1 < x0 || y1 < y0) return false;

        rect.x = x0;
        rect.y = y0;
        rect.width = x1 - x0 + 1;
        rect.height = y1 - y0 + 1;
        return true;
    }

    int roundHalfUp(double value)
    {
        return static_cast<int>(std::floor(value + 0.5));
    }

    /**
     * @brief 将拖拽外接矩形对应的椭圆填充到选区掩码。
     *
     * 说明：
     * - 椭圆几何始终基于“原始拖拽外接矩形”计算，而不是裁剪后的画布内矩形；
     *   这样拖拽到画布边缘时，边界不会因为裁剪而被重新拉伸。
     * - 边界采样逻辑与填充圆工具保持一致：同时按列/按行取边界，再按行回填；
     *   这样圆形框选的边界会更贴近最终圆形绘制工具的像素观感。
     */
    void applyEllipseSelectionToMask(std::vector<uint8_t>& ioMask,
                                     int canvasWidth,
                                     int canvasHeight,
                                     const AppContext::PixelRect& rect,
                                     AppContext::PixelSelectionOp op)
    {
        if (ioMask.size() != static_cast<size_t>(canvasWidth) * static_cast<size_t>(canvasHeight)) return;
        if (canvasWidth <= 0 || canvasHeight <= 0 || rect.width <= 0 || rect.height <= 0) return;

        AppContext::PixelRect clipped = rect;
        if (!clampRectToCanvas(clipped, canvasWidth, canvasHeight)) return;

        if (op == AppContext::PixelSelectionOp::Replace)
        {
            std::fill(ioMask.begin(), ioMask.end(), static_cast<uint8_t>(0));
        }

        const int minX = rect.x;
        const int minY = rect.y;
        const int maxX = rect.x + rect.width - 1;
        const int maxY = rect.y + rect.height - 1;

        if (rect.width == 1)
        {
            for (int y = clipped.y; y < clipped.y + clipped.height; ++y)
            {
                const size_t idx = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth)
                    + static_cast<size_t>(clipped.x);
                if (op == AppContext::PixelSelectionOp::Remove) ioMask[idx] = 0;
                else ioMask[idx] = 1;
            }
            return;
        }

        if (rect.height == 1)
        {
            const size_t rowOffset = static_cast<size_t>(clipped.y) * static_cast<size_t>(canvasWidth);
            for (int x = clipped.x; x < clipped.x + clipped.width; ++x)
            {
                const size_t idx = rowOffset + static_cast<size_t>(x);
                if (op == AppContext::PixelSelectionOp::Remove) ioMask[idx] = 0;
                else ioMask[idx] = 1;
            }
            return;
        }

        const double centerX = (static_cast<double>(minX) + static_cast<double>(maxX)) * 0.5;
        const double centerY = (static_cast<double>(minY) + static_cast<double>(maxY)) * 0.5;
        const double radiusInset = 0.25;
        const double radiusX = std::max(0.5, static_cast<double>(rect.width - 1) * 0.5 - radiusInset);
        const double radiusY = std::max(0.5, static_cast<double>(rect.height - 1) * 0.5 - radiusInset);

        std::vector<int> rowLeft(static_cast<size_t>(clipped.height), canvasWidth);
        std::vector<int> rowRight(static_cast<size_t>(clipped.height), -1);

        auto markBoundaryPoint = [&](int x, int y)
        {
            if (y < clipped.y || y >= clipped.y + clipped.height) return;

            const size_t rowIndex = static_cast<size_t>(y - clipped.y);
            rowLeft[rowIndex] = std::min(rowLeft[rowIndex], x);
            rowRight[rowIndex] = std::max(rowRight[rowIndex], x);
        };

        for (int x = minX; x <= maxX; ++x)
        {
            const double normalizedX = (static_cast<double>(x) - centerX) / radiusX;
            if (std::abs(normalizedX) > 1.0) continue;

            const double yOffset = radiusY * std::sqrt(std::max(0.0, 1.0 - normalizedX * normalizedX));
            const int topY = roundHalfUp(centerY - yOffset);
            const int bottomY = minY + maxY - topY;
            markBoundaryPoint(x, topY);
            markBoundaryPoint(x, bottomY);
        }

        for (int y = minY; y <= maxY; ++y)
        {
            const double normalizedY = (static_cast<double>(y) - centerY) / radiusY;
            if (std::abs(normalizedY) > 1.0) continue;

            const double xOffset = radiusX * std::sqrt(std::max(0.0, 1.0 - normalizedY * normalizedY));
            const int leftX = roundHalfUp(centerX - xOffset);
            const int rightX = minX + maxX - leftX;
            markBoundaryPoint(leftX, y);
            markBoundaryPoint(rightX, y);
        }

        for (int y = clipped.y; y < clipped.y + clipped.height; ++y)
        {
            const size_t rowIndex = static_cast<size_t>(y - clipped.y);
            if (rowRight[rowIndex] < rowLeft[rowIndex]) continue;

            const int fillStartX = std::max(clipped.x, rowLeft[rowIndex]);
            const int fillEndX = std::min(clipped.x + clipped.width - 1, rowRight[rowIndex]);
            if (fillEndX < fillStartX) continue;

            const size_t rowOffset = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth);
            for (int x = fillStartX; x <= fillEndX; ++x)
            {
                const size_t idx = rowOffset + static_cast<size_t>(x);
                if (op == AppContext::PixelSelectionOp::Remove) ioMask[idx] = 0;
                else ioMask[idx] = 1;
            }
        }
    }

    // 比较两个项目是否在“可编辑语义”上完全一致。
    bool areProjectsEquivalent(const Project& lhs, const Project& rhs)
    {
        if (lhs.getName() != rhs.getName()) return false;
        if (lhs.getWidth() != rhs.getWidth()) return false;
        if (lhs.getHeight() != rhs.getHeight()) return false;
        if (lhs.getTimelineFps() != rhs.getTimelineFps()) return false;
        if (lhs.getFrameCount() != rhs.getFrameCount()) return false;
        if (lhs.getLayerCount() != rhs.getLayerCount()) return false;
        if (lhs.getActiveLayerIndex() != rhs.getActiveLayerIndex()) return false;

        for (int layerIndex = 0; layerIndex < lhs.getLayerCount(); ++layerIndex)
        {
            const Project::LayerInfo& lhsLayer = lhs.getLayerInfo(layerIndex);
            const Project::LayerInfo& rhsLayer = rhs.getLayerInfo(layerIndex);
            if (lhsLayer.name != rhsLayer.name) return false;
            if (lhsLayer.visible != rhsLayer.visible) return false;
            if (lhsLayer.locked != rhsLayer.locked) return false;
            if (std::abs(lhsLayer.opacity - rhsLayer.opacity) > 0.0001f) return false;
        }

        for (int i = 0; i < lhs.getFrameCount(); ++i)
        {
            const Project::Frame& lhsFrame = lhs.getFrame(i);
            const Project::Frame& rhsFrame = rhs.getFrame(i);
            if (lhsFrame.getLayerCount() != rhsFrame.getLayerCount()) return false;
            for (int layerIndex = 0; layerIndex < lhsFrame.getLayerCount(); ++layerIndex)
            {
                if (lhsFrame.getLayerPixels(layerIndex) != rhsFrame.getLayerPixels(layerIndex)) return false;
            }
        }
        return true;
    }
} // namespace

// 后续实现 CommandStack 后在此包含，并取消下方 TODO 注释
// #include "CommandStack.h"

AppContext::AppContext() = default;

AppContext::~AppContext() = default;

void AppContext::setProjectDirty(bool dirty, const std::string& actionLabel)
{
    // 标记“已保存”时，仅更新 dirty 位与“已保存历史指针”。
    if (!dirty)
    {
        m_projectDirty = false;
        m_undoHistorySavedIndex = m_undoHistoryCurrentIndex;
        return;
    }

    // 无项目时不记录快照。
    if (!m_project)
    {
        m_projectDirty = true;
        return;
    }

    // 历史为空时先创建基线，确保 undo/redo 指针总是有锚点。
    if (m_undoHistory.empty())
    {
        m_undoHistory.push_back(captureUndoHistoryEntry("Initial"));
        m_undoHistoryCurrentIndex = 0;
        m_undoHistorySavedIndex = 0;
    }

    // 从当前状态生成候选快照。若与当前指针条目等价，则不追加历史，避免重复噪声。
    UndoHistoryEntry candidate = captureUndoHistoryEntry(actionLabel.empty() ? "Edit" : actionLabel);
    if (m_undoHistoryCurrentIndex >= 0
        && m_undoHistoryCurrentIndex < static_cast<int>(m_undoHistory.size())
        && isEquivalentToCurrentState(m_undoHistory[static_cast<size_t>(m_undoHistoryCurrentIndex)]))
    {
        m_projectDirty = (m_undoHistoryCurrentIndex != m_undoHistorySavedIndex);
        return;
    }

    // 若当前位于历史中间，再次编辑需要先裁掉 redo 分支。
    if (m_undoHistoryCurrentIndex + 1 < static_cast<int>(m_undoHistory.size()))
    {
        m_undoHistory.erase(
            m_undoHistory.begin() + static_cast<long long>(m_undoHistoryCurrentIndex + 1),
            m_undoHistory.end());
    }

    m_undoHistory.push_back(std::move(candidate));
    m_undoHistoryCurrentIndex = static_cast<int>(m_undoHistory.size()) - 1;
    trimUndoHistoryToLimit();
    m_projectDirty = (m_undoHistoryCurrentIndex != m_undoHistorySavedIndex);
}

void AppContext::setBrushSize(int size)
{
    // 限制在合理范围，避免非法值导致绘制异常
    if (size < 1) size = 1;
    if (size > 32) size = 32;
    m_brushSize = size;
}

void AppContext::setCanvasZoom(int zoom)
{
    static const int allowed[] = {1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024};
    for (int z : allowed)
    {
        if (z == zoom)
        {
            m_canvasZoom = z;
            return;
        }
    }
    // 非法值忽略，或 clamp 到最近
}

void AppContext::rebuildPixelSelectionMeta()
{
    m_pixelSelectionHasAny = false;
    m_pixelSelectionBounds = {};

    if (m_pixelSelectionCanvasWidth <= 0 || m_pixelSelectionCanvasHeight <= 0 || m_pixelSelectionMask.empty()) return;

    int minX = m_pixelSelectionCanvasWidth;
    int minY = m_pixelSelectionCanvasHeight;
    int maxX = -1;
    int maxY = -1;

    // 选区内容变化后集中扫描一次，缓存“是否有选区”和外接矩形。
    // 这样渲染层每帧取 bounds 时不再重复全画布扫描。
    for (int y = 0; y < m_pixelSelectionCanvasHeight; ++y)
    {
        const size_t rowOffset = static_cast<size_t>(y) * static_cast<size_t>(m_pixelSelectionCanvasWidth);
        for (int x = 0; x < m_pixelSelectionCanvasWidth; ++x)
        {
            const size_t index = rowOffset + static_cast<size_t>(x);
            if (m_pixelSelectionMask[index] == 0) continue;

            m_pixelSelectionHasAny = true;
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
        }
    }

    if (!m_pixelSelectionHasAny) return;

    m_pixelSelectionBounds.x = minX;
    m_pixelSelectionBounds.y = minY;
    m_pixelSelectionBounds.width = maxX - minX + 1;
    m_pixelSelectionBounds.height = maxY - minY + 1;
}

void AppContext::ensurePixelSelectionCanvasSize(int canvasWidth, int canvasHeight)
{
    if (canvasWidth <= 0 || canvasHeight <= 0)
    {
        m_pixelSelectionCanvasWidth = 0;
        m_pixelSelectionCanvasHeight = 0;
        m_pixelSelectionMask.clear();
        m_pixelSelectionHasAny = false;
        m_pixelSelectionBounds = {};
        return;
    }

    if (m_pixelSelectionCanvasWidth == canvasWidth
        && m_pixelSelectionCanvasHeight == canvasHeight
        && m_pixelSelectionMask.size() == static_cast<size_t>(canvasWidth) * static_cast<size_t>(canvasHeight))
    {
        return;
    }

    m_pixelSelectionCanvasWidth = canvasWidth;
    m_pixelSelectionCanvasHeight = canvasHeight;
    m_pixelSelectionMask.assign(static_cast<size_t>(canvasWidth) * static_cast<size_t>(canvasHeight), static_cast<uint8_t>(0));
    m_pixelSelectionHasAny = false;
    m_pixelSelectionBounds = {};
}

bool AppContext::hasPixelSelection() const
{
    return m_pixelSelectionHasAny;
}

bool AppContext::isPixelSelectionMaskCompatible(int canvasWidth, int canvasHeight) const
{
    return canvasWidth > 0
        && canvasHeight > 0
        && m_pixelSelectionCanvasWidth == canvasWidth
        && m_pixelSelectionCanvasHeight == canvasHeight
        && m_pixelSelectionMask.size() == static_cast<size_t>(canvasWidth) * static_cast<size_t>(canvasHeight);
}

const std::vector<uint8_t>& AppContext::getPixelSelectionMask() const
{
    return m_pixelSelectionMask;
}

void AppContext::clearPixelSelection()
{
    std::fill(m_pixelSelectionMask.begin(), m_pixelSelectionMask.end(), static_cast<uint8_t>(0));
    m_pixelSelectionHasAny = false;
    m_pixelSelectionBounds = {};
}

bool AppContext::isPixelSelected(int x, int y, int canvasWidth, int canvasHeight) const
{
    if (x < 0 || y < 0 || x >= canvasWidth || y >= canvasHeight) return false;
    if (canvasWidth <= 0 || canvasHeight <= 0) return false;
    if (m_pixelSelectionCanvasWidth != canvasWidth || m_pixelSelectionCanvasHeight != canvasHeight) return false;
    if (m_pixelSelectionMask.empty()) return false;

    const size_t index = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth) + static_cast<size_t>(x);
    if (index >= m_pixelSelectionMask.size()) return false;
    return m_pixelSelectionMask[index] != 0;
}

bool AppContext::canEditPixel(int x, int y, int canvasWidth, int canvasHeight) const
{
    if (!hasPixelSelection()) return true;
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
    if (m_pixelSelectionMask.empty()) return false;

    PixelRect rect;
    rect.x = std::min(x0, x1);
    rect.y = std::min(y0, y1);
    rect.width = std::abs(x1 - x0) + 1;
    rect.height = std::abs(y1 - y0) + 1;
    if (!clampRectToCanvas(rect, canvasWidth, canvasHeight)) return false;

    const std::vector<uint8_t> beforeMask = m_pixelSelectionMask;
    if (op == PixelSelectionOp::Replace) std::fill(m_pixelSelectionMask.begin(), m_pixelSelectionMask.end(), static_cast<uint8_t>(0));

    for (int py = rect.y; py < rect.y + rect.height; ++py)
    {
        const size_t rowOffset = static_cast<size_t>(py) * static_cast<size_t>(canvasWidth);
        for (int px = rect.x; px < rect.x + rect.width; ++px)
        {
            const size_t index = rowOffset + static_cast<size_t>(px);
            if (op == PixelSelectionOp::Remove) m_pixelSelectionMask[index] = 0;
            else
                m_pixelSelectionMask[index] = 1;
        }
    }

    rebuildPixelSelectionMeta();
    return m_pixelSelectionMask != beforeMask;
}

bool AppContext::applyEllipsePixelSelection(int x0,
                                            int y0,
                                            int x1,
                                            int y1,
                                            int canvasWidth,
                                            int canvasHeight,
                                            PixelSelectionOp op)
{
    ensurePixelSelectionCanvasSize(canvasWidth, canvasHeight);
    if (m_pixelSelectionMask.empty()) return false;

    PixelRect rect;
    rect.x = std::min(x0, x1);
    rect.y = std::min(y0, y1);
    rect.width = std::abs(x1 - x0) + 1;
    rect.height = std::abs(y1 - y0) + 1;

    const std::vector<uint8_t> beforeMask = m_pixelSelectionMask;
    applyEllipseSelectionToMask(m_pixelSelectionMask, canvasWidth, canvasHeight, rect, op);

    rebuildPixelSelectionMeta();
    return m_pixelSelectionMask != beforeMask;
}

bool AppContext::applyMaskPixelSelection(const std::vector<uint8_t>& inputMask,
                                         int canvasWidth,
                                         int canvasHeight,
                                         PixelSelectionOp op)
{
    ensurePixelSelectionCanvasSize(canvasWidth, canvasHeight);
    if (m_pixelSelectionMask.empty()) return false;

    const size_t expectedSize = static_cast<size_t>(canvasWidth) * static_cast<size_t>(canvasHeight);
    if (inputMask.size() != expectedSize) return false;

    const std::vector<uint8_t> beforeMask = m_pixelSelectionMask;
    if (op == PixelSelectionOp::Replace) std::fill(m_pixelSelectionMask.begin(), m_pixelSelectionMask.end(), static_cast<uint8_t>(0));

    for (size_t i = 0; i < expectedSize; ++i)
    {
        if (inputMask[i] == 0) continue;
        if (op == PixelSelectionOp::Remove) m_pixelSelectionMask[i] = 0;
        else
            m_pixelSelectionMask[i] = 1;
    }

    rebuildPixelSelectionMeta();
    return m_pixelSelectionMask != beforeMask;
}

bool AppContext::getPixelSelectionBounds(PixelRect& outRect) const
{
    if (!m_pixelSelectionHasAny) return false;
    outRect = m_pixelSelectionBounds;
    return true;
}

bool AppContext::movePixelSelection(int dx, int dy)
{
    if (m_pixelSelectionCanvasWidth <= 0 || m_pixelSelectionCanvasHeight <= 0 || m_pixelSelectionMask.empty()) return false;
    if (dx == 0 && dy == 0) return false;

    const std::vector<uint8_t> beforeMask = m_pixelSelectionMask;
    std::vector<uint8_t> movedMask(static_cast<size_t>(m_pixelSelectionCanvasWidth) * static_cast<size_t>(m_pixelSelectionCanvasHeight),
                                   static_cast<uint8_t>(0));

    for (int y = 0; y < m_pixelSelectionCanvasHeight; ++y)
    {
        const size_t rowOffset = static_cast<size_t>(y) * static_cast<size_t>(m_pixelSelectionCanvasWidth);
        for (int x = 0; x < m_pixelSelectionCanvasWidth; ++x)
        {
            const size_t index = rowOffset + static_cast<size_t>(x);
            if (beforeMask[index] == 0) continue;

            const int nx = x + dx;
            const int ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= m_pixelSelectionCanvasWidth || ny >= m_pixelSelectionCanvasHeight) continue;

            const size_t newIndex = static_cast<size_t>(ny) * static_cast<size_t>(m_pixelSelectionCanvasWidth) + static_cast<size_t>(nx);
            movedMask[newIndex] = 1;
        }
    }

    m_pixelSelectionMask.swap(movedMask);
    rebuildPixelSelectionMeta();
    return m_pixelSelectionMask != beforeMask;
}

bool AppContext::transformPixelSelectionByRect(const PixelRect& fromRect,
                                               const PixelRect& toRect,
                                               bool flipX,
                                               bool flipY)
{
    if (m_pixelSelectionCanvasWidth <= 0 || m_pixelSelectionCanvasHeight <= 0 || m_pixelSelectionMask.empty()) return false;

    PixelRect src = fromRect;
    PixelRect dst = toRect;
    if (!clampRectToCanvas(src, m_pixelSelectionCanvasWidth, m_pixelSelectionCanvasHeight)) return false;
    if (!clampRectToCanvas(dst, m_pixelSelectionCanvasWidth, m_pixelSelectionCanvasHeight)) return false;
    if (src.width <= 0 || src.height <= 0 || dst.width <= 0 || dst.height <= 0) return false;

    const std::vector<uint8_t> beforeMask = m_pixelSelectionMask;
    std::vector<uint8_t> transformedMask(
        static_cast<size_t>(m_pixelSelectionCanvasWidth) * static_cast<size_t>(m_pixelSelectionCanvasHeight),
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
            if (sx < 0 || sy < 0 || sx >= m_pixelSelectionCanvasWidth || sy >= m_pixelSelectionCanvasHeight) continue;

            const size_t srcIndex = static_cast<size_t>(sy) * static_cast<size_t>(m_pixelSelectionCanvasWidth) + static_cast<size_t>(sx);
            if (beforeMask[srcIndex] == 0) continue;

            const size_t dstIndex = static_cast<size_t>(dy) * static_cast<size_t>(m_pixelSelectionCanvasWidth) + static_cast<size_t>(dx);
            transformedMask[dstIndex] = 1;
        }
    }

    m_pixelSelectionMask.swap(transformedMask);
    rebuildPixelSelectionMeta();
    return m_pixelSelectionMask != beforeMask;
}

void AppContext::setSingleFrameSelection(int frameIndex, int frameCount)
{
    // 帧数非法时，兜底到单帧 0，避免后续访问出现空选区。
    if (frameCount <= 0)
    {
        m_currentFrameIndex = 0;
        m_selectedFrameIndices.assign(1, 0);
        return;
    }

    // 把输入索引夹到合法范围。
    const int clamped = std::clamp(frameIndex, 0, frameCount - 1);
    m_currentFrameIndex = clamped;

    // 单选语义：选区里只保留当前帧。
    m_selectedFrameIndices.assign(1, clamped);
}


void AppContext::toggleFrameSelection(int frameIndex, int frameCount)
{
    if (frameCount <= 0) return;

    const int clamped = std::clamp(frameIndex, 0, frameCount - 1);
    auto it = std::find(m_selectedFrameIndices.begin(), m_selectedFrameIndices.end(), clamped);
    if (it == m_selectedFrameIndices.end())
    {
        // 未选中：追加到选区末尾，不改变主帧顺序。
        m_selectedFrameIndices.push_back(clamped);
    }
    else
    {
        // 已选中：尝试取消。为了避免空选区，若当前仅一项则保持不变。
        if (m_selectedFrameIndices.size() > 1) m_selectedFrameIndices.erase(it);
    }

    // 每次切换后做一次统一校正，保证主帧与 m_currentFrameIndex 一致。
    sanitizeFrameSelection(frameCount, m_currentFrameIndex);
}

void AppContext::sanitizeFrameSelection(int frameCount, int fallbackIndex)
{
    if (frameCount <= 0)
    {
        m_currentFrameIndex = 0;
        m_selectedFrameIndices.assign(1, 0);
        return;
    }

    // 先移除越界项（例如删帧后遗留的旧索引）。
    m_selectedFrameIndices.erase(
        std::remove_if(
            m_selectedFrameIndices.begin(),
            m_selectedFrameIndices.end(),
            [frameCount](int idx) { return idx < 0 || idx >= frameCount; }),
        m_selectedFrameIndices.end());

    // 不允许空选区：为空时回退到 fallbackIndex。
    if (m_selectedFrameIndices.empty())
    {
        const int clampedFallback = std::clamp(fallbackIndex, 0, frameCount - 1);
        m_selectedFrameIndices.push_back(clampedFallback);
    }

    // 主帧始终是第一个选中帧；画布显示与 m_currentFrameIndex 对齐。
    m_currentFrameIndex = m_selectedFrameIndices.front();

    // 帧数量变化后，同步清理分组中的越界帧，避免时间轴高亮访问非法索引。
    sanitizeFrameGroups(frameCount);
}

void AppContext::addFrameGroup(const std::string& groupName,
                               const std::vector<int>& frameIndices,
                               int frameCount,
                               uint32_t colorRGBA)
{
    if (frameCount <= 0 || frameIndices.empty()) return;

    // 过滤越界并去重，保留输入顺序。
    std::vector<int> filtered;
    filtered.reserve(frameIndices.size());
    std::unordered_set<int> seen;
    for (int idx : frameIndices)
    {
        if (idx < 0 || idx >= frameCount) continue;
        if (seen.insert(idx).second) filtered.push_back(idx);
    }
    if (filtered.empty()) return;

    // 从旧分组中剔除“将被新分组接管”的帧，保证同一帧仅属于一个分组。
    for (FrameGroup& group : m_frameGroups)
    {
        group.frameIndices.erase(
            std::remove_if(group.frameIndices.begin(),
                           group.frameIndices.end(),
                           [&filtered](int idx) {
                               return std::find(filtered.begin(), filtered.end(), idx) != filtered.end();
                           }),
            group.frameIndices.end());
    }

    // 删除已经为空的旧分组。
    m_frameGroups.erase(
        std::remove_if(m_frameGroups.begin(),
                       m_frameGroups.end(),
                       [](const FrameGroup& group) { return group.frameIndices.empty(); }),
        m_frameGroups.end());

    // 添加新分组。
    FrameGroup newGroup;
    newGroup.name = groupName.empty() ? ("Group " + std::to_string(m_frameGroups.size() + 1)) : groupName;
    newGroup.frameIndices = std::move(filtered);
    newGroup.colorRGBA = colorRGBA;
    m_frameGroups.push_back(std::move(newGroup));
}

void AppContext::sanitizeFrameGroups(int frameCount)
{
    if (frameCount <= 0)
    {
        m_frameGroups.clear();
        return;
    }

    for (FrameGroup& group : m_frameGroups)
    {
        group.frameIndices.erase(
            std::remove_if(group.frameIndices.begin(),
                           group.frameIndices.end(),
                           [frameCount](int idx) { return idx < 0 || idx >= frameCount; }),
            group.frameIndices.end());
    }

    m_frameGroups.erase(
        std::remove_if(m_frameGroups.begin(),
                       m_frameGroups.end(),
                       [](const FrameGroup& group) { return group.frameIndices.empty(); }),
        m_frameGroups.end());
}

void AppContext::onFrameInserted(int insertedFrameIndex, int anchorFrameIndex, int frameCount)
{
    if (insertedFrameIndex < 0) return;

    // 先定位“参照帧所属分组”（使用插入前索引语义）。
    int targetGroupIndex = -1;
    int anchorPosInGroup = -1;
    for (size_t gi = 0; gi < m_frameGroups.size(); ++gi)
    {
        std::vector<int>& indices = m_frameGroups[gi].frameIndices;
        auto it = std::find(indices.begin(), indices.end(), anchorFrameIndex);
        if (it != indices.end())
        {
            targetGroupIndex = static_cast<int>(gi);
            anchorPosInGroup = static_cast<int>(std::distance(indices.begin(), it));
            break;
        }
    }

    // 所有受影响索引统一后移（>= insertedFrameIndex 的成员 +1）。
    for (FrameGroup& group : m_frameGroups)
    {
        for (int& idx : group.frameIndices)
        {
            if (idx >= insertedFrameIndex) ++idx;
        }
    }

    // 若参照帧在某个分组中，则把新帧并入该分组，位置紧跟参照帧之后。
    if (targetGroupIndex >= 0 && targetGroupIndex < static_cast<int>(m_frameGroups.size()))
    {
        FrameGroup& targetGroup = m_frameGroups[static_cast<size_t>(targetGroupIndex)];

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

    // 统一做一次边界/空组清理。
    sanitizeFrameGroups(frameCount);
}

void AppContext::onFrameRemoved(int removedFrameIndex, int frameCount)
{
    if (removedFrameIndex < 0) return;

    for (FrameGroup& group : m_frameGroups)
    {
        // 删除被移除的帧。
        group.frameIndices.erase(
            std::remove(group.frameIndices.begin(), group.frameIndices.end(), removedFrameIndex),
            group.frameIndices.end());

        // 删除点之后的索引整体前移。
        for (int& idx : group.frameIndices)
        {
            if (idx > removedFrameIndex) --idx;
        }
    }

    sanitizeFrameGroups(frameCount);
}

void AppContext::onFrameMoved(int fromIndex, int toIndex, int frameCount)
{
    if (fromIndex == toIndex) return;

    // 更新选区索引，使“选中的帧对象”在换序后仍被选中。
    for (int& idx : m_selectedFrameIndices)
    {
        idx = remapIndexAfterMove(idx, fromIndex, toIndex);
    }
    m_currentFrameIndex = remapIndexAfterMove(m_currentFrameIndex, fromIndex, toIndex);

    // 更新每个分组内的帧索引。
    for (FrameGroup& group : m_frameGroups)
    {
        for (int& idx : group.frameIndices)
            idx = remapIndexAfterMove(idx, fromIndex, toIndex);

        // 按时间轴顺序排序，保证组内顺序与当前帧顺序一致。
        std::sort(group.frameIndices.begin(), group.frameIndices.end());
    }

    // 统一清理边界与空组。
    sanitizeFrameSelection(frameCount, m_currentFrameIndex);
}

void AppContext::renameFrameGroup(int groupIndex, const std::string& newName)
{
    if (groupIndex < 0 || groupIndex >= static_cast<int>(m_frameGroups.size())) return;
    if (newName.empty()) return;
    m_frameGroups[static_cast<size_t>(groupIndex)].name = newName;
}

void AppContext::removeFrameGroup(int groupIndex)
{
    if (groupIndex < 0 || groupIndex >= static_cast<int>(m_frameGroups.size())) return;
    m_frameGroups.erase(m_frameGroups.begin() + groupIndex);
}

void AppContext::clearFrameGroups()
{
    m_frameGroups.clear();
}

bool AppContext::canUndo() const
{
    return m_undoHistoryCurrentIndex > 0 && m_undoHistoryCurrentIndex < static_cast<int>(m_undoHistory.size());
}

bool AppContext::canRedo() const
{
    return m_undoHistoryCurrentIndex >= 0
        && m_undoHistoryCurrentIndex + 1 < static_cast<int>(m_undoHistory.size());
}

void AppContext::undo()
{
    if (!canUndo()) return;
    --m_undoHistoryCurrentIndex;
    applyUndoHistoryEntry(m_undoHistory[static_cast<size_t>(m_undoHistoryCurrentIndex)]);
    m_projectDirty = (m_undoHistoryCurrentIndex != m_undoHistorySavedIndex);
}

void AppContext::redo()
{
    if (!canRedo()) return;
    ++m_undoHistoryCurrentIndex;
    applyUndoHistoryEntry(m_undoHistory[static_cast<size_t>(m_undoHistoryCurrentIndex)]);
    m_projectDirty = (m_undoHistoryCurrentIndex != m_undoHistorySavedIndex);
}

void AppContext::resetUndoRedoHistory(const std::string& initialLabel)
{
    m_undoHistory.clear();
    m_undoHistoryCurrentIndex = -1;
    m_undoHistorySavedIndex = -1;

    if (!m_project)
    {
        m_projectDirty = false;
        return;
    }

    m_undoHistory.push_back(captureUndoHistoryEntry(initialLabel.empty() ? "Initial" : initialLabel));
    m_undoHistoryCurrentIndex = 0;
    m_undoHistorySavedIndex = 0;
    m_projectDirty = false;
}

int AppContext::getUndoHistoryCount() const
{
    return static_cast<int>(m_undoHistory.size());
}

int AppContext::getUndoHistoryCurrentIndex() const
{
    return m_undoHistoryCurrentIndex;
}

int AppContext::getUndoHistorySavedIndex() const
{
    return m_undoHistorySavedIndex;
}

std::string AppContext::getUndoHistoryLabel(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_undoHistory.size())) return "";
    return m_undoHistory[static_cast<size_t>(index)].label;
}

void AppContext::jumpToUndoHistoryIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(m_undoHistory.size())) return;
    if (index == m_undoHistoryCurrentIndex) return;

    m_undoHistoryCurrentIndex = index;
    applyUndoHistoryEntry(m_undoHistory[static_cast<size_t>(m_undoHistoryCurrentIndex)]);
    m_projectDirty = (m_undoHistoryCurrentIndex != m_undoHistorySavedIndex);
}

int AppContext::getUndoHistoryMaxEntries() const
{
    return m_undoHistoryMaxEntries;
}

void AppContext::setUndoHistoryMaxEntries(int maxEntries)
{
    m_undoHistoryMaxEntries = std::max(1, maxEntries);
    trimUndoHistoryToLimit();
    m_projectDirty = (m_undoHistoryCurrentIndex != m_undoHistorySavedIndex);
}

AppContext::UndoHistoryEntry AppContext::captureUndoHistoryEntry(const std::string& label) const
{
    UndoHistoryEntry entry;
    entry.label = label.empty() ? "Edit" : label;

    if (m_project) entry.projectSnapshot = std::make_shared<Project>(*m_project);

    entry.currentAnimationIndex = m_currentAnimationIndex;
    entry.currentFrameIndex = m_currentFrameIndex;
    entry.selectedFrameIndices = m_selectedFrameIndices;
    entry.frameGroups = m_frameGroups;
    entry.selectionCanvasWidth = m_pixelSelectionCanvasWidth;
    entry.selectionCanvasHeight = m_pixelSelectionCanvasHeight;
    entry.selectionMask = m_pixelSelectionMask;
    entry.selectionHasAny = m_pixelSelectionHasAny;
    return entry;
}

void AppContext::applyUndoHistoryEntry(const UndoHistoryEntry& entry)
{
    // 历史恢复的前提是当前上下文仍持有一个项目实例。
    if (!m_project || !entry.projectSnapshot) return;

    *m_project = *entry.projectSnapshot;
    m_currentAnimationIndex = entry.currentAnimationIndex;
    m_currentFrameIndex = entry.currentFrameIndex;
    m_selectedFrameIndices = entry.selectedFrameIndices;
    m_frameGroups = entry.frameGroups;
    m_pixelSelectionCanvasWidth = entry.selectionCanvasWidth;
    m_pixelSelectionCanvasHeight = entry.selectionCanvasHeight;
    m_pixelSelectionMask = entry.selectionMask;
    m_pixelSelectionHasAny = entry.selectionHasAny;
    rebuildPixelSelectionMeta();

    // 恢复后做一次统一校正，避免索引因历史差异越界。
    sanitizeFrameSelection(m_project->getFrameCount(), m_currentFrameIndex);
}

bool AppContext::isEquivalentToCurrentState(const UndoHistoryEntry& entry) const
{
    if (!m_project || !entry.projectSnapshot) return false;
    if (!areProjectsEquivalent(*m_project, *entry.projectSnapshot)) return false;
    if (m_currentAnimationIndex != entry.currentAnimationIndex) return false;
    if (m_currentFrameIndex != entry.currentFrameIndex) return false;
    if (m_selectedFrameIndices != entry.selectedFrameIndices) return false;
    if (m_frameGroups.size() != entry.frameGroups.size()) return false;
    for (size_t i = 0; i < m_frameGroups.size(); ++i)
    {
        const FrameGroup& lhs = m_frameGroups[i];
        const FrameGroup& rhs = entry.frameGroups[i];
        if (lhs.name != rhs.name) return false;
        if (lhs.frameIndices != rhs.frameIndices) return false;
        if (lhs.colorRGBA != rhs.colorRGBA) return false;
    }
    if (m_pixelSelectionCanvasWidth != entry.selectionCanvasWidth) return false;
    if (m_pixelSelectionCanvasHeight != entry.selectionCanvasHeight) return false;
    if (m_pixelSelectionMask != entry.selectionMask) return false;
    if (m_pixelSelectionHasAny != entry.selectionHasAny) return false;
    return true;
}

void AppContext::trimUndoHistoryToLimit()
{
    if (m_undoHistoryMaxEntries <= 0) return;
    while (static_cast<int>(m_undoHistory.size()) > m_undoHistoryMaxEntries)
    {
        m_undoHistory.erase(m_undoHistory.begin());
        --m_undoHistoryCurrentIndex;
        --m_undoHistorySavedIndex;
    }
    if (m_undoHistoryCurrentIndex < 0 && !m_undoHistory.empty()) m_undoHistoryCurrentIndex = 0;
    if (m_undoHistorySavedIndex < -1) m_undoHistorySavedIndex = -1;
}

