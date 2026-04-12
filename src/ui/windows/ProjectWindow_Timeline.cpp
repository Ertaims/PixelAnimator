#include "ProjectWindow.h"

#include "core/AppContext.h"
#include "core/Project.h"
#include "imgui.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <cstdio>
#include <vector>

namespace
{
    GLuint loadTextureFromFile(const char* path)
    {
        SDL_Surface* surface = IMG_Load(path);
        if (!surface) return 0;

        SDL_Surface* rgbaSurface = surface;
        if (surface->format != SDL_PIXELFORMAT_RGBA32)
        {
            rgbaSurface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
            SDL_DestroySurface(surface);
            if (!rgbaSurface) return 0;
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
        if (loaded) return;

        const char* playCandidates[] = {"src/assets/start.png", "../src/assets/start.png", "../../src/assets/start.png"};
        const char* pauseCandidates[] = {"src/assets/pause.png", "../src/assets/pause.png", "../../src/assets/pause.png"};

        for (const char* p : playCandidates)
        {
            playIcon = loadTextureFromFile(p);
            if (playIcon != 0) break;
        }
        for (const char* p : pauseCandidates)
        {
            pauseIcon = loadTextureFromFile(p);
            if (pauseIcon != 0) break;
        }

        loaded = true;
    }

    ImVec4 rgbaToImVec4(uint32_t rgba)
    {
        const float r = static_cast<float>((rgba >> 0) & 0xFF) / 255.0f;
        const float g = static_cast<float>((rgba >> 8) & 0xFF) / 255.0f;
        const float b = static_cast<float>((rgba >> 16) & 0xFF) / 255.0f;
        const float a = static_cast<float>((rgba >> 24) & 0xFF) / 255.0f;
        return ImVec4(r, g, b, a);
    }

    uint32_t pickGroupColorByIndex(size_t index)
    {
        // 预置一组区分度较高的颜色，方便时间轴直观区分不同分组。
        static const uint32_t kPalette[] = {
            0x5EA1FFFFu, // 天蓝
            0x6ED6A0FFu, // 薄荷绿
            0xF2B566FFu, // 橙黄
            0xC98CFFFFu, // 紫粉
            0x82D8F4FFu, // 青蓝
            0xF48AA1FFu, // 珊瑚粉
            0xA2D56DFFu, // 黄绿
            0xF4D36AFFu  // 暖黄
        };
        return kPalette[index % (sizeof(kPalette) / sizeof(kPalette[0]))];
    }

    int findFrameGroupIndex(const std::vector<AppContext::FrameGroup>& groups, int frameIndex)
    {
        for (size_t gi = 0; gi < groups.size(); ++gi)
        {
            const std::vector<int>& frames = groups[gi].frameIndices;
            if (std::find(frames.begin(), frames.end(), frameIndex) != frames.end()) return static_cast<int>(gi);
        }
        return -1;
    }
} // namespace

void ProjectWindow::renderTimelinePanel(Project* project)
{
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 2.0f));

    if (timelineState_.lastTick == 0) timelineState_.lastTick = SDL_GetTicks();
    const uint64_t nowTick = SDL_GetTicks();
    const double dt = static_cast<double>(nowTick - timelineState_.lastTick) / 1000.0;
    timelineState_.lastTick = nowTick;
    timelineState_.fps = static_cast<float>(project->getTimelineFps());
    if (timelineState_.isPlaying && timelineState_.fps > 0.0f) timelineState_.accumulator += dt;

    // 进入时间轴渲染前先做一次选区校正：
    // - 删除越界选中项
    // - 保证至少有一个选中帧
    context->sanitizeFrameSelection(project->getFrameCount(), context->getCurrentFrameIndex());

    {
        const ImVec2 btnSize(22.0f, 18.0f);
        if (ImGui::Button("<<", btnSize)) context->setSingleFrameSelection(0, project->getFrameCount());
        ImGui::SameLine();
        if (ImGui::Button("<", btnSize))
        {
            const int frameCount = project->getFrameCount();
            const int current = context->getCurrentFrameIndex();
            if (frameCount > 0) context->setSingleFrameSelection(std::max(0, current - 1), frameCount);
        }
        ImGui::SameLine();
        if (ImGui::Button(">", btnSize))
        {
            const int frameCount = project->getFrameCount();
            const int current = context->getCurrentFrameIndex();
            if (frameCount > 0) context->setSingleFrameSelection(std::min(frameCount - 1, current + 1), frameCount);
        }
        ImGui::SameLine();
        if (ImGui::Button(">>", btnSize))
        {
            const int frameCount = project->getFrameCount();
            if (frameCount > 0) context->setSingleFrameSelection(frameCount - 1, frameCount);
        }
        ImGui::SameLine();
        const char* loopLabel = timelineState_.loopEnabled ? "Loop" : "Once";
        if (ImGui::Button(loopLabel, ImVec2(44.0f, 18.0f))) timelineState_.loopEnabled = !timelineState_.loopEnabled;
        ImGui::SameLine();
        if (ImGui::Button("+", btnSize))
        {
            const int current = context->getCurrentFrameIndex();
            project->insertFrameAfter(current, 0x00000000);
            // 插帧后同步分组索引：
            // - 新帧索引 = current + 1；
            // - 若当前帧属于某分组，新帧自动并入该分组并跟在当前帧后面。
            context->onFrameInserted(current + 1, current, project->getFrameCount());
            context->setSingleFrameSelection(current + 1, project->getFrameCount());
            context->setProjectDirty(true, "Insert Frame");
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
                // 删帧后同步分组索引，避免组成员索引错位。
                context->onFrameRemoved(current, newCount);
                context->setSingleFrameSelection(std::min(current, newCount - 1), newCount);
                context->setProjectDirty(true, "Delete Frame");
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
    if (clickedToggle) timelineState_.isPlaying = !timelineState_.isPlaying;

    ImGui::Separator();

    ImGui::TextUnformatted("FPS");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    if (ImGui::SliderFloat("##timeline_fps", &timelineState_.fps, 1.0f, 60.0f, "%.0f"))
    {
        project->setTimelineFps(static_cast<int>(timelineState_.fps + 0.5f));
        timelineState_.fps = static_cast<float>(project->getTimelineFps());
        context->setProjectDirty(true, "Change FPS");
    }

    ImGui::Separator();

    const int frameCount = project->getFrameCount();
    context->sanitizeFrameGroups(frameCount);
    const std::vector<AppContext::FrameGroup>& frameGroups = context->getFrameGroups();

    // 当前显示帧取“主选中帧”（多选时选区第一个）。
    context->sanitizeFrameSelection(frameCount, context->getCurrentFrameIndex());
    int current = context->getPrimarySelectedFrameIndex();
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
            // 播放切帧属于“显示帧切换”，同步为单选，避免与手动多选语义冲突。
            context->setSingleFrameSelection(next, frameCount);
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

    // 由于下面会遍历 frameGroups 引用，为避免遍历期间直接修改容器导致引用失效，
    // 分组删除操作采用“延迟执行”策略：先记录索引，循环结束后再真正删除。
    int pendingDeleteGroupIndex = -1;

    for (int i = 0; i < frameCount; ++i)
    {
        ImGui::PushID(i);
        // 多选高亮：选区内任意帧都高亮；主帧会因 current 同步显示在画布。
        const std::vector<int> selectedFrames = context->getSelectedFrameIndices();
        const bool selected = std::find(selectedFrames.begin(), selectedFrames.end(), i) != selectedFrames.end();
        const int groupIndex = findFrameGroupIndex(frameGroups, i);
        const bool grouped = groupIndex >= 0;
        const ImVec4 groupCol = grouped
            ? rgbaToImVec4(frameGroups[static_cast<size_t>(groupIndex)].colorRGBA)
            : ImVec4(0.35f, 0.35f, 0.35f, 0.9f);

        // 颜色策略：
        // - 普通：灰色
        // - 分组：使用组颜色
        // - 选中：在当前底色基础上进一步提亮，保证选中态优先可见
        ImVec4 col = groupCol;
        if (selected)
        {
            col.x = std::min(1.0f, col.x + 0.15f);
            col.y = std::min(1.0f, col.y + 0.15f);
            col.z = std::min(1.0f, col.z + 0.15f);
            col.w = 0.95f;
        }
        ImGui::PushStyleColor(ImGuiCol_Button, col);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(col.x + 0.1f, col.y + 0.1f, col.z + 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(col.x + 0.15f, col.y + 0.15f, col.z + 0.15f, 1.0f));
        if (ImGui::Button("##frame_cell", ImVec2(cellW, cellH)))
        {
            // Ctrl+点击：切换多选；普通点击：单选。
            if (ImGui::GetIO().KeyCtrl) context->toggleFrameSelection(i, frameCount);
            else
                context->setSingleFrameSelection(i, frameCount);

            // 画布总是显示主选中帧。
            context->setCurrentFrameIndex(context->getPrimarySelectedFrameIndex());
        }

        // 帧顺序拖拽：
        // - 从某个帧单元格开始拖拽；
        // - 在目标帧单元格释放后，把源帧移动到目标索引位置。
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoDisableHover))
        {
            timelineState_.draggingFrameIndex = i;
            ImGui::SetDragDropPayload("TIMELINE_FRAME_INDEX", &i, sizeof(int));
            ImGui::Text("Move Frame %d", i + 1);
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TIMELINE_FRAME_INDEX"))
            {
                if (payload->DataSize == sizeof(int))
                {
                    const int fromIndex = *static_cast<const int*>(payload->Data);
                    const int toIndex = i;
                    if (fromIndex != toIndex)
                    {
                        project->moveFrame(fromIndex, toIndex);
                        context->onFrameMoved(fromIndex, toIndex, frameCount);
                        context->setProjectDirty(true, "Reorder Frames");
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        // 为已分组帧绘制一个顶部细条，增强视觉区分度（即使未选中也可识别归组）。
        if (grouped)
        {
            const ImVec2 minPos = ImGui::GetItemRectMin();
            const ImVec2 maxPos = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(minPos.x, minPos.y),
                ImVec2(maxPos.x, minPos.y + 2.5f),
                ImGui::GetColorU32(groupCol));
        }

        // 右键菜单：对“当前多选帧”创建分组。
        if (ImGui::BeginPopupContextItem("##frame_context"))
        {
            if (grouped)
            {
                const AppContext::FrameGroup& hitGroup = frameGroups[static_cast<size_t>(groupIndex)];
                if (ImGui::MenuItem("Rename Group..."))
                {
                    timelineState_.renameGroupIndex = groupIndex;
                    std::snprintf(timelineState_.renameGroupName,
                                  sizeof(timelineState_.renameGroupName),
                                  "%s",
                                  hitGroup.name.c_str());
                    timelineState_.openRenameGroupPopup = true;
                }
                if (ImGui::MenuItem("Delete Group"))
                {
                    pendingDeleteGroupIndex = groupIndex;
                }
                ImGui::Separator();
            }

            const size_t selectedCount = selectedFrames.size();
            if (selectedCount >= 2)
            {
                if (ImGui::MenuItem("Group Selected Frames..."))
                {
                    timelineState_.pendingGroupFrames = selectedFrames;
                    std::snprintf(timelineState_.pendingGroupName,
                                  sizeof(timelineState_.pendingGroupName),
                                  "Group %d",
                                  static_cast<int>(frameGroups.size() + 1));
                    timelineState_.openCreateGroupNamePopup = true;
                }
            }
            else
            {
                ImGui::BeginDisabled();
                ImGui::MenuItem("Group Selected Frames...");
                ImGui::EndDisabled();
                ImGui::TextUnformatted("Tip: Ctrl+Click select at least 2 frames.");
            }
            ImGui::EndPopup();
        }

        ImGui::PopStyleColor(3);
        ImGui::PopID();
        ImGui::SameLine();
    }

    if (pendingDeleteGroupIndex >= 0)
    {
        context->removeFrameGroup(pendingDeleteGroupIndex);
        context->setProjectDirty(true, "Delete Group");
    }

    // 统一在帧区域末尾打开命名弹窗，避免与单元格循环中的 PushID 状态耦合。
    if (timelineState_.openCreateGroupNamePopup)
    {
        ImGui::OpenPopup("Create Frame Group");
        timelineState_.openCreateGroupNamePopup = false;
    }

    if (timelineState_.openRenameGroupPopup)
    {
        ImGui::OpenPopup("Rename Frame Group");
        timelineState_.openRenameGroupPopup = false;
    }

    if (ImGui::BeginPopupModal("Create Frame Group", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Group Name");
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputText("##group_name", timelineState_.pendingGroupName, sizeof(timelineState_.pendingGroupName));

        ImGui::Separator();
        ImGui::Text("Frames: %d selected", static_cast<int>(timelineState_.pendingGroupFrames.size()));

        if (ImGui::Button("Create", ImVec2(120.0f, 0.0f)))
        {
            const uint32_t color = pickGroupColorByIndex(frameGroups.size());
            context->addFrameGroup(timelineState_.pendingGroupName,
                                   timelineState_.pendingGroupFrames,
                                   frameCount,
                                   color);
            context->setProjectDirty(true, "Create Group");
            timelineState_.pendingGroupFrames.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
        {
            timelineState_.pendingGroupFrames.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Rename Frame Group", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("New Group Name");
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputText("##rename_group_name",
                         timelineState_.renameGroupName,
                         sizeof(timelineState_.renameGroupName));

        if (ImGui::Button("Apply", ImVec2(120.0f, 0.0f)))
        {
            context->renameFrameGroup(timelineState_.renameGroupIndex, timelineState_.renameGroupName);
            context->setProjectDirty(true, "Rename Group");
            timelineState_.renameGroupIndex = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
        {
            timelineState_.renameGroupIndex = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::EndChild();
    ImGui::EndChild();
}
