#include "ProjectWindow.h"

#include "core/AppContext.h"
#include "core/Project.h"
#include "imgui.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3_image/SDL_image.h>

#include <algorithm>

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

void ensureTimelineIconTextures(GLuint& playIcon, GLuint& pauseIcon, bool& loaded)
{
    if (loaded)
        return;

    const char* playCandidates[] = {"src/assets/start.png", "../src/assets/start.png", "../../src/assets/start.png"};
    const char* pauseCandidates[] = {"src/assets/pause.png", "../src/assets/pause.png", "../../src/assets/pause.png"};

    for (const char* p : playCandidates)
    {
        playIcon = loadTextureFromFile(p);
        if (playIcon != 0)
            break;
    }
    for (const char* p : pauseCandidates)
    {
        pauseIcon = loadTextureFromFile(p);
        if (pauseIcon != 0)
            break;
    }

    loaded = true;
}
} // namespace

void ProjectWindow::renderTimelinePanel(Project* project)
{
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 2.0f));

    if (timelineState_.lastTick == 0)
        timelineState_.lastTick = SDL_GetTicks();
    const uint64_t nowTick = SDL_GetTicks();
    const double dt = static_cast<double>(nowTick - timelineState_.lastTick) / 1000.0;
    timelineState_.lastTick = nowTick;
    if (timelineState_.isPlaying && timelineState_.fps > 0.0f)
        timelineState_.accumulator += dt;

    {
        const ImVec2 btnSize(22.0f, 18.0f);
        if (ImGui::Button("<<", btnSize))
            context->setCurrentFrameIndex(0);
        ImGui::SameLine();
        if (ImGui::Button("<", btnSize))
        {
            const int frameCount = project->getFrameCount();
            const int current = context->getCurrentFrameIndex();
            if (frameCount > 0)
                context->setCurrentFrameIndex(std::max(0, current - 1));
        }
        ImGui::SameLine();
        if (ImGui::Button(">", btnSize))
        {
            const int frameCount = project->getFrameCount();
            const int current = context->getCurrentFrameIndex();
            if (frameCount > 0)
                context->setCurrentFrameIndex(std::min(frameCount - 1, current + 1));
        }
        ImGui::SameLine();
        if (ImGui::Button(">>", btnSize))
        {
            const int frameCount = project->getFrameCount();
            if (frameCount > 0)
                context->setCurrentFrameIndex(frameCount - 1);
        }
        ImGui::SameLine();
        const char* loopLabel = timelineState_.loopEnabled ? "Loop" : "Once";
        if (ImGui::Button(loopLabel, ImVec2(44.0f, 18.0f)))
            timelineState_.loopEnabled = !timelineState_.loopEnabled;
        ImGui::SameLine();
        if (ImGui::Button("+", btnSize))
        {
            const int current = context->getCurrentFrameIndex();
            project->insertFrameAfter(current, 0x00000000);
            context->setCurrentFrameIndex(current + 1);
            context->setProjectDirty(true);
        }
        ImGui::SameLine();
        if (ImGui::Button("-", btnSize))
        {
            const int frameCount = project->getFrameCount();
            if (frameCount > 1)
            {
                const int current = context->getCurrentFrameIndex();
                project->removeFrame(current);
                const int newCount = project->getFrameCount();
                context->setCurrentFrameIndex(std::min(current, newCount - 1));
                context->setProjectDirty(true);
            }
        }
    }

    ImGui::PopStyleVar(2);

    const float leftPanelWidth = 120.0f;
    const float timelineHeight = ImGui::GetContentRegionAvail().y;
    ImGui::BeginChild("##TimelineMain", ImVec2(0.0f, timelineHeight), false);

    ImGui::BeginChild("##TimelineLeft", ImVec2(leftPanelWidth, 0.0f), true);
    ImGui::TextUnformatted("Layers");
    ImGui::Separator();
    ImGui::TextUnformatted("1  Background");
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##TimelineFrames", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_HorizontalScrollbar);

    ensureTimelineIconTextures(
        timelineState_.playIconTexture,
        timelineState_.pauseIconTexture,
        timelineState_.iconsLoaded);

    const ImVec2 iconSize(20.0f, 20.0f);
    bool clickedToggle = false;
    if (timelineState_.isPlaying)
    {
        if (timelineState_.pauseIconTexture != 0)
        {
            clickedToggle = ImGui::ImageButton(
                "##timeline_toggle",
                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(timelineState_.pauseIconTexture)),
                iconSize);
        }
        else
        {
            clickedToggle = ImGui::Button("Pause");
        }
    }
    else
    {
        if (timelineState_.playIconTexture != 0)
        {
            clickedToggle = ImGui::ImageButton(
                "##timeline_toggle",
                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(timelineState_.playIconTexture)),
                iconSize);
        }
        else
        {
            clickedToggle = ImGui::Button("Play");
        }
    }
    if (clickedToggle)
        timelineState_.isPlaying = !timelineState_.isPlaying;

    ImGui::Separator();

    ImGui::TextUnformatted("FPS");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    ImGui::SliderFloat("##timeline_fps", &timelineState_.fps, 1.0f, 60.0f, "%.0f");

    ImGui::Separator();

    const int frameCount = project->getFrameCount();
    int current = context->getCurrentFrameIndex();
    current = std::clamp(current, 0, std::max(0, frameCount - 1));
    context->setCurrentFrameIndex(current);

    if (timelineState_.isPlaying && timelineState_.fps > 0.0f && frameCount > 0)
    {
        const double frameDuration = 1.0 / static_cast<double>(timelineState_.fps);
        while (timelineState_.accumulator >= frameDuration)
        {
            timelineState_.accumulator -= frameDuration;
            int next = context->getCurrentFrameIndex() + 1;
            if (next >= frameCount)
            {
                if (timelineState_.loopEnabled)
                {
                    next = 0;
                }
                else
                {
                    next = frameCount - 1;
                    timelineState_.isPlaying = false;
                    timelineState_.accumulator = 0.0;
                }
            }
            context->setCurrentFrameIndex(next);
        }
    }

    const float cellW = 36.0f;
    const float cellH = 24.0f;
    const float headerH = 18.0f;
    for (int i = 0; i < frameCount; ++i)
    {
        ImGui::PushID(1000 + i);
        ImGui::BeginGroup();
        ImGui::Text(" %d", i + 1);
        ImGui::EndGroup();
        ImGui::PopID();
        ImGui::SameLine(0.0f, cellW - 8.0f);
    }

    ImGui::Dummy(ImVec2(0.0f, headerH));

    for (int i = 0; i < frameCount; ++i)
    {
        ImGui::PushID(i);
        const bool selected = (i == current);
        const ImVec4 col = selected ? ImVec4(0.2f, 0.5f, 0.9f, 0.9f) : ImVec4(0.35f, 0.35f, 0.35f, 0.9f);
        ImGui::PushStyleColor(ImGuiCol_Button, col);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(col.x + 0.1f, col.y + 0.1f, col.z + 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(col.x + 0.15f, col.y + 0.15f, col.z + 0.15f, 1.0f));
        if (ImGui::Button("##frame_cell", ImVec2(cellW, cellH)))
            context->setCurrentFrameIndex(i);
        ImGui::PopStyleColor(3);
        ImGui::PopID();
        ImGui::SameLine();
    }

    ImGui::EndChild();
    ImGui::EndChild();
}
