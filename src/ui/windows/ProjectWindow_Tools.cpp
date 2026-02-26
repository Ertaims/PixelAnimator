#include "ProjectWindow.h"

#include "core/AppContext.h"
#include "imgui.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3_image/SDL_image.h>

namespace
{
GLuint loadTextureFromFile(const char* path)
{
    SDL_Surface* surface = IMG_Load(path);
    if (!surface)
        return 0;

    SDL_Surface* rgbaSurface = surface;
    if (surface->format != SDL_PIXELFORMAT_RGBA32)
    {
        rgbaSurface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
        SDL_DestroySurface(surface);
        if (!rgbaSurface)
            return 0;
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, rgbaSurface->w, rgbaSurface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbaSurface->pixels);

    SDL_DestroySurface(rgbaSurface);
    return texture;
}

void ensureToolbarIconTextures(GLuint& brushIcon,
                               GLuint& eraserIcon,
                               GLuint& eyedropperIcon,
                               GLuint& fillIcon,
                               bool& loaded)
{
    if (loaded)
        return;

    const char* brushCandidates[] = {"src/assets/paintbrush.png", "../src/assets/paintbrush.png", "../../src/assets/paintbrush.png"};
    const char* eraserCandidates[] = {"src/assets/erase.png", "../src/assets/erase.png", "../../src/assets/erase.png"};
    const char* eyedropperCandidates[] = {"src/assets/eyedropper.png", "../src/assets/eyedropper.png", "../../src/assets/eyedropper.png"};
    const char* fillCandidates[] = {"src/assets/fill.png", "../src/assets/fill.png", "../../src/assets/fill.png"};

    for (const char* p : brushCandidates)
    {
        brushIcon = loadTextureFromFile(p);
        if (brushIcon != 0)
            break;
    }
    for (const char* p : eraserCandidates)
    {
        eraserIcon = loadTextureFromFile(p);
        if (eraserIcon != 0)
            break;
    }
    for (const char* p : eyedropperCandidates)
    {
        eyedropperIcon = loadTextureFromFile(p);
        if (eyedropperIcon != 0)
            break;
    }
    for (const char* p : fillCandidates)
    {
        fillIcon = loadTextureFromFile(p);
        if (fillIcon != 0)
            break;
    }

    loaded = true;
}
} // namespace

void ProjectWindow::renderToolbarPanel()
{
    ensureToolbarIconTextures(
        toolbarState_.brushIconTexture,
        toolbarState_.eraserIconTexture,
        toolbarState_.eyedropperIconTexture,
        toolbarState_.fillIconTexture,
        toolbarState_.iconsLoaded);

    ImGui::TextUnformatted("Tools");
    ImGui::Separator();

    struct ToolbarItem
    {
        ToolType tool;
        const char* label;
        unsigned int icon;
    };

    const ToolbarItem items[] = {
        {ToolType::Brush, "Brush", toolbarState_.brushIconTexture},
        {ToolType::Eraser, "Eraser", toolbarState_.eraserIconTexture},
        {ToolType::Eyedropper, "Eyedropper", toolbarState_.eyedropperIconTexture},
        {ToolType::Fill, "Fill", toolbarState_.fillIconTexture}
    };

    const ImVec2 iconSize(26.0f, 26.0f);
    for (const ToolbarItem& item : items)
    {
        const bool selected = (context->getTool() == item.tool);
        ImGui::PushID(static_cast<int>(item.tool));
        if (selected)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.52f, 0.86f, 1.0f));

        bool clicked = false;
        if (item.icon != 0)
        {
            clicked = ImGui::ImageButton(
                "##toolbar_icon",
                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(item.icon)),
                iconSize);
        }
        else
        {
            clicked = ImGui::Button(item.label, ImVec2(80.0f, 26.0f));
        }

        if (selected)
        {
            ImGui::PopStyleColor();
            ImGui::GetWindowDrawList()->AddRect(
                ImGui::GetItemRectMin(),
                ImGui::GetItemRectMax(),
                IM_COL32(255, 220, 40, 255),
                4.0f,
                0,
                2.0f);
        }

        if (clicked)
            context->setTool(item.tool);

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", item.label);

        ImGui::PopID();
    }
}
