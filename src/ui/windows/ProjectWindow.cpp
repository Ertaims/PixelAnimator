#include "ProjectWindow.h"

#include "core/AppContext.h"
#include "core/Project.h"
#include "imgui.h"
#include "render/Texture.h"

#include <algorithm>
#include <vector>

ProjectWindow::~ProjectWindow()
{
    m_canvasTexture.release();
    render::deleteTexture(m_timelineState.playIconTexture);
    render::deleteTexture(m_timelineState.pauseIconTexture);
    render::deleteTexture(m_toolbarState.brushIconTexture);
    render::deleteTexture(m_toolbarState.eraserIconTexture);
    render::deleteTexture(m_toolbarState.eyedropperIconTexture);
    render::deleteTexture(m_toolbarState.fillIconTexture);
    render::deleteTexture(m_toolbarState.rectSelectIconTexture);
    render::deleteTexture(m_toolbarState.circleSelectIconTexture);
    render::deleteTexture(m_toolbarState.magicWandSelectIconTexture);
    render::deleteTexture(m_toolbarState.lassoSelectIconTexture);
    render::deleteTexture(m_toolbarState.polygonLassoSelectIconTexture);
    render::deleteTexture(m_toolbarState.lineIconTexture);
    render::deleteTexture(m_toolbarState.curveIconTexture);
    render::deleteTexture(m_toolbarState.rectIconTexture);
    render::deleteTexture(m_toolbarState.rectFilledIconTexture);
    render::deleteTexture(m_toolbarState.circleIconTexture);
    render::deleteTexture(m_toolbarState.circleFilledIconTexture);
    render::deleteTexture(m_toolbarState.symmetryLeftRightIconTexture);
    render::deleteTexture(m_toolbarState.symmetryUpDownIconTexture);
    m_layerPanel.releaseTextures();
}

void ProjectWindow::beginPastePreview(const commands::PixelClipboardData& clipboard)
{
    if (!clipboard.isValid())
    {
        m_pastePreviewState.active = false;
        m_pastePreviewState.clipboard.clear();
        m_pastePreviewState.originX = 0;
        m_pastePreviewState.originY = 0;
        return;
    }

    m_pastePreviewState.active = true;
    m_pastePreviewState.clipboard = clipboard;
    m_pastePreviewState.originX = 0;
    m_pastePreviewState.originY = 0;
}

bool ProjectWindow::isPastePreviewActive() const
{
    return m_pastePreviewState.active;
}

void ProjectWindow::cancelPastePreview()
{
    m_pastePreviewState.active = false;
    m_pastePreviewState.clipboard.clear();
    m_pastePreviewState.originX = 0;
    m_pastePreviewState.originY = 0;
}

// 确保画布纹理存在并匹配当前画布尺寸。
void ProjectWindow::ensureCanvasTexture(int width, int height)
{
    m_canvasTexture.ensureSize(width, height);
}

// 上传合成后的画布像素，供 ImGui 画布区域显示。
void ProjectWindow::uploadCanvasPixels(const std::vector<uint32_t>& pixels) const
{
    m_canvasTexture.uploadPixels(pixels);
}

/**
 * @brief 渲染项目窗口的内容。
 *
 * 该函数负责渲染整个项目窗口，包括顶部区域、时间轴区域以及各个子面板。
 * 它会根据窗口的可见性、焦点状态以及项目加载情况来决定是否渲染内容。
 * 
 * 主要功能包括：
 * - 检查窗口是否可见，若不可见则直接返回。
 * - 根据窗口标签和名称初始化ImGui窗口。
 * - 处理窗口焦点事件并调用相应的回调函数。
 * - 渲染顶部区域（包含左侧面板、工具栏、画布和右侧面板）。
 * - 渲染可调整大小的时间轴区域。
 * - 使用ImGui表格布局管理各个子面板的排列。
 */
