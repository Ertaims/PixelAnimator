#include "ProjectWindow.h"

#include "core/AppContext.h"
#include "imgui.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3_image/SDL_image.h>
#include <string>

namespace
{
    /**
     * @brief 从磁盘加载图片并创建 OpenGL 2D 纹理。
     *
     * @param path 图片文件路径（支持 png 等 SDL_image 可读格式）
     * @return GLuint 成功返回纹理 ID，失败返回 0
     */
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

    /**
     * @brief 懒加载工具栏图标纹理（仅首次调用时执行）。
     *
     * @param brushIcon 画笔图标纹理输出
     * @param eraserIcon 橡皮擦图标纹理输出
     * @param eyedropperIcon 吸管图标纹理输出
     * @param fillIcon 填充工具图标纹理输出
     * @param rectSelectIcon 矩形选区图标纹理输出
     * @param lineIcon 直线工具图标纹理输出
     * @param rectIcon 矩形描边图标纹理输出
     * @param rectFilledIcon 矩形填充图标纹理输出
     * @param loaded 图标是否已加载完成
     */
    void ensureToolbarIconTextures(GLuint& brushIcon,
                                   GLuint& eraserIcon,
                                   GLuint& eyedropperIcon,
                                   GLuint& fillIcon,
                                   GLuint& rectSelectIcon,
                                   GLuint& lineIcon,
                                   GLuint& rectIcon,
                                   GLuint& rectFilledIcon,
                                   bool& loaded)
    {
        if (loaded) return;

        const char* brushCandidates[] = {"src/assets/paintbrush.png", "../src/assets/paintbrush.png", "../../src/assets/paintbrush.png"};
        const char* eraserCandidates[] = {"src/assets/erase.png", "../src/assets/erase.png", "../../src/assets/erase.png"};
        const char* eyedropperCandidates[] = {"src/assets/eyedropper.png", "../src/assets/eyedropper.png", "../../src/assets/eyedropper.png"};
        const char* fillCandidates[] = {"src/assets/fill.png", "../src/assets/fill.png", "../../src/assets/fill.png"};
        const char* rectSelectCandidates[] = {"src/assets/RectangularSelection.png", "../src/assets/RectangularSelection.png", "../../src/assets/RectangularSelection.png"};
        const char* lineCandidates[] = {"src/assets/StraightLine.png", "../src/assets/StraightLine.png", "../../src/assets/StraightLine.png"};
        const char* rectCandidates[] = {"src/assets/rectangleTool.png", "../src/assets/rectangleTool.png", "../../src/assets/rectangleTool.png"};
        const char* rectFilledCandidates[] = {"src/assets/rectangleTool_Filled.png", "../src/assets/rectangleTool_Filled.png", "../../src/assets/rectangleTool_Filled.png"};

        for (const char* p : brushCandidates)
        {
            brushIcon = loadTextureFromFile(p);
            if (brushIcon != 0) break;
        }
        for (const char* p : eraserCandidates)
        {
            eraserIcon = loadTextureFromFile(p);
            if (eraserIcon != 0) break;
        }
        for (const char* p : eyedropperCandidates)
        {
            eyedropperIcon = loadTextureFromFile(p);
            if (eyedropperIcon != 0) break;
        }
        for (const char* p : fillCandidates)
        {
            fillIcon = loadTextureFromFile(p);
            if (fillIcon != 0) break;
        }
        for (const char* p : rectSelectCandidates)
        {
            rectSelectIcon = loadTextureFromFile(p);
            if (rectSelectIcon != 0) break;
        }
        for (const char* p : lineCandidates)
        {
            lineIcon = loadTextureFromFile(p);
            if (lineIcon != 0) break;
        }
        for (const char* p : rectCandidates)
        {
            rectIcon = loadTextureFromFile(p);
            if (rectIcon != 0) break;
        }
        for (const char* p : rectFilledCandidates)
        {
            rectFilledIcon = loadTextureFromFile(p);
            if (rectFilledIcon != 0) break;
        }

        loaded = true;
    }
} // namespace

/**
 * @brief 渲染项目窗口左侧的工具栏（Tools 列）。
 *
 * 本函数除了常规“按钮切换工具”外，还实现了 Rectangle 工具的
 * “长按弹出模式面板，松开确认模式”的交互。
 *
 * 交互状态机（Rectangle）：
 * - Idle：未按下。
 * - Pressing：按下 Rectangle 按钮，开始计时。
 * - PanelVisible：按住超过阈值，显示模式面板（Outline/Filled）。
 * - Commit：鼠标松开时，如果悬停在某模式图标上，则切换到该模式并收起面板。
 */
