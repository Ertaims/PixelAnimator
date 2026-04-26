#include "Project.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace
{
    // 保证宽高/帧数为正值，避免 0 或负数导致空画布
    int clampPositive(int value)
    {
        return std::max(1, value);
    }

    int clampTimelineFps(int fps)
    {
        return std::clamp(fps, 1, 60);
    }

    int clampLayerCount(int count)
    {
        return std::max(1, count);
    }

    float clampOpacity(float opacity)
    {
        return std::clamp(opacity, 0.0f, 1.0f);
    }

    uint8_t rgbaChannel(uint32_t color, int shift)
    {
        return static_cast<uint8_t>((color >> shift) & 0xFFu);
    }

    uint32_t packRgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        return static_cast<uint32_t>(r) |
               (static_cast<uint32_t>(g) << 8) |
               (static_cast<uint32_t>(b) << 16) |
               (static_cast<uint32_t>(a) << 24);
    }

    uint8_t toByte(float value)
    {
        return static_cast<uint8_t>(std::clamp(std::lround(value), 0L, 255L));
    }

    uint32_t alphaBlendPixel(uint32_t dst, uint32_t src, float layerOpacity)
    {
        const float srcAlpha = (static_cast<float>(rgbaChannel(src, 24)) / 255.0f) * layerOpacity;
        if (srcAlpha <= 0.0f) return dst;
        if (srcAlpha >= 1.0f) return (src & 0x00FFFFFFu) | (0xFFu << 24);

        const float dstAlpha = static_cast<float>(rgbaChannel(dst, 24)) / 255.0f;
        const float outAlpha = srcAlpha + dstAlpha * (1.0f - srcAlpha);
        if (outAlpha <= 0.0f) return 0x00000000;

        const float srcR = static_cast<float>(rgbaChannel(src, 0));
        const float srcG = static_cast<float>(rgbaChannel(src, 8));
        const float srcB = static_cast<float>(rgbaChannel(src, 16));
        const float dstR = static_cast<float>(rgbaChannel(dst, 0));
        const float dstG = static_cast<float>(rgbaChannel(dst, 8));
        const float dstB = static_cast<float>(rgbaChannel(dst, 16));
        const float dstWeight = dstAlpha * (1.0f - srcAlpha);

        const float outR = (srcR * srcAlpha + dstR * dstWeight) / outAlpha;
        const float outG = (srcG * srcAlpha + dstG * dstWeight) / outAlpha;
        const float outB = (srcB * srcAlpha + dstB * dstWeight) / outAlpha;

        return packRgba(toByte(outR), toByte(outG), toByte(outB), toByte(outAlpha * 255.0f));
    }

    template <typename T>
    void moveVectorItem(std::vector<T>& items, int fromIndex, int toIndex)
    {
        const int maxIndex = static_cast<int>(items.size()) - 1;
        const int from = std::clamp(fromIndex, 0, maxIndex);
        const int to = std::clamp(toIndex, 0, maxIndex);
        if (from == to) return;

        T moving = std::move(items[static_cast<size_t>(from)]);
        items.erase(items.begin() + static_cast<long long>(from));
        items.insert(items.begin() + static_cast<long long>(to), std::move(moving));
    }

    std::vector<int> normalizeLayerIndices(const std::vector<int>& layerIndices, int layerCount)
    {
        std::vector<int> normalized;
        normalized.reserve(layerIndices.size());
        for (int index : layerIndices)
        {
            if (index < 0 || index >= layerCount) continue;
            if (std::find(normalized.begin(), normalized.end(), index) != normalized.end()) continue;
            normalized.push_back(index);
        }
        std::sort(normalized.begin(), normalized.end());
        return normalized;
    }

    bool areLayerIndicesContiguous(const std::vector<int>& layerIndices)
    {
        if (layerIndices.empty()) return false;
        for (size_t i = 1; i < layerIndices.size(); ++i)
        {
            if (layerIndices[i] != layerIndices[i - 1] + 1) return false;
        }
        return true;
    }
}

