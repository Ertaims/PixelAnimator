#include "ProjectWindow.h"

#include "core/AppContext.h"
#include "core/Project.h"
#include "imgui.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace
{
    // -----------------------------------------------------------------------------
    // 颜色工具（RGBA8888 <-> ImGui float4）
    // -----------------------------------------------------------------------------
    // RGBA8888 -> ImGui float4 (0~1)
    ImVec4 rgbaToFloat4(uint32_t rgba)
    {
        const float r = static_cast<float>((rgba >> 0) & 0xFF) / 255.0f;
        const float g = static_cast<float>((rgba >> 8) & 0xFF) / 255.0f;
        const float b = static_cast<float>((rgba >> 16) & 0xFF) / 255.0f;
        const float a = static_cast<float>((rgba >> 24) & 0xFF) / 255.0f;
        return ImVec4(r, g, b, a);
    }

    // ImGui float4 (0~1) -> RGBA8888
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

    // 确保 OpenGL 纹理存在且尺寸与画布一致
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

    // 把 CPU 侧像素上传到 GPU 纹理
    void uploadCanvasPixels(const std::vector<uint32_t>& pixels)
    {
        // 像素缓冲是 RGBA8888（R 在低字节），因此 GL_RGBA/GL_UNSIGNED_BYTE 对应正确。
        glBindTexture(GL_TEXTURE_2D, g_canvasTexture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, g_texWidth, g_texHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    }

    // 在给定像素坐标画一个“方形笔刷”（当前最简单实现）
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

    // 从图片文件加载 OpenGL 纹理（用于 Timeline 图标按钮）。
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
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGBA8,
                     rgbaSurface->w,
                     rgbaSurface->h,
                     0,
                     GL_RGBA,
                     GL_UNSIGNED_BYTE,
                     rgbaSurface->pixels);

        SDL_DestroySurface(rgbaSurface);
        return texture;
    }

    void ensureTimelineIconTextures(GLuint& playIcon, GLuint& pauseIcon, bool& loaded)
    {
        if (loaded)
            return;

        const char* playCandidates[] = {
            "src/assets/start.png",
            "../src/assets/start.png",
            "../../src/assets/start.png"
        };
        const char* pauseCandidates[] = {
            "src/assets/pause.png",
            "../src/assets/pause.png",
            "../../src/assets/pause.png"
        };

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

// 主窗口渲染入口：负责组织四个子区域（左/中/右/时间线）
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

    // 取得当前项目实例，后续所有面板都基于它渲染
    Project* project = context->getProject();

    // 顶部三列 + 底部时间线，时间线高度可拖拽调整
    static float timelineHeight = 140.0f;
    const float splitterHeight = 2.0f;
    const float minTopHeight = 120.0f;
    const float minTimelineHeight = 80.0f;
    const float availableHeight = ImGui::GetContentRegionAvail().y;
    const float maxTimelineHeight = std::max(minTimelineHeight, availableHeight - minTopHeight - splitterHeight);
    timelineHeight = std::clamp(timelineHeight, minTimelineHeight, maxTimelineHeight);
    const float topHeight = std::max(minTopHeight, availableHeight - timelineHeight - splitterHeight);

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

    // 时间线高度拖拽分割条
    ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y));
    ImGui::InvisibleButton("##TimelineSplitter", ImVec2(-1.0f, splitterHeight));
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        const float deltaY = ImGui::GetIO().MouseDelta.y;
        timelineHeight = std::clamp(timelineHeight - deltaY, minTimelineHeight, maxTimelineHeight);
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

