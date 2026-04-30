#pragma once

namespace render
{
    /**
     * @brief 从图片文件创建 OpenGL 纹理。
     *
     * 默认使用线性过滤，适合 UI 图标；像素画布请使用 CanvasTexture。
     */
    unsigned int loadTextureFromFile(const char* path);

    /**
     * @brief 释放 OpenGL 纹理并把 ID 置 0。
     *
     * 调用方可安全重复调用；texture 为 0 时不会做任何事。
     */
    void deleteTexture(unsigned int& texture);
}
