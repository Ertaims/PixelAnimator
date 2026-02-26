#include "ProjectWindow.h"

#include "core/AppContext.h"
#include "core/Project.h"
#include "imgui.h"

#include <SDL3/SDL_opengl.h>

#include <algorithm>
#include <vector>

ProjectWindow::~ProjectWindow()
{
    if (canvasTexture_.texture != 0)
    {
        glDeleteTextures(1, &canvasTexture_.texture);
        canvasTexture_.texture = 0;
    }
    if (timelineState_.playIconTexture != 0)
    {
        glDeleteTextures(1, &timelineState_.playIconTexture);
        timelineState_.playIconTexture = 0;
    }
    if (timelineState_.pauseIconTexture != 0)
    {
        glDeleteTextures(1, &timelineState_.pauseIconTexture);
        timelineState_.pauseIconTexture = 0;
    }
    if (toolbarState_.brushIconTexture != 0)
    {
        glDeleteTextures(1, &toolbarState_.brushIconTexture);
        toolbarState_.brushIconTexture = 0;
    }
    if (toolbarState_.eraserIconTexture != 0)
    {
        glDeleteTextures(1, &toolbarState_.eraserIconTexture);
        toolbarState_.eraserIconTexture = 0;
    }
    if (toolbarState_.eyedropperIconTexture != 0)
    {
        glDeleteTextures(1, &toolbarState_.eyedropperIconTexture);
        toolbarState_.eyedropperIconTexture = 0;
    }
    if (toolbarState_.fillIconTexture != 0)
    {
        glDeleteTextures(1, &toolbarState_.fillIconTexture);
        toolbarState_.fillIconTexture = 0;
    }
}

void ProjectWindow::ensureCanvasTexture(int width, int height)
{
    if (canvasTexture_.texture == 0)
    {
        glGenTextures(1, &canvasTexture_.texture);
        glBindTexture(GL_TEXTURE_2D, canvasTexture_.texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    if (width != canvasTexture_.width || height != canvasTexture_.height)
    {
        canvasTexture_.width = width;
        canvasTexture_.height = height;
        glBindTexture(GL_TEXTURE_2D, canvasTexture_.texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }
}

void ProjectWindow::uploadCanvasPixels(const std::vector<uint32_t>& pixels) const
{
    glBindTexture(GL_TEXTURE_2D, canvasTexture_.texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        0,
        0,
        canvasTexture_.width,
        canvasTexture_.height,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels.data());
}

void ProjectWindow::render()
{
    if (!visible)
        return;

    const char* label = windowLabel_.empty() ? name : windowLabel_.c_str();
    if (!ImGui::Begin(label, &visible))
    {
        ImGui::End();
        return;
    }

    if (onFocused_ && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
    {
        onFocused_(context);
    }

    if (!context || !context->hasProject())
    {
        ImGui::TextUnformatted("No project loaded.");
        ImGui::End();
        return;
    }

    Project* project = context->getProject();

    const float splitterHeight = 2.0f;
    const float minTopHeight = 120.0f;
    const float minTimelineHeight = 80.0f;
    const float availableHeight = ImGui::GetContentRegionAvail().y;
    const float maxTimelineHeight = std::max(minTimelineHeight, availableHeight - minTopHeight - splitterHeight);
    timelineState_.height = std::clamp(timelineState_.height, minTimelineHeight, maxTimelineHeight);
    const float topHeight = std::max(minTopHeight, availableHeight - timelineState_.height - splitterHeight);

    if (ImGui::BeginChild("##ProjectTopRegion", ImVec2(0.0f, topHeight), false))
    {
        if (ImGui::BeginTable("##ProjectMainColumns", 4, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Left", ImGuiTableColumnFlags_WidthStretch, 0.90f);
            ImGui::TableSetupColumn(
                "Tools",
                ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize,
                50.0f);
            ImGui::TableSetupColumn("Center", ImGuiTableColumnFlags_WidthStretch, 1.75f);
            ImGui::TableSetupColumn("Right", ImGuiTableColumnFlags_WidthStretch, 0.90f);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            if (ImGui::BeginChild("##LeftPanel", ImVec2(0.0f, 0.0f), true))
            {
                renderLeftPanel(project);
            }
            ImGui::EndChild();

            ImGui::TableSetColumnIndex(1);
            if (ImGui::BeginChild("##ToolBarPanel", ImVec2(0.0f, 0.0f), true))
            {
                renderToolbarPanel();
            }
            ImGui::EndChild();

            ImGui::TableSetColumnIndex(2);
            if (ImGui::BeginChild("##CanvasPanel", ImVec2(0.0f, 0.0f), true))
            {
                renderCanvasPanel(project);
            }
            ImGui::EndChild();

            ImGui::TableSetColumnIndex(3);
            if (ImGui::BeginChild("##ToolPropsPanel", ImVec2(0.0f, 0.0f), true))
            {
                renderRightPanel(project);
            }
            ImGui::EndChild();

            ImGui::EndTable();
        }
    }
    ImGui::EndChild();

    ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y));
    ImGui::InvisibleButton("##TimelineSplitter", ImVec2(-1.0f, splitterHeight));
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        const float deltaY = ImGui::GetIO().MouseDelta.y;
        timelineState_.height = std::clamp(timelineState_.height - deltaY, minTimelineHeight, maxTimelineHeight);
    }
    if (ImGui::IsItemHovered() || ImGui::IsItemActive())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }

    if (ImGui::BeginChild("##TimelinePanel", ImVec2(0.0f, 0.0f), true))
    {
        renderTimelinePanel(project);
    }
    ImGui::EndChild();

    ImGui::End();
}