Project::Frame::Frame()
    : pixels(this)
{
    ensureValidLayerStorage();
}

Project::Frame::Frame(const Frame& other)
    : pixels(this),
      m_proxyLayerIndex(other.m_proxyLayerIndex),
      m_layerPixels(other.m_layerPixels)
{
    ensureValidLayerStorage();
}

Project::Frame::Frame(Frame&& other) noexcept
    : pixels(this),
      m_proxyLayerIndex(other.m_proxyLayerIndex),
      m_layerPixels(std::move(other.m_layerPixels))
{
    ensureValidLayerStorage();
    other.ensureValidLayerStorage();
}

Project::Frame& Project::Frame::operator=(const Frame& other)
{
    if (this == &other) return *this;
    m_proxyLayerIndex = other.m_proxyLayerIndex;
    m_layerPixels = other.m_layerPixels;
    ensureValidLayerStorage();
    pixels.bind(this);
    return *this;
}

Project::Frame& Project::Frame::operator=(Frame&& other) noexcept
{
    if (this == &other) return *this;
    m_proxyLayerIndex = other.m_proxyLayerIndex;
    m_layerPixels = std::move(other.m_layerPixels);
    ensureValidLayerStorage();
    other.ensureValidLayerStorage();
    pixels.bind(this);
    return *this;
}

std::vector<uint32_t>& Project::Frame::PixelsProxy::get()
{
    if (!m_owner) throw std::logic_error("Project::Frame::PixelsProxy is not bound to a frame");
    return m_owner->getLayerPixels(m_owner->getClampedProxyLayerIndex());
}

const std::vector<uint32_t>& Project::Frame::PixelsProxy::get() const
{
    if (!m_owner) throw std::logic_error("Project::Frame::PixelsProxy is not bound to a frame");
    return static_cast<const Frame*>(m_owner)->getLayerPixels(m_owner->getClampedProxyLayerIndex());
}

void Project::Frame::setProxyLayerIndex(int index)
{
    ensureValidLayerStorage();
    m_proxyLayerIndex = std::clamp(index, 0, static_cast<int>(m_layerPixels.size()) - 1);
}

std::vector<uint32_t>& Project::Frame::getLayerPixels(int index)
{
    if (index < 0 || index >= static_cast<int>(m_layerPixels.size())) throw std::out_of_range("Project::Frame::getLayerPixels index out of range");
    return m_layerPixels[static_cast<size_t>(index)];
}

const std::vector<uint32_t>& Project::Frame::getLayerPixels(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_layerPixels.size())) throw std::out_of_range("Project::Frame::getLayerPixels index out of range");
    return m_layerPixels[static_cast<size_t>(index)];
}

void Project::Frame::insertLayerPixels(int index, size_t pixelCount, uint32_t fillColor)
{
    ensureValidLayerStorage();
    const int insertIndex = std::clamp(index, 0, static_cast<int>(m_layerPixels.size()));
    m_layerPixels.insert(
        m_layerPixels.begin() + static_cast<long long>(insertIndex),
        std::vector<uint32_t>(pixelCount, fillColor));
    if (m_proxyLayerIndex >= insertIndex) ++m_proxyLayerIndex;
    setProxyLayerIndex(m_proxyLayerIndex);
    pixels.bind(this);
}

bool Project::Frame::removeLayerPixels(int index)
{
    if (m_layerPixels.size() <= 1) return false;
    if (index < 0 || index >= static_cast<int>(m_layerPixels.size())) return false;

    m_layerPixels.erase(m_layerPixels.begin() + static_cast<long long>(index));
    if (m_proxyLayerIndex >= static_cast<int>(m_layerPixels.size()))
    {
        m_proxyLayerIndex = static_cast<int>(m_layerPixels.size()) - 1;
    }
    else if (m_proxyLayerIndex > index)
    {
        --m_proxyLayerIndex;
    }
    pixels.bind(this);
    return true;
}