// 左侧区域：颜色选择器 + 调色板
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
        const bool isSelected = (!selectedIsUser && selectedIndex == i);
        ImGui::PushID(i);
        if (ImGui::ColorButton("##palette_default", rgbaToFloat4(kDefaultPalette[i]), ImGuiColorEditFlags_NoTooltip, ImVec2(24.0f, 24.0f)))
        {
            selectedIsUser = false;
            selectedIndex = i;
            context->setColorRGBA(kDefaultPalette[i]);
        }
        if (isSelected)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRect(
                ImGui::GetItemRectMin(),
                ImGui::GetItemRectMax(),
                IM_COL32(255, 220, 40, 255),
                2.0f,
                0,
                2.0f);
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
            const bool isSelected = (selectedIsUser && selectedIndex == i);
            ImGui::PushID(i);
            if (ImGui::ColorButton("##palette_user", rgbaToFloat4(userPalette[static_cast<size_t>(i)]), ImGuiColorEditFlags_NoTooltip, ImVec2(24.0f, 24.0f)))
            {
                selectedIsUser = true;
                selectedIndex = i;
                context->setColorRGBA(userPalette[static_cast<size_t>(i)]);
            }
            if (isSelected)
            {
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                drawList->AddRect(
                    ImGui::GetItemRectMin(),
                    ImGui::GetItemRectMax(),
                    IM_COL32(255, 220, 40, 255),
                    2.0f,
                    0,
                    2.0f);
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

// 中间区域：画布显示与绘制交互
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

    // 1) 把当前帧像素上传到 OpenGL 纹理（CPU -> GPU）。
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

    // 用一个覆盖面板的透明按钮接收鼠标交互（滚轮缩放/中键拖拽/左键绘制）。
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

    // 中键拖拽平移（修改 AppContext 的 pan 偏移）。
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
    {
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        context->addCanvasPan(delta.x, delta.y);
    }

    // 绘制画布图像（OpenGL 纹理 id 通过 ImTextureID 传入）。
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 imageMin = imagePos;
    const ImVec2 imageMax = ImVec2(imagePos.x + imageW, imagePos.y + imageH);
    // 绘制棋盘背景（四格：2x2），作为透明像素的对比底色。
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

    // 可选网格显示（按像素格）。
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

    // 在图像范围内处理鼠标拖动绘制（左键按下）。
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

    // 选中像素高亮（鼠标悬停时高亮当前像素）。
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

// 右侧区域：工具属性 + 项目信息
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

    static int newWidth = 16;
    static int newHeight = 16;
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

// 底部区域：时间线（帧选择/增删）
void ProjectWindow::renderTimelinePanel(Project* project)
{
    // Timeline 顶部控制栏（参考 Aseprite 风格的左侧按钮条 + 中央滚动条）
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 2.0f));

    // 播放状态与节奏控制（内存态，后续可移入 AppContext）
    static bool isPlaying = false;
    static bool loopEnabled = true;
    static float fps = 8.0f;
    static uint64_t lastTick = 0;
    static double accumulator = 0.0;

    if (lastTick == 0)
        lastTick = SDL_GetTicks();
    const uint64_t nowTick = SDL_GetTicks();
    const double dt = static_cast<double>(nowTick - lastTick) / 1000.0;
    lastTick = nowTick;
    if (isPlaying && fps > 0.0f)
        accumulator += dt;

    // 控制按钮区（含添加/删除帧）
    {
        const ImVec2 btnSize(22.0f, 18.0f);
        if (ImGui::Button("<<", btnSize))
        {
            // 跳到首帧
            context->setCurrentFrameIndex(0);
        }
        ImGui::SameLine();
        if (ImGui::Button("<", btnSize))
        {
            // 上一帧
            const int frameCount = project->getFrameCount();
            const int current = context->getCurrentFrameIndex();
            if (frameCount > 0)
                context->setCurrentFrameIndex(std::max(0, current - 1));
        }
        ImGui::SameLine();
        if (ImGui::Button(">", btnSize))
        {
            // 下一帧
            const int frameCount = project->getFrameCount();
            const int current = context->getCurrentFrameIndex();
            if (frameCount > 0)
                context->setCurrentFrameIndex(std::min(frameCount - 1, current + 1));
        }
        ImGui::SameLine();
        if (ImGui::Button(">>", btnSize))
        {
            // 跳到尾帧
            const int frameCount = project->getFrameCount();
            if (frameCount > 0)
                context->setCurrentFrameIndex(frameCount - 1);
        }
        ImGui::SameLine();
        const char* loopLabel = loopEnabled ? "Loop" : "Once";
        if (ImGui::Button(loopLabel, ImVec2(44.0f, 18.0f)))
        {
            loopEnabled = !loopEnabled;
        }
        ImGui::SameLine();
        if (ImGui::Button("+", btnSize))
        {
            // 添加一帧，并切到新帧
            const int frameCount = project->getFrameCount();
            project->setFrameCount(frameCount + 1, 0x00000000);
            context->setCurrentFrameIndex(frameCount);
            context->setProjectDirty(true);
        }
        ImGui::SameLine();
        if (ImGui::Button("-", btnSize))
        {
            // 删除当前帧（至少保留 1 帧）
            const int frameCount = project->getFrameCount();
            if (frameCount > 1)
            {
                const int current = context->getCurrentFrameIndex();
                project->setFrameCount(frameCount - 1);
                context->setCurrentFrameIndex(std::min(current, frameCount - 2));
                context->setProjectDirty(true);
            }
        }
    }

    ImGui::PopStyleVar(2);

    // 左侧层/行区域 + 右侧帧区域
    const float leftPanelWidth = 120.0f;
    const float timelineHeight = ImGui::GetContentRegionAvail().y;
    ImGui::BeginChild("##TimelineMain", ImVec2(0.0f, timelineHeight), false);

    // 左侧：层/行信息区域（仅 UI 占位）
    ImGui::BeginChild("##TimelineLeft", ImVec2(leftPanelWidth, 0.0f), true);
    ImGui::TextUnformatted("Layers");
    ImGui::Separator();
    ImGui::TextUnformatted("1  Background");
    ImGui::EndChild();

    ImGui::SameLine();

    // 右侧：帧格子区域
    ImGui::BeginChild("##TimelineFrames", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_HorizontalScrollbar);

    // 播放/暂停按钮（当前仅 UI 占位，不实现播放逻辑）。
    static GLuint playIconTexture = 0;
    static GLuint pauseIconTexture = 0;
    static bool iconsLoaded = false;
    ensureTimelineIconTextures(playIconTexture, pauseIconTexture, iconsLoaded);

    const ImVec2 iconSize(20.0f, 20.0f);
    bool clickedToggle = false;
    if (isPlaying)
    {
        if (pauseIconTexture != 0)
        {
            clickedToggle = ImGui::ImageButton(
                "##timeline_toggle",
                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(pauseIconTexture)),
                iconSize);
        }
        else
        {
            clickedToggle = ImGui::Button("Pause");
        }
    }
    else
    {
        if (playIconTexture != 0)
        {
            clickedToggle = ImGui::ImageButton(
                "##timeline_toggle",
                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(playIconTexture)),
                iconSize);
        }
        else
        {
            clickedToggle = ImGui::Button("Play");
        }
    }
    if (clickedToggle)
    {
        // 仅做 UI 状态切换，帧播放逻辑后续再接入。
        isPlaying = !isPlaying;
    }

    ImGui::Separator();

    ImGui::TextUnformatted("FPS");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    ImGui::SliderFloat("##timeline_fps", &fps, 1.0f, 60.0f, "%.0f");

    ImGui::Separator();

    const int frameCount = project->getFrameCount();
    int current = context->getCurrentFrameIndex();
    current = std::clamp(current, 0, std::max(0, frameCount - 1));
    context->setCurrentFrameIndex(current);

    // 播放逻辑：按 fps 推进帧；到尾部时按 loop 状态处理
    if (isPlaying && fps > 0.0f && frameCount > 0)
    {
        const double frameDuration = 1.0 / static_cast<double>(fps);
        while (accumulator >= frameDuration)
        {
            accumulator -= frameDuration;
            int next = context->getCurrentFrameIndex() + 1;
            if (next >= frameCount)
            {
                if (loopEnabled)
                {
                    next = 0;
                }
                else
                {
                    next = frameCount - 1;
                    isPlaying = false;
                    accumulator = 0.0;
                }
            }
            context->setCurrentFrameIndex(next);
        }
    }

    // 顶部帧序号行
    const float cellW = 36.0f;
    const float cellH = 24.0f;
    const float headerH = 18.0f;
    for (int i = 0; i < frameCount; ++i)
    {
        ImGui::PushID(1000 + i);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY());
        ImGui::BeginGroup();
        ImGui::Text(" %d", i + 1);
        ImGui::EndGroup();
        ImGui::PopID();
        ImGui::SameLine(0.0f, cellW - 8.0f);
    }

    ImGui::Dummy(ImVec2(0.0f, headerH));

    // 帧格子行（单层示例）
    for (int i = 0; i < frameCount; ++i)
    {
        ImGui::PushID(i);
        const bool selected = (i == current);
        ImVec4 col = selected ? ImVec4(0.2f, 0.5f, 0.9f, 0.9f) : ImVec4(0.35f, 0.35f, 0.35f, 0.9f);
        ImGui::PushStyleColor(ImGuiCol_Button, col);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(col.x + 0.1f, col.y + 0.1f, col.z + 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(col.x + 0.15f, col.y + 0.15f, col.z + 0.15f, 1.0f));
        if (ImGui::Button("##frame_cell", ImVec2(cellW, cellH)))
        {
            context->setCurrentFrameIndex(i);
        }
        ImGui::PopStyleColor(3);
        ImGui::PopID();
        ImGui::SameLine();
    }

    ImGui::EndChild();
    ImGui::EndChild();
}
