#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

/**
 * @brief 像素动画项目的数据模型
 *
 * 主要职责：
 * - 记录画布尺寸（宽高）
 * - 维护统一的图层结构
 * - 维护帧列表（每帧保存各图层的 RGBA8888 像素数组）
 * - 提供简单的画布调整与帧数量管理
 *
 * 注意：
 * - 本类只管理内存中的数据，不负责文件 IO
 * - 像素格式为 RGBA8888（uint32_t），与 ImGui/渲染层便于对接
 */
class Project
{
public:
    /**
     * @brief 图层元信息。
     *
     * 第一阶段先把图层骨架搭起来：
     * - name：图层名称
     * - visible：是否可见
     * - locked：是否锁定
     * - opacity：图层整体透明度（0~1）
     */
    struct LayerInfo
    {
        std::string name = "Background";
        bool visible = true;
        bool locked = false;
        float opacity = 1.0f;
    };

    struct Frame
    {
        /**
         * @brief 当前可编辑图层的像素兼容代理。
         *
         * 说明：
         * - 旧代码大量直接访问 frame.pixels；
         * - 多数工具仍通过 frame.pixels 读写像素；
         * - 因此这里提供一个代理，把旧的 frame.pixels 透明映射到“当前活动图层”。
         *
         * 这样做的好处是：
         * - 当前单图层逻辑继续可用；
         * - 后续逐步把工具切到“当前活动图层”时，不会一次改太多文件。
         */
        class PixelsProxy
        {
        public:
            PixelsProxy() = default;
            explicit PixelsProxy(Frame* owner) : m_owner(owner) {}

            void bind(Frame* owner)
            {
                m_owner = owner;
            }

            std::vector<uint32_t>& get();
            const std::vector<uint32_t>& get() const;

            operator std::vector<uint32_t>&()
            {
                return get();
            }

            operator const std::vector<uint32_t>&() const
            {
                return get();
            }

            uint32_t& operator[](size_t index)
            {
                return get()[index];
            }

            const uint32_t& operator[](size_t index) const
            {
                return get()[index];
            }

            size_t size() const
            {
                return get().size();
            }

            bool empty() const
            {
                return get().empty();
            }

            uint32_t* data()
            {
                return get().data();
            }

            const uint32_t* data() const
            {
                return get().data();
            }

            std::vector<uint32_t>::iterator begin()
            {
                return get().begin();
            }

            std::vector<uint32_t>::iterator end()
            {
                return get().end();
            }

            std::vector<uint32_t>::const_iterator begin() const
            {
                return get().begin();
            }

            std::vector<uint32_t>::const_iterator end() const
            {
                return get().end();
            }

            std::vector<uint32_t>::const_iterator cbegin() const
            {
                return get().cbegin();
            }

            std::vector<uint32_t>::const_iterator cend() const
            {
                return get().cend();
            }

            void clear()
            {
                get().clear();
            }

            void resize(size_t count)
            {
                get().resize(count);
            }

            void resize(size_t count, uint32_t value)
            {
                get().resize(count, value);
            }

            void assign(size_t count, uint32_t value)
            {
                get().assign(count, value);
            }

            void swap(std::vector<uint32_t>& other)
            {
                get().swap(other);
            }

            PixelsProxy& operator=(const std::vector<uint32_t>& other)
            {
                get() = other;
                return *this;
            }

            PixelsProxy& operator=(std::vector<uint32_t>&& other)
            {
                get() = std::move(other);
                return *this;
            }

            bool operator==(const PixelsProxy& other) const
            {
                return get() == other.get();
            }

            bool operator!=(const PixelsProxy& other) const
            {
                return !(*this == other);
            }

            bool operator==(const std::vector<uint32_t>& other) const
            {
                return get() == other;
            }

            bool operator!=(const std::vector<uint32_t>& other) const
            {
                return get() != other;
            }

            friend bool operator==(const std::vector<uint32_t>& lhs, const PixelsProxy& rhs)
            {
                return lhs == rhs.get();
            }

            friend bool operator!=(const std::vector<uint32_t>& lhs, const PixelsProxy& rhs)
            {
                return lhs != rhs.get();
            }

        private:
            Frame* m_owner = nullptr;
        };

        Frame();
        Frame(const Frame& other);
        Frame(Frame&& other) noexcept;
        Frame& operator=(const Frame& other);
        Frame& operator=(Frame&& other) noexcept;

        // 兼容旧代码：等价于访问当前活动图层像素。
        PixelsProxy pixels;

        int getLayerCount() const
        {
            return static_cast<int>(m_layerPixels.size());
        }

        void setProxyLayerIndex(int index);
        std::vector<uint32_t>& getLayerPixels(int index);
        const std::vector<uint32_t>& getLayerPixels(int index) const;
        void insertLayerPixels(int index, size_t pixelCount, uint32_t fillColor);
        bool removeLayerPixels(int index);
        bool moveLayerPixels(int fromIndex, int toIndex);

        const std::vector<std::vector<uint32_t>>& getAllLayerPixels() const
        {
            return m_layerPixels;
        }