bool Project::Frame::moveLayerPixels(int fromIndex, int toIndex)
{
    if (m_layerPixels.empty()) return false;

    const int maxIndex = static_cast<int>(m_layerPixels.size()) - 1;
    const int from = std::clamp(fromIndex, 0, maxIndex);
    const int to = std::clamp(toIndex, 0, maxIndex);
    if (from == to) return false;

    moveVectorItem(m_layerPixels, from, to);
    pixels.bind(this);
    return true;
}

void Project::Frame::assignLayers(int layerCount, size_t pixelCount, uint32_t fillColor)
{
    m_layerPixels.assign(static_cast<size_t>(clampLayerCount(layerCount)),
                         std::vector<uint32_t>(pixelCount, fillColor));
    setProxyLayerIndex(m_proxyLayerIndex);
    pixels.bind(this);
}

void Project::Frame::ensureValidLayerStorage()
{
    if (m_layerPixels.empty()) m_layerPixels.emplace_back();
}

int Project::Frame::getClampedProxyLayerIndex() const
{
    if (m_layerPixels.empty()) return 0;
    return std::clamp(m_proxyLayerIndex, 0, static_cast<int>(m_layerPixels.size()) - 1);
}

Project::Project() : Project(16, 16, 1, 0x00000000) {}

Project::Project(int width, int height, int frameCount, uint32_t fillColor)
{
    // 规范化参数，保证画布有效
    m_width = clampPositive(width);
    m_height = clampPositive(height);
    createDefaultLayer();
    // 创建并填充帧数据
    createFrames(std::max(1, frameCount), fillColor);
    m_timelineFps = clampTimelineFps(m_timelineFps);
}

void Project::createDefaultLayer()
{
    m_layers.clear();
    m_layers.push_back(LayerInfo{});
    m_activeLayerIndex = 0;
}

size_t Project::getPixelCount() const
{
    return static_cast<size_t>(m_width) * static_cast<size_t>(m_height);
}

std::string Project::makeDefaultLayerName() const
{
    return "Layer " + std::to_string(getLayerCount() + 1);
}

void Project::setActiveLayerIndex(int index)
{
    if (m_layers.empty())
    {
        m_activeLayerIndex = 0;
        return;
    }

    m_activeLayerIndex = std::clamp(index, 0, static_cast<int>(m_layers.size()) - 1);
    syncFrameProxyLayerIndices();
}

Project::LayerInfo& Project::getLayerInfo(int index)
{
    if (index < 0 || index >= static_cast<int>(m_layers.size())) throw std::out_of_range("Project::getLayerInfo index out of range");
    return m_layers[static_cast<size_t>(index)];
}

const Project::LayerInfo& Project::getLayerInfo(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_layers.size())) throw std::out_of_range("Project::getLayerInfo index out of range");
    return m_layers[static_cast<size_t>(index)];
}

Project::LayerInfo& Project::getActiveLayerInfo()
{
    return getLayerInfo(m_activeLayerIndex);
}

const Project::LayerInfo& Project::getActiveLayerInfo() const
{
    return getLayerInfo(m_activeLayerIndex);
}

int Project::addLayer(const std::string& name, uint32_t fillColor)
{
    if (m_layers.empty()) createDefaultLayer();

    const int insertIndex = std::clamp(m_activeLayerIndex + 1, 0, getLayerCount());
    LayerInfo layer;
    layer.name = name.empty() ? makeDefaultLayerName() : name;

    m_layers.insert(m_layers.begin() + static_cast<long long>(insertIndex), std::move(layer));

    const size_t pixelCount = getPixelCount();
    for (Frame& frame : m_frames)
    {
        frame.insertLayerPixels(insertIndex, pixelCount, fillColor);
    }

    m_activeLayerIndex = insertIndex;
    syncFrameProxyLayerIndices();
    return m_activeLayerIndex;
}

