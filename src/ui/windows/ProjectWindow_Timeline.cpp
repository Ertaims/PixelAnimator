#include "ProjectWindow.h"

#include "core/AppContext.h"
#include "core/Project.h"
#include "imgui.h"
#include "render/Texture.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <vector>

namespace
{
    void ensureTimelineIconTextures(unsigned int& playIcon, unsigned int& pauseIcon, bool& loaded)
    {
        if (loaded) return;

        const char* playCandidates[] = {"src/assets/start.png", "../src/assets/start.png", "../../src/assets/start.png"};
        const char* pauseCandidates[] = {"src/assets/pause.png", "../src/assets/pause.png", "../../src/assets/pause.png"};

        for (const char* p : playCandidates)
        {
            playIcon = render::loadTextureFromFile(p);
            if (playIcon != 0) break;
        }
        for (const char* p : pauseCandidates)
        {
            pauseIcon = render::loadTextureFromFile(p);
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

    std::vector<int> buildPlaybackFramesForCurrentFrame(const std::vector<AppContext::FrameGroup>& groups,
                                                        int currentFrame,
                                                        int frameCount)
    {
        std::vector<int> playbackFrames;
        if (frameCount <= 0) return playbackFrames;

        const int groupIndex = findFrameGroupIndex(groups, currentFrame);
        if (groupIndex < 0)
        {
            // 当前帧不属于任何分组时，保持旧行为：播放完整时间轴。
            playbackFrames.reserve(static_cast<size_t>(frameCount));
            for (int i = 0; i < frameCount; ++i) playbackFrames.push_back(i);
            return playbackFrames;
        }

        // 当前帧属于某个分组时，只播放该组内帧。
        // 这里按时间轴索引排序，避免 Ctrl 多选的点击顺序影响播放顺序。
        playbackFrames = groups[static_cast<size_t>(groupIndex)].frameIndices;
        playbackFrames.erase(
            std::remove_if(
                playbackFrames.begin(),
                playbackFrames.end(),
                [frameCount](int index) { return index < 0 || index >= frameCount; }),
            playbackFrames.end());
        std::sort(playbackFrames.begin(), playbackFrames.end());
        playbackFrames.erase(std::unique(playbackFrames.begin(), playbackFrames.end()), playbackFrames.end());

        if (playbackFrames.empty()) playbackFrames.push_back(std::clamp(currentFrame, 0, frameCount - 1));
        return playbackFrames;
    }

    int findNextPlaybackFrame(const std::vector<int>& playbackFrames,
                              int currentFrame,
                              bool loopEnabled,
                              bool& outReachedEnd)
    {
        outReachedEnd = false;
        if (playbackFrames.empty()) return currentFrame;

        const auto currentIt = std::lower_bound(playbackFrames.begin(), playbackFrames.end(), currentFrame);
        if (currentIt != playbackFrames.end() && *currentIt == currentFrame)
        {
            const auto nextIt = currentIt + 1;
            if (nextIt != playbackFrames.end()) return *nextIt;
        }
        else if (currentIt != playbackFrames.end())
        {
            // 当前帧不在播放集合但位于集合中间时，前进到后面的第一个合法帧。
            return *currentIt;
        }

        outReachedEnd = true;
        return loopEnabled ? playbackFrames.front() : playbackFrames.back();
    }
} // namespace

void ProjectWindow::renderTimelinePanel(Project* project)
{
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 2.0f));

    if (m_timelineState.lastTick == 0) m_timelineState.lastTick = SDL_GetTicks();
    const uint64_t nowTick = SDL_GetTicks();
    const double dt = static_cast<double>(nowTick - m_timelineState.lastTick) / 1000.0;
    m_timelineState.lastTick = nowTick;
    m_timelineState.fps = static_cast<float>(project->getTimelineFps());
    if (m_timelineState.isPlaying && m_timelineState.fps > 0.0f) m_timelineState.accumulator += dt;

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
        const char* loopLabel = m_timelineState.loopEnabled ? "Loop" : "Once";
        if (ImGui::Button(loopLabel, ImVec2(44.0f, 18.0f))) m_timelineState.loopEnabled = !m_timelineState.loopEnabled;
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

    ImGui::BeginChild("##TimelineFrames", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_HorizontalScrollbar);

    ensureTimelineIconTextures(
        m_timelineState.playIconTexture,
        m_timelineState.pauseIconTexture,
        m_timelineState.iconsLoaded);