void ProjectWindow::renderToolbarPanel()
{
    // 确保工具图标纹理可用（首次进入时加载，后续复用）。
    ensureToolbarIconTextures(
        toolbarState_.brushIconTexture,
        toolbarState_.eraserIconTexture,
        toolbarState_.eyedropperIconTexture,
        toolbarState_.fillIconTexture,
        toolbarState_.rectSelectIconTexture,
        toolbarState_.lineIconTexture,
        toolbarState_.rectIconTexture,
        toolbarState_.rectFilledIconTexture,
        toolbarState_.iconsLoaded);

    ImGui::TextUnformatted("Tools");
    ImGui::Separator();

    // 如果当前已处于 Rectangle / RectFilled，则同步“上次矩形模式”。
    // 这样切走其他工具后再回来，图标与模式都能记住用户最近选择。
    if (context->getTool() == ToolType::Rect || context->getTool() == ToolType::RectFilled) toolbarState_.lastRectMode = context->getTool();

    const bool rectModeActive = (context->getTool() == ToolType::Rect || context->getTool() == ToolType::RectFilled);
    const bool rectFilledMode = (toolbarState_.lastRectMode == ToolType::RectFilled);
    const unsigned int currentRectModeIcon =
        rectFilledMode && toolbarState_.rectFilledIconTexture != 0
            ? toolbarState_.rectFilledIconTexture
            : toolbarState_.rectIconTexture;
    const char* currentRectModeLabel = rectFilledMode ? "Rectangle (Filled)" : "Rectangle (Outline)";

    // 工具栏按钮描述：工具类型 + tooltip 文案 + 可选图标纹理。
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
        {ToolType::Fill, "Fill", toolbarState_.fillIconTexture},
        {ToolType::RectSelection, "Rect Selection", toolbarState_.rectSelectIconTexture},
        {ToolType::Line, "Line", toolbarState_.lineIconTexture},
        // 矩形按钮图标会根据当前模式动态切换（描边/填充）。
        {ToolType::Rect, currentRectModeLabel, currentRectModeIcon}
    };

    // 每帧输入快照：避免在循环中多次调用同一输入接口导致阅读复杂。
    const ImVec2 iconSize(26.0f, 26.0f);
    const bool leftMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    const double now = ImGui::GetTime();
    const ImVec2 mousePos = ImGui::GetMousePos();

    for (const ToolbarItem& item : items)
    {
        // Rectangle 按钮视为“模式族入口”（Rect + RectFilled）：
        // 只要当前是任一矩形模式，都给入口按钮显示 selected 态。
        const bool selected = (item.tool == ToolType::Rect) ? rectModeActive : (context->getTool() == item.tool);
        ImGui::PushID(static_cast<int>(item.tool));
        if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.52f, 0.86f, 1.0f));

        // 优先显示图标按钮；如果纹理没加载成功，回退为文本按钮。
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
            // selected 态额外绘制一层黄边，增强低对比主题下的可见性。
            ImGui::PopStyleColor();
            ImGui::GetWindowDrawList()->AddRect(
                ImGui::GetItemRectMin(),
                ImGui::GetItemRectMax(),
                IM_COL32(255, 220, 40, 255),
                4.0f,
                0,
                2.0f);
        }

        if (clicked && item.tool != ToolType::Rect) context->setTool(item.tool);

        // 统一 tooltip。
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", item.label);

        // Rectangle 按钮包含“长按切换子模式”的附加逻辑。
        if (item.tool == ToolType::Rect)
        {
            const ImVec2 rectButtonMin = ImGui::GetItemRectMin();
            const ImVec2 rectButtonMax = ImGui::GetItemRectMax();
            const bool pressedOnRectButton = ImGui::IsItemClicked(ImGuiMouseButton_Left);

            // Rectangle 工具的交互规则：
            // Rectangle 长按交互流程：按下开始计时，达到阈值显示面板，松开时按悬停模式确认并关闭。
            // 这里使用“悬停 + 鼠标按下”作为起点，避免依赖 ImageButton 的激活时机，
            // 确保长按面板在所有平台上都能稳定弹出。
            if (pressedOnRectButton)
            {
                // 进入 Pressing 态：记录长按起点时间 + 面板锚点位置。
                toolbarState_.rectModeLongPressActive = true;
                toolbarState_.rectModeLongPressStart = now;
                toolbarState_.rectModePanelPosX = rectButtonMax.x + 6.0f;
                toolbarState_.rectModePanelPosY = rectButtonMin.y;
                toolbarState_.rectModePanelHasHover = false;

                // 短按时也应切到 Rectangle 工具（沿用上一次模式）。
                if (!rectModeActive) context->setTool(toolbarState_.lastRectMode);
            }

            if (toolbarState_.rectModeLongPressActive && leftMouseDown)
            {
                const double held = now - toolbarState_.rectModeLongPressStart;
                if (held >= static_cast<double>(toolbarState_.rectModeLongPressThreshold))
                {
                    // 达到阈值，进入 PanelVisible 态。
                    toolbarState_.rectModePopupVisible = true;
                    // 仅在“按住过程”内重置并重新计算悬停目标，
                    // 松开当帧保留上一帧的悬停结果用于确认选择。
                    toolbarState_.rectModePanelHasHover = false;

                    ImGui::SetNextWindowPos(
                        ImVec2(toolbarState_.rectModePanelPosX, toolbarState_.rectModePanelPosY),
                        ImGuiCond_Always);

                    ImGuiWindowFlags panelFlags =
                        ImGuiWindowFlags_NoTitleBar |
                        ImGuiWindowFlags_NoResize |
                        ImGuiWindowFlags_NoSavedSettings |
                        ImGuiWindowFlags_AlwaysAutoResize |
                        ImGuiWindowFlags_NoDocking |
                        ImGuiWindowFlags_NoFocusOnAppearing |
                        ImGuiWindowFlags_NoNav;

                    // 每个项目窗口使用独立面板 ID，避免多个 ProjectWindow 之间同名冲突。
                    const std::string panelWindowName =
                        "##RectModePanel_" + std::to_string(reinterpret_cast<uintptr_t>(this));
                    if (ImGui::Begin(panelWindowName.c_str(), nullptr, panelFlags))
                    {
                        // 面板内两个候选模式。lastRectMode 决定默认高亮。
                        const bool modeRect = (toolbarState_.lastRectMode == ToolType::Rect);
                        const bool modeRectFilled = (toolbarState_.lastRectMode == ToolType::RectFilled);
                        const ImVec2 modeIconSize(24.0f, 24.0f);
                        const ImVec2 modeFramePad(3.0f, 3.0f);

                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, modeFramePad);
                        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));

                        if (modeRect) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.52f, 0.86f, 1.0f));
                        if (toolbarState_.rectIconTexture != 0)
                        {
                            ImGui::ImageButton(
                                "##rect_mode_outline_panel",
                                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(toolbarState_.rectIconTexture)),
                                modeIconSize);
                        }
                        else
                        {
                            ImGui::Button("O", modeIconSize);
                        }
                        if (modeRect) ImGui::PopStyleColor();
                        const ImVec2 outlineMin = ImGui::GetItemRectMin();
                        const ImVec2 outlineMax = ImGui::GetItemRectMax();
                        // 不依赖 IsItemHovered：
                        // 长按时按钮 ActiveId 可能影响 hovered 判定，直接用屏幕矩形命中更稳定。
                        const bool outlineHovered =
                            mousePos.x >= outlineMin.x && mousePos.y >= outlineMin.y &&
                            mousePos.x < outlineMax.x && mousePos.y < outlineMax.y;
                        if (outlineHovered)
                        {
                            toolbarState_.rectModePanelHasHover = true;
                            toolbarState_.rectModePanelHoverMode = ToolType::Rect;
                            ImGui::SetTooltip("%s", "Outline Rectangle");
                            // 悬停高亮：黄边，给“松开将选择”的视觉反馈。
                            ImGui::GetWindowDrawList()->AddRect(
                                outlineMin,
                                outlineMax,
                                IM_COL32(255, 220, 40, 255),
                                3.0f,
                                0,
                                2.0f);
                        }

                        ImGui::SameLine();

                        if (modeRectFilled) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.52f, 0.86f, 1.0f));
                        if (toolbarState_.rectFilledIconTexture != 0)
                        {
                            ImGui::ImageButton(
                                "##rect_mode_filled_panel",
                                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(toolbarState_.rectFilledIconTexture)),
                                modeIconSize);
                        }
                        else
                        {
                            ImGui::Button("F", modeIconSize);
                        }
                        if (modeRectFilled) ImGui::PopStyleColor();
                        const ImVec2 filledMin = ImGui::GetItemRectMin();
                        const ImVec2 filledMax = ImGui::GetItemRectMax();
                        // 同样采用屏幕矩形命中，保证 Filled 项在长按链路下可稳定识别。
                        const bool filledHovered =
                            mousePos.x >= filledMin.x && mousePos.y >= filledMin.y &&
                            mousePos.x < filledMax.x && mousePos.y < filledMax.y;
                        if (filledHovered)
                        {
                            toolbarState_.rectModePanelHasHover = true;
                            toolbarState_.rectModePanelHoverMode = ToolType::RectFilled;
                            ImGui::SetTooltip("%s", "Filled Rectangle");
                            ImGui::GetWindowDrawList()->AddRect(
                                filledMin,
                                filledMax,
                                IM_COL32(255, 220, 40, 255),
                                3.0f,
                                0,
                                2.0f);
                        }

                        ImGui::PopStyleVar(2);
                    }
                    ImGui::End();
                }
            }

            // 鼠标松开时完成一次长按会话：
            // - 若面板已显示且悬停某模式图标：确认该模式；
            // - 无悬停则保持当前模式不变；
            // - 最后自动关闭面板。
            if (toolbarState_.rectModeLongPressActive && !leftMouseDown)
            {
                // Commit 阶段：只要释放时有有效 hover 目标，就提交模式切换。
                if (toolbarState_.rectModePopupVisible && toolbarState_.rectModePanelHasHover) toolbarState_.lastRectMode = toolbarState_.rectModePanelHoverMode;
                if (toolbarState_.rectModePopupVisible && toolbarState_.rectModePanelHasHover) context->setTool(toolbarState_.lastRectMode);
                // 无论是否切换，松开后都要清理会话状态，回到 Idle。
                toolbarState_.rectModeLongPressActive = false;
                toolbarState_.rectModePopupVisible = false;
                toolbarState_.rectModePanelHasHover = false;
            }
        }

        ImGui::PopID();
    }
}