bool Project::removeLayer(int index)
{
    if (m_layers.size() <= 1) return false;
    if (index < 0 || index >= getLayerCount()) return false;

    const int removedIndex = index;
    m_layers.erase(m_layers.begin() + static_cast<long long>(removedIndex));

    for (Frame& frame : m_frames)
    {
        frame.removeLayerPixels(removedIndex);
    }

    // 删除当前层时选中同位置的新层；如果删的是最后一层，则选中新的最后一层。
    if (m_activeLayerIndex >= getLayerCount())
    {
        m_activeLayerIndex = getLayerCount() - 1;
    }
    else if (m_activeLayerIndex > removedIndex)
    {
        --m_activeLayerIndex;
    }

    syncFrameProxyLayerIndices();
    return true;
}

bool Project::moveLayer(int fromIndex, int toIndex)
{
    if (m_layers.empty()) return false;

    const int maxIndex = getLayerCount() - 1;
    const int from = std::clamp(fromIndex, 0, maxIndex);
    const int to = std::clamp(toIndex, 0, maxIndex);
    if (from == to) return false;

    moveVectorItem(m_layers, from, to);
    for (Frame& frame : m_frames)
    {
        frame.moveLayerPixels(from, to);
    }

    if (m_activeLayerIndex == from)
    {
        m_activeLayerIndex = to;
    }
    else if (from < m_activeLayerIndex && m_activeLayerIndex <= to)
    {
        --m_activeLayerIndex;
    }
    else if (to <= m_activeLayerIndex && m_activeLayerIndex < from)
    {
        ++m_activeLayerIndex;
    }

    syncFrameProxyLayerIndices();
    return true;
}

bool Project::moveLayerUp(int index)
{
    return moveLayer(index, index + 1);
}

bool Project::moveLayerDown(int index)
{
    return moveLayer(index, index - 1);
}

bool Project::canMergeLayers(const std::vector<int>& layerIndices) const
{
    const std::vector<int> normalized = normalizeLayerIndices(layerIndices, getLayerCount());
    return normalized.size() >= 2 && areLayerIndicesContiguous(normalized);
}

int Project::mergeLayers(const std::vector<int>& layerIndices, bool keepOriginalLayers)
{
    const std::vector<int> normalized = normalizeLayerIndices(layerIndices, getLayerCount());
    if (normalized.size() < 2 || !areLayerIndicesContiguous(normalized)) return -1;

    const int bottomIndex = normalized.front();
    const int topIndex = normalized.back();
    const size_t pixelCount = getPixelCount();
    std::vector<std::vector<uint32_t>> mergedFramePixels(
        static_cast<size_t>(getFrameCount()),
        std::vector<uint32_t>(pixelCount, 0x00000000));

    bool mergedVisible = false;
    for (int layerIndex : normalized)
    {
        if (getLayerInfo(layerIndex).visible)
        {
            mergedVisible = true;
            break;
        }
    }

    // 把选中图层按照原本从底到顶的顺序烘焙成一张新图层像素。
    for (int frameIndex = 0; frameIndex < getFrameCount(); ++frameIndex)
    {
        for (int layerIndex : normalized)
        {
            const LayerInfo& layer = getLayerInfo(layerIndex);
            if (!layer.visible || layer.opacity <= 0.0f) continue;

            const float layerOpacity = clampOpacity(layer.opacity);
            const std::vector<uint32_t>& source = getLayerPixels(frameIndex, layerIndex);
            std::vector<uint32_t>& merged = mergedFramePixels[static_cast<size_t>(frameIndex)];
            for (size_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex)
            {
                merged[pixelIndex] = alphaBlendPixel(merged[pixelIndex], source[pixelIndex], layerOpacity);
            }
        }
    }

    LayerInfo mergedLayer;
    mergedLayer.name = keepOriginalLayers ? "Merged Layer" : getLayerInfo(bottomIndex).name;
    mergedLayer.visible = mergedVisible;
    mergedLayer.locked = false;
    mergedLayer.opacity = 1.0f;

    const int insertIndex = topIndex + 1;
    m_layers.insert(m_layers.begin() + static_cast<long long>(insertIndex), mergedLayer);
    for (int frameIndex = 0; frameIndex < getFrameCount(); ++frameIndex)
    {
        Frame& frame = m_frames[static_cast<size_t>(frameIndex)];
        frame.insertLayerPixels(insertIndex, pixelCount, 0x00000000);
        frame.getLayerPixels(insertIndex) = mergedFramePixels[static_cast<size_t>(frameIndex)];
    }

    if (!keepOriginalLayers)
    {
        for (auto it = normalized.rbegin(); it != normalized.rend(); ++it)
        {
            m_layers.erase(m_layers.begin() + static_cast<long long>(*it));
            for (Frame& frame : m_frames)
            {
                frame.removeLayerPixels(*it);
            }
        }
        m_activeLayerIndex = bottomIndex;
    }
    else
    {
        m_activeLayerIndex = insertIndex;
    }

    syncFrameProxyLayerIndices();
    return m_activeLayerIndex;
}