    const ImVec2 iconSize(20.0f, 20.0f);
    bool clickedToggle = false;
    if (m_timelineState.isPlaying)
    {
        if (m_timelineState.pauseIconTexture != 0)
        {
            clickedToggle = ImGui::ImageButton(
                "##timeline_toggle",
                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(m_timelineState.pauseIconTexture)),
                iconSize);
        }
        else
        {
            clickedToggle = ImGui::Button("Pause");
        }
    }
    else
    {
        if (m_timelineState.playIconTexture != 0)
        {
            clickedToggle = ImGui::ImageButton(
                "##timeline_toggle",
                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(m_timelineState.playIconTexture)),
                iconSize);
        }
        else
        {
            clickedToggle = ImGui::Button("Play");
        }
    }
    if (clickedToggle) m_timelineState.isPlaying = !m_timelineState.isPlaying;

    ImGui::Separator();

    ImGui::TextUnformatted("FPS");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    if (ImGui::SliderFloat("##timeline_fps", &m_timelineState.fps, 1.0f, 60.0f, "%.0f"))
    {
        project->setTimelineFps(static_cast<int>(m_timelineState.fps + 0.5f));
        m_timelineState.fps = static_cast<float>(project->getTimelineFps());
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

    if (m_timelineState.isPlaying && m_timelineState.fps > 0.0f && frameCount > 0)
    {
        const double frameDuration = 1.0 / static_cast<double>(m_timelineState.fps);
        while (m_timelineState.accumulator >= frameDuration)
        {
            m_timelineState.accumulator -= frameDuration;
            const std::vector<int> playbackFrames = buildPlaybackFramesForCurrentFrame(
                frameGroups,
                context->getCurrentFrameIndex(),
                frameCount);
            bool reachedEnd = false;
            const int next = findNextPlaybackFrame(
                playbackFrames,
                context->getCurrentFrameIndex(),
                m_timelineState.loopEnabled,
                reachedEnd);
            if (reachedEnd && !m_timelineState.loopEnabled)
            {
                m_timelineState.isPlaying = false;
                m_timelineState.accumulator = 0.0;
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
        // 帧号按单元格宽度手动居中绘制，避免 1 位数/2 位数宽度不同导致视觉偏移。
        const ImVec2 cellPos = ImGui::GetCursorScreenPos();
        const std::string label = std::to_string(i + 1);
        const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(
                cellPos.x + (cellW - textSize.x) * 0.5f,
                cellPos.y + (headerH - textSize.y) * 0.5f),
            ImGui::GetColorU32(ImGuiCol_Text),
            label.c_str());
        ImGui::Dummy(ImVec2(cellW, headerH));
        if (i + 1 < frameCount) ImGui::SameLine();
    }

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
            m_timelineState.draggingFrameIndex = i;
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
                    m_timelineState.renameGroupIndex = groupIndex;
                    std::snprintf(m_timelineState.renameGroupName,
                                  sizeof(m_timelineState.renameGroupName),
                                  "%s",
                                  hitGroup.name.c_str());
                    m_timelineState.openRenameGroupPopup = true;
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
                    m_timelineState.pendingGroupFrames = selectedFrames;
                    std::snprintf(m_timelineState.pendingGroupName,
                                  sizeof(m_timelineState.pendingGroupName),
                                  "Group %d",
                                  static_cast<int>(frameGroups.size() + 1));
                    m_timelineState.openCreateGroupNamePopup = true;
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
    if (m_timelineState.openCreateGroupNamePopup)
    {
        ImGui::OpenPopup("Create Frame Group");
        m_timelineState.openCreateGroupNamePopup = false;
    }

    if (m_timelineState.openRenameGroupPopup)
    {
        ImGui::OpenPopup("Rename Frame Group");
        m_timelineState.openRenameGroupPopup = false;
    }

    if (ImGui::BeginPopupModal("Create Frame Group", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Group Name");
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputText("##group_name", m_timelineState.pendingGroupName, sizeof(m_timelineState.pendingGroupName));

        ImGui::Separator();
        ImGui::Text("Frames: %d selected", static_cast<int>(m_timelineState.pendingGroupFrames.size()));

        if (ImGui::Button("Create", ImVec2(120.0f, 0.0f)))
        {
            const uint32_t color = pickGroupColorByIndex(frameGroups.size());
            context->addFrameGroup(m_timelineState.pendingGroupName,
                                   m_timelineState.pendingGroupFrames,
                                   frameCount,
                                   color);
            context->setProjectDirty(true, "Create Group");
            m_timelineState.pendingGroupFrames.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
        {
            m_timelineState.pendingGroupFrames.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Rename Frame Group", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("New Group Name");
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputText("##rename_group_name",
                         m_timelineState.renameGroupName,
                         sizeof(m_timelineState.renameGroupName));

        if (ImGui::Button("Apply", ImVec2(120.0f, 0.0f)))
        {
            context->renameFrameGroup(m_timelineState.renameGroupIndex, m_timelineState.renameGroupName);
            context->setProjectDirty(true, "Rename Group");
            m_timelineState.renameGroupIndex = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
        {
            m_timelineState.renameGroupIndex = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::EndChild();
    ImGui::EndChild();
}