void ProjectWindow::render()
{
    // 如果窗口不可见，则直接返回
    if (!visible) return;

    // 获取窗口标签，如果未设置则使用默认名称
    const char* label = m_windowLabel.empty() ? name : m_windowLabel.c_str();
    
    // 尝试开始ImGui窗口，如果失败则结束并返回
    if (!ImGui::Begin(label, &visible))
    {
        ImGui::End();
        return;
    }

    // 如果窗口获得焦点且存在焦点回调函数，则调用回调函数
    if (m_onFocused && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
    {
        m_onFocused(context);
    }

    // 如果上下文为空或没有加载项目，则显示提示信息并结束渲染
    if (!context || !context->hasProject())
    {
        ImGui::TextUnformatted("No project loaded.");
        ImGui::End();
        return;
    }

    // 获取当前项目指针
    Project* project = context->getProject();

    // 计算分割器高度、最小顶部区域高度、最小时间轴高度等布局参数
    const float splitterHeight = 2.0f;
    const float minTopHeight = 120.0f;
    const float minTimelineHeight = 80.0f;
    const float availableHeight = ImGui::GetContentRegionAvail().y;
    const float maxTimelineHeight = std::max(minTimelineHeight, availableHeight - minTopHeight - splitterHeight);
    m_timelineState.height = std::clamp(m_timelineState.height, minTimelineHeight, maxTimelineHeight);
    const float topHeight = std::max(minTopHeight, availableHeight - m_timelineState.height - splitterHeight);

    // 开始渲染顶部区域的子窗口
    if (ImGui::BeginChild("##ProjectTopRegion", ImVec2(0.0f, topHeight), false))
    {
        // 使用表格布局管理顶部区域的四个列（左侧面板、工具栏、画布、右侧面板）
        if (ImGui::BeginTable("##ProjectMainColumns", 4, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
        {
            // 设置各列的宽度和属性
            ImGui::TableSetupColumn("Left", ImGuiTableColumnFlags_WidthStretch, 0.90f);
            ImGui::TableSetupColumn(
                "Tools",
                ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize,
                50.0f);
            ImGui::TableSetupColumn("Center", ImGuiTableColumnFlags_WidthStretch, 1.75f);
            ImGui::TableSetupColumn("Right", ImGuiTableColumnFlags_WidthStretch, 0.90f);
            ImGui::TableNextRow();

            // 渲染左侧面板
            ImGui::TableSetColumnIndex(0);
            if (ImGui::BeginChild("##LeftPanel", ImVec2(0.0f, 0.0f), true))
            {
                renderLeftPanel(project);
            }
            ImGui::EndChild();

            // 渲染工具栏面板
            ImGui::TableSetColumnIndex(1);
            if (ImGui::BeginChild("##ToolBarPanel", ImVec2(0.0f, 0.0f), true))
            {
                renderToolbarPanel();
            }
            ImGui::EndChild();

            // 渲染画布面板
            ImGui::TableSetColumnIndex(2);
            if (ImGui::BeginChild("##CanvasPanel", ImVec2(0.0f, 0.0f), true))
            {
                renderCanvasPanel(project);
            }
            ImGui::EndChild();

            // 渲染右侧面板
            ImGui::TableSetColumnIndex(3);
            if (ImGui::BeginChild("##ToolPropsPanel", ImVec2(0.0f, 0.0f), true))
            {
                renderRightPanel(project);
            }
            ImGui::EndChild();

            // 结束表格布局
            ImGui::EndTable();
        }
    }
    // 结束顶部区域的子窗口
    ImGui::EndChild();

    // 设置光标位置以绘制时间轴分割器
    ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y));
    ImGui::InvisibleButton("##TimelineSplitter", ImVec2(-1.0f, splitterHeight));

    // 处理时间轴分割器的拖拽操作，调整时间轴高度
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        const float deltaY = ImGui::GetIO().MouseDelta.y;
        m_timelineState.height = std::clamp(m_timelineState.height - deltaY, minTimelineHeight, maxTimelineHeight);
    }

    // 当鼠标悬停在分割器上时，更改光标样式为垂直调整大小
    if (ImGui::IsItemHovered() || ImGui::IsItemActive())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }

    // 开始渲染时间轴面板的子窗口
    if (ImGui::BeginChild("##TimelinePanel", ImVec2(0.0f, 0.0f), true))
    {
        renderTimelinePanel(project);
    }
    // 结束时间轴面板的子窗口
    ImGui::EndChild();

    // 结束ImGui窗口
    ImGui::End();
}