        std::vector<std::vector<uint32_t>>& getAllLayerPixels()
        {
            return m_layerPixels;
        }

        void assignLayers(int layerCount, size_t pixelCount, uint32_t fillColor);

    private:
        void ensureValidLayerStorage();
        int getClampedProxyLayerIndex() const;

        int m_proxyLayerIndex = 0;
        std::vector<std::vector<uint32_t>> m_layerPixels;
    };

    // 默认构造：16x16、1 帧、透明填充
    Project();

    // 自定义构造：指定宽高/帧数/填充色
    Project(int width, int height, int frameCount = 1, uint32_t fillColor = 0x00000000);

    // 项目名（UI 展示用）
    const std::string& getName() const 
    { 
        return m_name; 
    }
    void setName(const std::string& name) 
    { 
        m_name = name; 
    }

    // 画布尺寸
    int getWidth() const 
    { 
        return m_width; 
    }
    int getHeight() const 
    { 
        return m_height; 
    }

    // 图层信息与当前活动图层
    int getLayerCount() const
    {
        return static_cast<int>(m_layers.size());
    }

    int getActiveLayerIndex() const
    {
        return m_activeLayerIndex;
    }
    void setActiveLayerIndex(int index);

    LayerInfo& getLayerInfo(int index);
    const LayerInfo& getLayerInfo(int index) const;
    LayerInfo& getActiveLayerInfo();
    const LayerInfo& getActiveLayerInfo() const;

    /**
     * @brief 在当前图层上方新增一层，并同步给所有帧创建空像素层。
     *
     * 图层顺序约定：
     * - 索引越小越靠底部；
     * - 索引越大越靠顶部；
     * - “上移”表示更靠近前景，也就是索引 +1。
     */
    int addLayer(const std::string& name = "", uint32_t fillColor = 0x00000000);

    // 删除/移动图层时会同步处理所有帧的对应像素层。
    bool removeLayer(int index);
    bool moveLayer(int fromIndex, int toIndex);
    bool moveLayerUp(int index);
    bool moveLayerDown(int index);
    bool canMergeLayers(const std::vector<int>& layerIndices) const;
    int mergeLayers(const std::vector<int>& layerIndices, bool keepOriginalLayers);

    // 图层属性修改接口：UI 层后续只需要调用这些函数，不直接改底层 vector。
    void renameLayer(int index, const std::string& name);
    void setLayerVisible(int index, bool visible);
    void setLayerLocked(int index, bool locked);
    void setLayerOpacity(int index, float opacity);
    bool isActiveLayerLocked() const;

    // 帧数量与帧访问
    int getFrameCount() const 
    { 
        return static_cast<int>(m_frames.size()); 
    }
    Frame& getFrame(int index);
    const Frame& getFrame(int index) const;
    std::vector<uint32_t>& getLayerPixels(int frameIndex, int layerIndex);
    const std::vector<uint32_t>& getLayerPixels(int frameIndex, int layerIndex) const;
    std::vector<uint32_t>& getActiveLayerPixels(int frameIndex);
    const std::vector<uint32_t>& getActiveLayerPixels(int frameIndex) const;

    // 按“从底到顶”的图层顺序合成一帧，供画布预览、洋葱皮和后续导出复用。
    std::vector<uint32_t> composeFrame(int frameIndex) const;

    // 调整画布尺寸（保留左上角旧像素，其余用 fillColor 填充）
    void resizeCanvas(int width, int height, uint32_t fillColor = 0x00000000);

    // 调整帧数量（新增帧用 fillColor 填充）
    void setFrameCount(int count, uint32_t fillColor = 0x00000000);

    // 在指定帧之后插入一帧（新增帧用 fillColor 填充）
    void insertFrameAfter(int index, uint32_t fillColor = 0x00000000);

    // 删除指定帧（至少保留 1 帧）
    void removeFrame(int index);

    /**
     * @brief 调整帧顺序：把 fromIndex 的帧移动到 toIndex 位置。
     *
     * 示例（帧序列按索引展示）：
     * - [A,B,C,D], from=1,to=3 => [A,C,D,B]
     * - [A,B,C,D], from=3,to=1 => [A,D,B,C]
     *
     * 说明：
     * - 索引会自动 clamp 到合法范围；
     * - 当 from == to 时不做任何修改。
     */
    void moveFrame(int fromIndex, int toIndex);

    // 时间轴 FPS（用于播放速度与 JSON 持久化）
    int getTimelineFps() const
    {
        return m_timelineFps;
    }
    void setTimelineFps(int fps);

private:
    // 按当前 width_/height_ 创建指定数量的帧并填充像素
    void createFrames(int count, uint32_t fillColor);
    void createDefaultLayer();
    size_t getPixelCount() const;
    std::string makeDefaultLayerName() const;
    void syncFrameProxyLayerIndices();

    // 项目信息
    std::string m_name = "Untitled";
    int m_width = 0;
    int m_height = 0;
    int m_timelineFps = 8;

    // 项目级图层结构（所有帧共享同一套层定义）
    std::vector<LayerInfo> m_layers;
    int m_activeLayerIndex = 0;

    // 帧列表
    std::vector<Frame> m_frames;
};