void Project::renameLayer(int index, const std::string& name)
{
    if (name.empty()) return;
    getLayerInfo(index).name = name;
}

void Project::setLayerVisible(int index, bool visible)
{
    getLayerInfo(index).visible = visible;
}

void Project::setLayerLocked(int index, bool locked)
{
    getLayerInfo(index).locked = locked;
}

void Project::setLayerOpacity(int index, float opacity)
{
    getLayerInfo(index).opacity = clampOpacity(opacity);
}

bool Project::isActiveLayerLocked() const
{
    return getActiveLayerInfo().locked;
}

Project::Frame& Project::getFrame(int index)
{
    // 边界检查，防止越界访问
    if (index < 0 || index >= static_cast<int>(m_frames.size())) throw std::out_of_range("Project::getFrame index out of range");
    return m_frames[static_cast<size_t>(index)];
}

const Project::Frame& Project::getFrame(int index) const
{
    // 边界检查，防止越界访问
    if (index < 0 || index >= static_cast<int>(m_frames.size())) throw std::out_of_range("Project::getFrame index out of range");
    return m_frames[static_cast<size_t>(index)];
}

std::vector<uint32_t>& Project::getLayerPixels(int frameIndex, int layerIndex)
{
    return getFrame(frameIndex).getLayerPixels(layerIndex);
}

const std::vector<uint32_t>& Project::getLayerPixels(int frameIndex, int layerIndex) const
{
    return getFrame(frameIndex).getLayerPixels(layerIndex);
}

std::vector<uint32_t>& Project::getActiveLayerPixels(int frameIndex)
{
    return getLayerPixels(frameIndex, m_activeLayerIndex);
}

const std::vector<uint32_t>& Project::getActiveLayerPixels(int frameIndex) const
{
    return getLayerPixels(frameIndex, m_activeLayerIndex);
}

std::vector<uint32_t> Project::composeFrame(int frameIndex) const
{
    const Frame& frame = getFrame(frameIndex);
    std::vector<uint32_t> composed(getPixelCount(), 0x00000000);
    const int layerCount = std::min(getLayerCount(), frame.getLayerCount());

    // 从底层到顶层逐层混合，隐藏层和完全透明层不参与显示。
    for (int layerIndex = 0; layerIndex < layerCount; ++layerIndex)
    {
        const LayerInfo& layer = getLayerInfo(layerIndex);
        if (!layer.visible || layer.opacity <= 0.0f) continue;

        const float layerOpacity = clampOpacity(layer.opacity);
        const std::vector<uint32_t>& layerPixels = frame.getLayerPixels(layerIndex);
        const size_t pixelCount = std::min(composed.size(), layerPixels.size());
        for (size_t i = 0; i < pixelCount; ++i)
        {
            composed[i] = alphaBlendPixel(composed[i], layerPixels[i], layerOpacity);
        }
    }

    return composed;
}

