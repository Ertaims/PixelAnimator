#include "ProjectWindow.h"

#include "core/AppContext.h"
#include "core/Project.h"
#include "imgui.h"
#include <SDL3/SDL_opengl.h>

#include <algorithm>
#include <cstdint>
#include <string>

namespace
{
// -----------------------------------------------------------------------------
// 颜色工具（RGBA8888 <-> ImGui float4）
// -----------------------------------------------------------------------------
ImVec4 rgbaToFloat4(uint32_t rgba)
{
    const float r = static_cast<float>((rgba >> 0) & 0xFF) / 255.0f;
    const float g = static_cast<float>((rgba >> 8) & 0xFF) / 255.0f;
    const float b = static_cast<float>((rgba >> 16) & 0xFF) / 255.0f;
    const float a = static_cast<float>((rgba >> 24) & 0xFF) / 255.0f;
    return ImVec4(r, g, b, a);
}

uint32_t float4ToRgba(const ImVec4& color)
{
    const uint32_t r = static_cast<uint32_t>(color.x * 255.0f + 0.5f) & 0xFF;
    const uint32_t g = static_cast<uint32_t>(color.y * 255.0f + 0.5f) & 0xFF;
    const uint32_t b = static_cast<uint32_t>(color.z * 255.0f + 0.5f) & 0xFF;
    const uint32_t a = static_cast<uint32_t>(color.w * 255.0f + 0.5f) & 0xFF;
    return (r << 0) | (g << 8) | (b << 16) | (a << 24);
}

// -----------------------------------------------------------------------------
// 画布预览用的最小 OpenGL 纹理缓存
// -----------------------------------------------------------------------------
GLuint g_canvasTexture = 0;
int g_texWidth = 0;
int g_texHeight = 0;

void ensureCanvasTexture(int width, int height)
{
    if (g_canvasTexture == 0)
    {
        glGenTextures(1, &g_canvasTexture);
        glBindTexture(GL_TEXTURE_2D, g_canvasTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    if (width != g_texWidth || height != g_texHeight)
    {
        // 当画布尺寸变化时分配/重分配纹理存储。
        g_texWidth = width;
        g_texHeight = height;
        glBindTexture(GL_TEXTURE_2D, g_canvasTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, g_texWidth, g_texHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }
}

void uploadCanvasPixels(const std::vector<uint32_t>& pixels)
{
    // 像素缓冲是 RGBA8888（R 在低字节），因此 GL_RGBA/GL_UNSIGNED_BYTE 对应正确。
    glBindTexture(GL_TEXTURE_2D, g_canvasTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, g_texWidth, g_texHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
}

// -----------------------------------------------------------------------------
// 基础绘制辅助（单帧，暂不含撤销/重做）
// -----------------------------------------------------------------------------
void paintAt(Project::Frame& frame, int canvasW, int canvasH, int x, int y, int brushSize, uint32_t color)
{
    // 将 brushSize 视为半径概念：size=1 时只画一个像素。
    const int radius = std::max(0, brushSize - 1);
    const int minX = std::max(0, x - radius);
    const int maxX = std::min(canvasW - 1, x + radius);
    const int minY = std::max(0, y - radius);
    const int maxY = std::min(canvasH - 1, y + radius);

    for (int py = minY; py <= maxY; ++py)
    {
        const size_t row = static_cast<size_t>(py) * static_cast<size_t>(canvasW);
        for (int px = minX; px <= maxX; ++px)
        {
            frame.pixels[row + static_cast<size_t>(px)] = color;
        }
    }
}
} // namespace

void ProjectWindow::render()
{
    if (!visible)
        return;

    if (!ImGui::Begin(name, &visible))
    {
        ImGui::End();
        return;
    }

    if (!context || !context->hasProject())
    {
        ImGui::TextUnformatted("No project loaded.");
        ImGui::End();
        return;
    }

    Project* project = context->getProject();

    const float timelineHeight = 140.0f;
    const float availableHeight = ImGui::GetContentRegionAvail().y;
    const float topHeight = std::max(120.0f, availableHeight - timelineHeight - 8.0f);

    if (ImGui::BeginChild("##ProjectTopRegion", ImVec2(0.0f, topHeight), false))
    {
        if (ImGui::BeginTable("##ProjectMainColumns", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Left", ImGuiTableColumnFlags_WidthStretch, 0.95f);
            ImGui::TableSetupColumn("Center", ImGuiTableColumnFlags_WidthStretch, 1.8f);
            ImGui::TableSetupColumn("Right", ImGuiTableColumnFlags_WidthStretch, 0.95f);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            if (ImGui::BeginChild("##LeftPanel", ImVec2(0.0f, 0.0f), true))
            {
                renderLeftPanel(project);
            }
            ImGui::EndChild();

            ImGui::TableSetColumnIndex(1);
            if (ImGui::BeginChild("##CanvasPanel", ImVec2(0.0f, 0.0f), true))
            {
                renderCanvasPanel(project);
            }
            ImGui::EndChild();

            ImGui::TableSetColumnIndex(2);
            if (ImGui::BeginChild("##ToolPropsPanel", ImVec2(0.0f, 0.0f), true))
            {
                renderRightPanel(project);
            }
            ImGui::EndChild();

            ImGui::EndTable();
        }
    }
    ImGui::EndChild();

    if (ImGui::BeginChild("##TimelinePanel", ImVec2(0.0f, 0.0f), true))
    {
        renderTimelinePanel(project);
    }
    ImGui::EndChild();

    ImGui::End();
}

void ProjectWindow::renderLeftPanel(Project* project)
{
    (void)project;
    // Palette 分为系统默认与用户自定义两部分。
    static const uint32_t kDefaultPalette[] = {
        0xFF000000, 0xFFFFFFFF, 0xFF404040, 0xFFC0C0C0,
        0xFF0000FF, 0xFF00FF00, 0xFFFF0000, 0xFF00FFFF,
        0xFFFF00FF, 0xFFFFFF00, 0xFF804000, 0xFFFFA500,
        0xFF8B4513, 0xFF800080, 0xFF008080, 0xFF1E90FF
    };
    static std::vector<uint32_t> userPalette;
    static int selectedIndex = 0;
    static bool selectedIsUser = false;

    const int defaultCount = static_cast<int>(sizeof(kDefaultPalette) / sizeof(kDefaultPalette[0]));
    const int userCount = static_cast<int>(userPalette.size());

    if (!selectedIsUser)
    {
        if (selectedIndex < 0 || selectedIndex >= defaultCount)
            selectedIndex = 0;
    }
    else
    {
        if (userCount == 0)
        {
            selectedIsUser = false;
            selectedIndex = 0;
        }
        else if (selectedIndex < 0 || selectedIndex >= userCount)
        {
            selectedIndex = 0;
        }
    }

    const uint32_t selectedColor = selectedIsUser
        ? userPalette[static_cast<size_t>(selectedIndex)]
        : kDefaultPalette[selectedIndex];

    ImGui::TextUnformatted("Color Picker");
    ImVec4 color = rgbaToFloat4(selectedColor);
    if (ImGui::ColorPicker4("##ProjectColorPicker", &color.x, ImGuiColorEditFlags_AlphaBar))
    {
        const uint32_t newColor = float4ToRgba(color);
        // 系统默认色不改动，仅更新当前前景色。
        if (selectedIsUser && !userPalette.empty())
        {
            userPalette[static_cast<size_t>(selectedIndex)] = newColor;
        }
        context->setColorRGBA(newColor);
        context->setProjectDirty(true);
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Palette - Default");
    for (int i = 0; i < defaultCount; ++i)
    {
        ImGui::PushID(i);
        if (ImGui::ColorButton("##palette_default", rgbaToFloat4(kDefaultPalette[i]), ImGuiColorEditFlags_NoTooltip, ImVec2(24.0f, 24.0f)))
        {
            selectedIsUser = false;
            selectedIndex = i;
            context->setColorRGBA(kDefaultPalette[i]);
        }
        ImGui::PopID();
        if ((i + 1) % 4 != 0)
            ImGui::SameLine();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Palette - User");
    if (userPalette.empty())
    {
        ImGui::TextUnformatted("No user colors yet.");
    }
    else
    {
        for (int i = 0; i < static_cast<int>(userPalette.size()); ++i)
        {
            ImGui::PushID(i);
            if (ImGui::ColorButton("##palette_user", rgbaToFloat4(userPalette[static_cast<size_t>(i)]), ImGuiColorEditFlags_NoTooltip, ImVec2(24.0f, 24.0f)))
            {
                selectedIsUser = true;
                selectedIndex = i;
                context->setColorRGBA(userPalette[static_cast<size_t>(i)]);
            }
            ImGui::PopID();
            if ((i + 1) % 4 != 0)
                ImGui::SameLine();
        }
    }

    // 添加按钮：新增一个颜色并选中（默认使用当前前景色）。
    ImGui::Separator();
    if (ImGui::Button("+ Add Color"))
    {
        const uint32_t newColor = context->getColorRGBA();
        userPalette.push_back(newColor);
        selectedIsUser = true;
        selectedIndex = static_cast<int>(userPalette.size()) - 1;
        context->setProjectDirty(true);
    }
}

void ProjectWindow::renderCanvasPanel(Project* project)
{
    const int width = project->getWidth();
    const int height = project->getHeight();
    int zoom = context->getCanvasZoom();
    const int frameIndex = context->getCurrentFrameIndex();
    const int frameCount = project->getFrameCount();
    ImGui::Text("Canvas  %dx%d   Zoom %dx   Frame %d/%d",
        width, height, zoom, frameIndex + 1, frameCount);
    ImGui::Separator();

    // 1) 把当前帧像素上传到 OpenGL 纹理。
    Project::Frame& frame = project->getFrame(frameIndex);
    ensureCanvasTexture(width, height);
    uploadCanvasPixels(frame.pixels);

    // 2) 计算画布在面板中的位置：居中 + 平移（pan）。
    const ImVec2 panelPos = ImGui::GetCursorScreenPos();
    const ImVec2 panelAvail = ImGui::GetContentRegionAvail();
    const float imageW = static_cast<float>(width * zoom);
    const float imageH = static_cast<float>(height * zoom);
    const ImVec2 centerOffset(
        (panelAvail.x - imageW) * 0.5f,
        (panelAvail.y - imageH) * 0.5f);
    const float panX = context->getCanvasPanX();
    const float panY = context->getCanvasPanY();
    const ImVec2 imagePos(
        panelPos.x + centerOffset.x + panX,
        panelPos.y + centerOffset.y + panY);

    // 用一个覆盖面板的透明按钮接收鼠标交互。
    ImGui::InvisibleButton("##CanvasHitbox", panelAvail,
                           ImGuiButtonFlags_MouseButtonLeft |
                           ImGuiButtonFlags_MouseButtonMiddle |
                           ImGuiButtonFlags_MouseButtonRight);

    // 处理滚轮缩放（鼠标悬停时）。
    if (ImGui::IsItemHovered())
    {
        const float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f)
        {
            const int zoomLevels[] = {1, 2, 4, 8, 16, 32};
            int zoomIndex = 0;
            for (int i = 0; i < 6; ++i)
            {
                if (zoomLevels[i] == zoom)
                {
                    zoomIndex = i;
                    break;
                }
            }
            zoomIndex = std::clamp(zoomIndex + (wheel > 0.0f ? 1 : -1), 0, 5);
            zoom = zoomLevels[zoomIndex];
            context->setCanvasZoom(zoom);
        }
    }

    // 中键拖拽平移。
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
    {
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        context->addCanvasPan(delta.x, delta.y);
    }

    // 绘制画布图像（OpenGL 纹理 id 通过 ImTextureID 传入）。
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 imageMin = imagePos;
    const ImVec2 imageMax = ImVec2(imagePos.x + imageW, imagePos.y + imageH);
    // 绘制棋盘背景（四格：2x2）。
    {
        const ImU32 c1 = IM_COL32(70, 70, 70, 255);
        const ImU32 c2 = IM_COL32(90, 90, 90, 255);
        const float tileW = imageW * 0.5f;
        const float tileH = imageH * 0.5f;
        for (int ty = 0; ty < 2; ++ty)
        {
            for (int tx = 0; tx < 2; ++tx)
            {
                const ImU32 col = ((tx + ty) % 2 == 0) ? c1 : c2;
                const ImVec2 p0(imageMin.x + tx * tileW, imageMin.y + ty * tileH);
                const ImVec2 p1(p0.x + tileW, p0.y + tileH);
                drawList->AddRectFilled(p0, p1, col);
            }
        }
    }
    // 绘制画布图像（OpenGL 纹理 id 通过 ImTextureID 传入）。
    drawList->AddImage(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(g_canvasTexture)),
                       imageMin, imageMax,
                       ImVec2(0, 0), ImVec2(1, 1));
    drawList->AddRect(imageMin, imageMax, IM_COL32(180, 180, 180, 255));

    // 4) 可选网格显示（按像素格）。
    if (context->isGridVisible() && zoom >= 4)
    {
        const ImU32 gridColor = IM_COL32(80, 80, 80, 120);
        for (int x = 1; x < width; ++x)
        {
            const float gx = imagePos.x + x * zoom;
            drawList->AddLine(ImVec2(gx, imagePos.y), ImVec2(gx, imagePos.y + imageH), gridColor);
        }
        for (int y = 1; y < height; ++y)
        {
            const float gy = imagePos.y + y * zoom;
            drawList->AddLine(ImVec2(imagePos.x, gy), ImVec2(imagePos.x + imageW, gy), gridColor);
        }
    }

    // 5) 在图像范围内处理鼠标拖动绘制。
    const ImVec2 mousePos = ImGui::GetMousePos();
    const bool hovered =
        mousePos.x >= imagePos.x &&
        mousePos.y >= imagePos.y &&
        mousePos.x < (imagePos.x + imageW) &&
        mousePos.y < (imagePos.y + imageH);

    if (hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        // 把屏幕坐标转换为画布像素坐标。
        const float localX = mousePos.x - imagePos.x;
        const float localY = mousePos.y - imagePos.y;
        const int pixelX = std::clamp(static_cast<int>(localX / zoom), 0, width - 1);
        const int pixelY = std::clamp(static_cast<int>(localY / zoom), 0, height - 1);

        // 应用工具：目前最小实现仅支持画笔/橡皮。
        uint32_t paintColor = context->getColorRGBA();
        if (context->getTool() == ToolType::Eraser)
            paintColor = 0x00000000;

        paintAt(frame, width, height, pixelX, pixelY, context->getBrushSize(), paintColor);
        context->setProjectDirty(true);
    }

    // 6) 选中像素高亮（鼠标悬停时高亮当前像素）。
    if (hovered)
    {
        const float localX = mousePos.x - imagePos.x;
        const float localY = mousePos.y - imagePos.y;
        const int pixelX = std::clamp(static_cast<int>(localX / zoom), 0, width - 1);
        const int pixelY = std::clamp(static_cast<int>(localY / zoom), 0, height - 1);
        const ImVec2 hlMin(imagePos.x + pixelX * zoom, imagePos.y + pixelY * zoom);
        const ImVec2 hlMax(hlMin.x + zoom, hlMin.y + zoom);
        drawList->AddRect(hlMin, hlMax, IM_COL32(255, 255, 0, 200));
    }
}

void ProjectWindow::renderRightPanel(Project* project)
{
    ImGui::TextUnformatted("Tool Properties");
    const char* toolNames[] = {"Brush", "Eraser", "Eyedropper", "Fill", "Line", "Rect", "RectFilled"};
    int toolIndex = static_cast<int>(context->getTool());
    if (ImGui::Combo("Tool", &toolIndex, toolNames, static_cast<int>(ToolType::Count)))
    {
        context->setTool(static_cast<ToolType>(toolIndex));
    }

    int brushSize = context->getBrushSize();
    if (ImGui::SliderInt("Brush Size", &brushSize, 1, 32))
    {
        context->setBrushSize(brushSize);
    }

    const int zoomLevels[] = {1, 2, 4, 8, 16, 32};
    const char* zoomLabels[] = {"1x", "2x", "4x", "8x", "16x", "32x"};
    int zoomIndex = 0;
    for (int i = 0; i < 6; ++i)
    {
        if (zoomLevels[i] == context->getCanvasZoom())
        {
            zoomIndex = i;
            break;
        }
    }
    if (ImGui::Combo("Canvas Zoom", &zoomIndex, zoomLabels, 6))
    {
        context->setCanvasZoom(zoomLevels[zoomIndex]);
    }

    bool showGrid = context->isGridVisible();
    if (ImGui::Checkbox("Show Grid", &showGrid))
        context->setGridVisible(showGrid);

    bool onionSkin = context->isOnionSkinEnabled();
    if (ImGui::Checkbox("Onion Skin", &onionSkin))
        context->setOnionSkinEnabled(onionSkin);

    ImGui::Separator();
    ImGui::TextUnformatted("Project");
    ImGui::Text("Name: %s", project->getName().c_str());
    ImGui::Text("Size: %dx%d", project->getWidth(), project->getHeight());
    ImGui::Text("Frames: %d", project->getFrameCount());
    ImGui::Text("Total Pixels: %d", project->getWidth() * project->getHeight());

    static int newWidth = 128;
    static int newHeight = 128;
    if (newWidth <= 0 || newHeight <= 0)
    {
        newWidth = project->getWidth();
        newHeight = project->getHeight();
    }

    ImGui::InputInt("Width", &newWidth);
    ImGui::InputInt("Height", &newHeight);
    if (ImGui::Button("Apply Size") && newWidth > 0 && newHeight > 0)
    {
        project->resizeCanvas(newWidth, newHeight, 0x00000000);
        context->setProjectDirty(true);
    }
}

void ProjectWindow::renderTimelinePanel(Project* project)
{
    ImGui::TextUnformatted("Timeline");
    const int frameCount = project->getFrameCount();
    int current = context->getCurrentFrameIndex();
    current = std::clamp(current, 0, std::max(0, frameCount - 1));
    context->setCurrentFrameIndex(current);

    if (ImGui::Button("+ Frame"))
    {
        project->setFrameCount(frameCount + 1, 0x00000000);
        context->setCurrentFrameIndex(frameCount);
        context->setProjectDirty(true);
    }
    ImGui::SameLine();
    if (ImGui::Button("- Frame") && frameCount > 1)
    {
        project->setFrameCount(frameCount - 1);
        context->setCurrentFrameIndex(std::min(current, frameCount - 2));
        context->setProjectDirty(true);
    }
    ImGui::Separator();

    if (ImGui::BeginChild("##FrameStrip", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar))
    {
        for (int i = 0; i < project->getFrameCount(); ++i)
        {
            ImGui::PushID(i);
            const std::string label = "F" + std::to_string(i + 1);
            const bool selected = (i == context->getCurrentFrameIndex());
            if (ImGui::Selectable(label.c_str(), selected, 0, ImVec2(56.0f, 28.0f)))
            {
                context->setCurrentFrameIndex(i);
            }
            ImGui::PopID();
            ImGui::SameLine();
        }
    }
    ImGui::EndChild();
}
