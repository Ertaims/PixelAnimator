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

/**
 * @brief 确保画布纹理已创建并具有指定的尺寸。
 * 
 * 该函数用于初始化或更新画布纹理。如果纹理尚未创建，则会生成一个新的纹理对象并设置其参数；
 * 如果纹理已存在但尺寸与指定的宽度和高度不匹配，则会重新分配纹理内存以适应新的尺寸。
 * 
 * @param width 纹理的宽度（像素）。
 * @param height 纹理的高度（像素）。
 */
void ProjectWindow::ensureCanvasTexture(int width, int height)
{
    // 如果纹理未创建，则生成一个新的纹理对象并设置基本参数
    if (canvasTexture_.texture == 0)
    {
        glGenTextures(1, &canvasTexture_.texture);
        glBindTexture(GL_TEXTURE_2D, canvasTexture_.texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    // 如果纹理尺寸与当前指定的尺寸不一致，则重新分配纹理内存
    if (width != canvasTexture_.width || height != canvasTexture_.height)
    {
        canvasTexture_.width = width;
        canvasTexture_.height = height;
        glBindTexture(GL_TEXTURE_2D, canvasTexture_.texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }
}

/**
 * @brief 将像素数据上传到画布纹理中
 * 
 * 该函数将给定的像素数据上传到OpenGL纹理对象中，用于更新画布的显示内容。
 * 像素数据以RGBA格式存储，每个像素占用4个字节（uint32_t）。
 * 
 * @param pixels 包含RGBA像素数据的向量，数据按行优先顺序排列
 */
void ProjectWindow::uploadCanvasPixels(const std::vector<uint32_t>& pixels) const
{
    // 绑定画布纹理对象，后续操作将针对此纹理进行
    glBindTexture(GL_TEXTURE_2D, canvasTexture_.texture);

    // 设置像素存储模式，确保数据按1字节对齐，避免因对齐问题导致的数据错误
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // 将像素数据上传到纹理中，替换纹理的全部内容
    // 参数说明：
    // - GL_TEXTURE_2D: 指定目标纹理类型为2D纹理
    // - 0: 指定纹理的mipmap级别为0（基础级别）
    // - 0, 0: 指定纹理更新的起始坐标为(0, 0)
    // - canvasTexture_.width, canvasTexture_.height: 指定更新区域的宽度和高度
    // - GL_RGBA: 指定像素数据的格式为RGBA
    // - GL_UNSIGNED_BYTE: 指定每个颜色分量的数据类型为无符号字节
    // - pixels.data(): 指向像素数据的指针
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
    if (!visible)
        return;

    // 获取窗口标签，如果未设置则使用默认名称
    const char* label = windowLabel_.empty() ? name : windowLabel_.c_str();
    
    // 尝试开始ImGui窗口，如果失败则结束并返回
    if (!ImGui::Begin(label, &visible))
    {
        ImGui::End();
        return;
    }

    // 如果窗口获得焦点且存在焦点回调函数，则调用回调函数
    if (onFocused_ && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
    {
        onFocused_(context);
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
    timelineState_.height = std::clamp(timelineState_.height, minTimelineHeight, maxTimelineHeight);
    const float topHeight = std::max(minTopHeight, availableHeight - timelineState_.height - splitterHeight);

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
        timelineState_.height = std::clamp(timelineState_.height - deltaY, minTimelineHeight, maxTimelineHeight);
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
