#include "render/CanvasTexture.h"

#include "render/Texture.h"

#include <SDL3/SDL_opengl.h>

namespace render
{
    void CanvasTexture::release()
    {
        deleteTexture(m_texture);
        m_width = 0;
        m_height = 0;
    }

    void CanvasTexture::ensureSize(int width, int height)
    {
        if (m_texture == 0)
        {
            glGenTextures(1, &m_texture);
            glBindTexture(GL_TEXTURE_2D, m_texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }

        if (width == m_width && height == m_height) return;

        m_width = width;
        m_height = height;
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }

    void CanvasTexture::uploadPixels(const std::vector<uint32_t>& pixels) const
    {
        if (m_texture == 0 || m_width <= 0 || m_height <= 0 || pixels.empty()) return;

        glBindTexture(GL_TEXTURE_2D, m_texture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(
            GL_TEXTURE_2D,
            0,
            0,
            0,
            m_width,
            m_height,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            pixels.data());
    }
}