void Project::resizeCanvas(int width, int height, uint32_t fillColor)
{
    const int newWidth = clampPositive(width);
    const int newHeight = clampPositive(height);

    // 尺寸不变则直接返回
    if (newWidth == m_width && newHeight == m_height) return;

    for (Frame& frame : m_frames)
    {
        for (std::vector<uint32_t>& layerPixels : frame.getAllLayerPixels())
        {
            // 新画布先用填充色初始化
            std::vector<uint32_t> newPixels(
                static_cast<size_t>(newWidth) * static_cast<size_t>(newHeight),
                fillColor);

            // 仅拷贝重叠区域（左上角对齐）
            const int copyWidth = std::min(m_width, newWidth);
            const int copyHeight = std::min(m_height, newHeight);

            for (int y = 0; y < copyHeight; ++y)
            {
                const size_t oldRow = static_cast<size_t>(y) * static_cast<size_t>(m_width);
                const size_t newRow = static_cast<size_t>(y) * static_cast<size_t>(newWidth);
                std::copy_n(layerPixels.begin() + static_cast<long long>(oldRow),
                            copyWidth,
                            newPixels.begin() + static_cast<long long>(newRow));
            }

            // 交换内存，避免额外拷贝
            layerPixels.swap(newPixels);
        }
    }

    // 更新尺寸
    m_width = newWidth;
    m_height = newHeight;
}

void Project::setFrameCount(int count, uint32_t fillColor)
{
    const int newCount = std::max(1, count);
    // 帧数不变则直接返回
    if (newCount == static_cast<int>(m_frames.size())) return;

    if (newCount < static_cast<int>(m_frames.size()))
    {
        // 缩小帧数：直接截断
        m_frames.resize(static_cast<size_t>(newCount));
        return;
    }

    // 扩展帧数：新增帧用填充色初始化
    const size_t pixelCount = getPixelCount();
    const size_t oldCount = m_frames.size();
    m_frames.resize(static_cast<size_t>(newCount));
    for (size_t i = oldCount; i < m_frames.size(); ++i)
    {
        m_frames[i].assignLayers(getLayerCount(), pixelCount, fillColor);
    }
    syncFrameProxyLayerIndices();
}

void Project::insertFrameAfter(int index, uint32_t fillColor)
{
    if (m_frames.empty())
    {
        createFrames(1, fillColor);
        return;
    }

    const int clamped = std::clamp(index, 0, static_cast<int>(m_frames.size()) - 1);
    const size_t insertPos = static_cast<size_t>(clamped + 1);

    Frame newFrame;
    const size_t pixelCount = getPixelCount();
    newFrame.assignLayers(getLayerCount(), pixelCount, fillColor);

    m_frames.insert(m_frames.begin() + static_cast<long long>(insertPos), std::move(newFrame));
    syncFrameProxyLayerIndices();
}

void Project::removeFrame(int index)
{
    if (m_frames.size() <= 1) return;

    const int clamped = std::clamp(index, 0, static_cast<int>(m_frames.size()) - 1);
    m_frames.erase(m_frames.begin() + static_cast<long long>(clamped));
}

void Project::moveFrame(int fromIndex, int toIndex)
{
    if (m_frames.empty()) return;

    const int maxIndex = static_cast<int>(m_frames.size()) - 1;
    const int from = std::clamp(fromIndex, 0, maxIndex);
    const int to = std::clamp(toIndex, 0, maxIndex);
    if (from == to) return;

    // 先把源帧取出，再插入到目标位置，避免不必要的深拷贝。
    moveVectorItem(m_frames, from, to);
}

void Project::setTimelineFps(int fps)
{
    m_timelineFps = clampTimelineFps(fps);
}

void Project::createFrames(int count, uint32_t fillColor)
{
    m_frames.clear();
    m_frames.resize(static_cast<size_t>(count));

    // 初始化每一帧的像素数据
    const size_t pixelCount = getPixelCount();
    for (Frame& frame : m_frames)
    {
        frame.assignLayers(getLayerCount(), pixelCount, fillColor);
    }
    syncFrameProxyLayerIndices();
}

void Project::syncFrameProxyLayerIndices()
{
    for (Frame& frame : m_frames)
    {
        frame.setProxyLayerIndex(m_activeLayerIndex);
    }
}
