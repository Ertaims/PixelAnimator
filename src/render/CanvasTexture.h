#pragma once

#include <cstdint>
#include <vector>

namespace render
{
    /**
     * @brief 管理画布显示用 OpenGL 纹理。
     *
     * 画布纹理使用 NEAREST 过滤，确保像素动画缩放时保持硬边。
     */
    class CanvasTexture
    {
    public:
        CanvasTexture() = default;
        CanvasTexture(const CanvasTexture&) = delete;
        CanvasTexture& operator=(const CanvasTexture&) = delete;

        /**
         * @brief 释放当前画布纹理并重置尺寸状态。
         */
        void release();

        /**
         * @brief 确保纹理已创建且尺寸匹配画布。
         *
         * 尺寸变化时会重新分配纹理存储，但不会上传像素内容。
         */
        void ensureSize(int width, int height);

        /**
         * @brief 将 RGBA 像素缓冲上传到当前画布纹理。
         */
        void uploadPixels(const std::vector<uint32_t>& pixels) const;

        // 返回 OpenGL 纹理 ID，供 ImGui::Image 使用。
        unsigned int id() const { return m_texture; }
        int width() const { return m_width; }
        int height() const { return m_height; }

    private:
        unsigned int m_texture = 0;
        int m_width = 0;
        int m_height = 0;
    };
}
