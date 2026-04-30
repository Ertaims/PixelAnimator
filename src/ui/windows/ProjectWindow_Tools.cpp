#include "ProjectWindow.h"

#include "core/AppContext.h"
#include "imgui.h"
#include "render/Texture.h"

#include <string>

namespace
{
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
     * @param circleIcon 圆形描边图标纹理输出
     * @param circleFilledIcon 圆形填充图标纹理输出
     * @param loaded 图标是否已加载完成
     */
    void ensureToolbarIconTextures(unsigned int& brushIcon,
                                   unsigned int& eraserIcon,
                                   unsigned int& eyedropperIcon,
                                   unsigned int& fillIcon,
                                   unsigned int& rectSelectIcon,
                                   unsigned int& circleSelectIcon,
                                   unsigned int& magicWandSelectIcon,
                                   unsigned int& lassoSelectIcon,
                                   unsigned int& polygonLassoSelectIcon,
                                   unsigned int& lineIcon,
                                   unsigned int& curveIcon,
                                   unsigned int& rectIcon,
                                   unsigned int& rectFilledIcon,
                                   unsigned int& circleIcon,
                                   unsigned int& circleFilledIcon,
                                   unsigned int& symmetryLeftRightIcon,
                                   unsigned int& symmetryUpDownIcon,
                                   bool& loaded)
    {
        if (loaded) return;

        const char* brushIconPath               =   { "../src/assets/paintbrush.png" };
        const char* eraserIconPath              =   { "../src/assets/erase.png" };
        const char* eyedropperIconPath          =   { "../src/assets/eyedropper.png" };
        const char* fillIconPath                =   { "../src/assets/fill.png" };
        const char* rectSelectIconPath          =   { "../src/assets/RectangularSelection.png" };
        const char* circleSelectIconPath        =   { "../src/assets/circleSelection.png" };
        const char* magicWandSelectIconPath     =   { "../src/assets/MagicWandSelection.png" };
        const char* lassoSelectIconPath         =   { "../src/assets/lassoSelection.png" };
        const char* polygonLassoSelectIconPath  =   { "../src/assets/PolygonalLassoSelection.png" };
        const char* lineIconPath                =   { "../src/assets/StraightLine.png" };
        const char* curveIconPath               =   { "../src/assets/curveTool.png" };
        const char* rectIconPath                =   { "../src/assets/rectangleTool.png" };
        const char* rectFilledIconPath          =   { "../src/assets/rectangleTool_Filled.png" };
        const char* circleIconPath              =   { "../src/assets/CircleTool.png" };
        const char* circleFilledIconPath        =   { "../src/assets/CircleTool_Filled.png" };
        const char* symmetryLeftRightIconPath   =   { "../src/assets/Symmetry_Right&Left.png" };
        const char* symmetryUpDownIconPath      =   { "../src/assets/Symmetry_Up&Down.png" };

        brushIcon = render::loadTextureFromFile(brushIconPath);
        eraserIcon = render::loadTextureFromFile(eraserIconPath);
        eyedropperIcon = render::loadTextureFromFile(eyedropperIconPath);
        fillIcon = render::loadTextureFromFile(fillIconPath);
        rectSelectIcon = render::loadTextureFromFile(rectSelectIconPath);
        circleSelectIcon = render::loadTextureFromFile(circleSelectIconPath);
        magicWandSelectIcon = render::loadTextureFromFile(magicWandSelectIconPath);
        lassoSelectIcon = render::loadTextureFromFile(lassoSelectIconPath);
        polygonLassoSelectIcon = render::loadTextureFromFile(polygonLassoSelectIconPath);
        lineIcon = render::loadTextureFromFile(lineIconPath);
        curveIcon = render::loadTextureFromFile(curveIconPath);
        rectIcon = render::loadTextureFromFile(rectIconPath);
        rectFilledIcon = render::loadTextureFromFile(rectFilledIconPath);
        circleIcon = render::loadTextureFromFile(circleIconPath);
        circleFilledIcon = render::loadTextureFromFile(circleFilledIconPath);
        symmetryLeftRightIcon = render::loadTextureFromFile(symmetryLeftRightIconPath);
        symmetryUpDownIcon = render::loadTextureFromFile(symmetryUpDownIconPath);

        loaded = true;
    }
}

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
        m_toolbarState.brushIconTexture,
        m_toolbarState.eraserIconTexture,
        m_toolbarState.eyedropperIconTexture,
        m_toolbarState.fillIconTexture,
        m_toolbarState.rectSelectIconTexture,
        m_toolbarState.circleSelectIconTexture,
        m_toolbarState.magicWandSelectIconTexture,
        m_toolbarState.lassoSelectIconTexture,
        m_toolbarState.polygonLassoSelectIconTexture,
        m_toolbarState.lineIconTexture,
        m_toolbarState.curveIconTexture,
        m_toolbarState.rectIconTexture,
        m_toolbarState.rectFilledIconTexture,
        m_toolbarState.circleIconTexture,
        m_toolbarState.circleFilledIconTexture,
        m_toolbarState.symmetryLeftRightIconTexture,
        m_toolbarState.symmetryUpDownIconTexture,
        m_toolbarState.iconsLoaded);

    ImGui::TextUnformatted("Tools");
    ImGui::Separator();

    // 同步工具模式状态：确保切走其他工具后再回来时，能记住用户最近的选择
    if (context->getTool() == ToolType::Rect || context->getTool() == ToolType::RectFilled || 
        context->getTool() == ToolType::Circle || context->getTool() == ToolType::CircleFilled) {
        m_toolbarState.lastRectMode = context->getTool();
    }
    if (context->getTool() == ToolType::Line || context->getTool() == ToolType::Curve) {
        m_toolbarState.lastLineMode = context->getTool();
    }
    if (context->getTool() == ToolType::RectSelection) {
        m_toolbarState.lastSelectionShape = m_rectSelectionTool.getSelectionShape();
    }
    
    // ===== 矩形工具组状态 =====
    const bool rectModeActive = (context->getTool() == ToolType::Rect || context->getTool() == ToolType::RectFilled || 
                               context->getTool() == ToolType::Circle || context->getTool() == ToolType::CircleFilled);
    
    // 确定当前工具图标和标签
    unsigned int currentRectModeIcon = m_toolbarState.rectIconTexture;
    const char* currentRectModeLabel = "Rectangle (Outline)";
    
    if (m_toolbarState.lastRectMode == ToolType::RectFilled) {
        currentRectModeIcon = m_toolbarState.rectFilledIconTexture;
        currentRectModeLabel = "Rectangle (Filled)";
    } else if (m_toolbarState.lastRectMode == ToolType::Circle) {
        currentRectModeIcon = m_toolbarState.circleIconTexture;
        currentRectModeLabel = "Circle (Outline)";
    } else if (m_toolbarState.lastRectMode == ToolType::CircleFilled) {
        currentRectModeIcon = m_toolbarState.circleFilledIconTexture;
        currentRectModeLabel = "Circle (Filled)";
    }
    
    // ===== 线条工具组状态 =====
    const bool lineModeActive = (context->getTool() == ToolType::Line || context->getTool() == ToolType::Curve);
    const bool curveMode = (m_toolbarState.lastLineMode == ToolType::Curve);
    const unsigned int currentLineModeIcon = curveMode && m_toolbarState.curveIconTexture != 0
        ? m_toolbarState.curveIconTexture
        : m_toolbarState.lineIconTexture;
    const char* currentLineModeLabel = curveMode ? "Curve" : "Line";
    
    // ===== 框选工具组状态 =====
    const bool selectionModeActive = (context->getTool() == ToolType::RectSelection);
    const bool selectionCircleMode = (m_toolbarState.lastSelectionShape == RectSelectionTool::SelectionShape::Ellipse);
    const bool selectionWandMode = (m_toolbarState.lastSelectionShape == RectSelectionTool::SelectionShape::MagicWand);
    const bool selectionLassoMode = (m_toolbarState.lastSelectionShape == RectSelectionTool::SelectionShape::Lasso);
    const bool selectionPolygonLassoMode = (m_toolbarState.lastSelectionShape == RectSelectionTool::SelectionShape::PolygonLasso);
    
    // 确定当前框选工具图标
    unsigned int currentSelectionModeIcon = m_toolbarState.rectSelectIconTexture;
    if (selectionPolygonLassoMode && m_toolbarState.polygonLassoSelectIconTexture != 0) {
        currentSelectionModeIcon = m_toolbarState.polygonLassoSelectIconTexture;
    } else if (selectionLassoMode && m_toolbarState.lassoSelectIconTexture != 0) {
        currentSelectionModeIcon = m_toolbarState.lassoSelectIconTexture;
    } else if (selectionWandMode && m_toolbarState.magicWandSelectIconTexture != 0) {
        currentSelectionModeIcon = m_toolbarState.magicWandSelectIconTexture;
    } else if (selectionCircleMode && m_toolbarState.circleSelectIconTexture != 0) {
        currentSelectionModeIcon = m_toolbarState.circleSelectIconTexture;
    }
    
    // 确定当前框选工具标签
    const char* currentSelectionModeLabel = "Rect Selection";
    if (selectionPolygonLassoMode) {
        currentSelectionModeLabel = "Polygon Lasso Selection";
    } else if (selectionLassoMode) {
        currentSelectionModeLabel = "Lasso Selection";
    } else if (selectionWandMode && m_toolbarState.magicWandSelectIconTexture != 0) {
        currentSelectionModeLabel = "Magic Wand Selection";
    } else if (selectionCircleMode) {
        currentSelectionModeLabel = "Circle Selection";
    }

    // 工具栏按钮描述：工具类型 + tooltip 文案 + 可选图标纹理。
    struct ToolbarItem
    {
        ToolType tool;
        const char* label;
        unsigned int icon;
    };

    const ToolbarItem items[] = {
        {ToolType::Brush, "Brush", m_toolbarState.brushIconTexture},
        {ToolType::Eraser, "Eraser", m_toolbarState.eraserIconTexture},
        {ToolType::Eyedropper, "Eyedropper", m_toolbarState.eyedropperIconTexture},
        {ToolType::Fill, "Fill", m_toolbarState.fillIconTexture},
        {ToolType::RectSelection, currentSelectionModeLabel, currentSelectionModeIcon},
        {ToolType::Line, currentLineModeLabel, currentLineModeIcon},
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
        const bool selected =
            (item.tool == ToolType::Rect) ? rectModeActive :
            ((item.tool == ToolType::Line) ? lineModeActive : (context->getTool() == item.tool));
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

        if (clicked && item.tool != ToolType::Rect && item.tool != ToolType::Line) context->setTool(item.tool);

        // 统一 tooltip。
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", item.label);

        // RectSelection 按钮包含“长按切换子模式”的附加逻辑。
        if (item.tool == ToolType::RectSelection)
        {
            const ImVec2 rectButtonMin = ImGui::GetItemRectMin();
            const ImVec2 rectButtonMax = ImGui::GetItemRectMax();
            const bool pressedOnRectButton = ImGui::IsItemClicked(ImGuiMouseButton_Left);

            if (pressedOnRectButton)
            {
                m_toolbarState.selectionModeLongPressActive = true;
                m_toolbarState.selectionModeLongPressStart = now;
                m_toolbarState.selectionModePanelPosX = rectButtonMax.x + 6.0f;
                m_toolbarState.selectionModePanelPosY = rectButtonMin.y;
                m_toolbarState.selectionModePanelHasHover = false;

                if (!selectionModeActive)
                {
                    context->setTool(ToolType::RectSelection);
                }
            }

            if (m_toolbarState.selectionModeLongPressActive && leftMouseDown)
            {
                const double held = now - m_toolbarState.selectionModeLongPressStart;
                if (held >= static_cast<double>(m_toolbarState.selectionModeLongPressThreshold))
                {
                    m_toolbarState.selectionModePopupVisible = true;
                    m_toolbarState.selectionModePanelHasHover = false;

                    ImGui::SetNextWindowPos(
                        ImVec2(m_toolbarState.selectionModePanelPosX, m_toolbarState.selectionModePanelPosY),
                        ImGuiCond_Always);

                    ImGuiWindowFlags panelFlags =
                        ImGuiWindowFlags_NoTitleBar |
                        ImGuiWindowFlags_NoResize |
                        ImGuiWindowFlags_NoSavedSettings |
                        ImGuiWindowFlags_AlwaysAutoResize |
                        ImGuiWindowFlags_NoDocking |
                        ImGuiWindowFlags_NoFocusOnAppearing |
                        ImGuiWindowFlags_NoNav;

                    const std::string panelWindowName =
                        "##SelectionModePanel_" + std::to_string(reinterpret_cast<uintptr_t>(this));
                    if (ImGui::Begin(panelWindowName.c_str(), nullptr, panelFlags))
                    {
                        const bool modeRect = (m_toolbarState.lastSelectionShape == RectSelectionTool::SelectionShape::Rectangle);
                        const bool modeCircle = (m_toolbarState.lastSelectionShape == RectSelectionTool::SelectionShape::Ellipse);
                        const bool modeWand = (m_toolbarState.lastSelectionShape == RectSelectionTool::SelectionShape::MagicWand);
                        const bool modeLasso = (m_toolbarState.lastSelectionShape == RectSelectionTool::SelectionShape::Lasso);
                        const bool modePolygonLasso = (m_toolbarState.lastSelectionShape == RectSelectionTool::SelectionShape::PolygonLasso);
                        const ImVec2 modeIconSize(24.0f, 24.0f);
                        const ImVec2 modeFramePad(3.0f, 3.0f);

                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, modeFramePad);
                        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));

                        if (modeRect) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.52f, 0.86f, 1.0f));
                        if (m_toolbarState.rectSelectIconTexture != 0)
                        {
                            ImGui::ImageButton(
                                "##selection_mode_rect_panel",
                                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(m_toolbarState.rectSelectIconTexture)),
                                modeIconSize);
                        }
                        else
                        {
                            ImGui::Button("R", modeIconSize);
                        }
                        if (modeRect) ImGui::PopStyleColor();
                        const ImVec2 rectMin = ImGui::GetItemRectMin();
                        const ImVec2 rectMax = ImGui::GetItemRectMax();
                        const bool rectHovered =
                            mousePos.x >= rectMin.x && mousePos.y >= rectMin.y &&
                            mousePos.x < rectMax.x && mousePos.y < rectMax.y;
                        if (rectHovered)
                        {
                            m_toolbarState.selectionModePanelHasHover = true;
                            m_toolbarState.selectionModePanelHoverShape = RectSelectionTool::SelectionShape::Rectangle;
                            ImGui::SetTooltip("%s", "Rect Selection");
                            ImGui::GetWindowDrawList()->AddRect(
                                rectMin,
                                rectMax,
                                IM_COL32(255, 220, 40, 255),
                                3.0f,
                                0,
                                2.0f);
                        }

                        ImGui::SameLine();

                        if (modeCircle) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.52f, 0.86f, 1.0f));
                        if (m_toolbarState.circleSelectIconTexture != 0)
                        {
                            ImGui::ImageButton(
                                "##selection_mode_circle_panel",
                                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(m_toolbarState.circleSelectIconTexture)),
                                modeIconSize);
                        }
                        else
                        {
                            ImGui::Button("C", modeIconSize);
                        }
                        if (modeCircle) ImGui::PopStyleColor();
                        const ImVec2 circleMin = ImGui::GetItemRectMin();
                        const ImVec2 circleMax = ImGui::GetItemRectMax();
                        const bool circleHovered =
                            mousePos.x >= circleMin.x && mousePos.y >= circleMin.y &&
                            mousePos.x < circleMax.x && mousePos.y < circleMax.y;
                        if (circleHovered)
                        {
                            m_toolbarState.selectionModePanelHasHover = true;
                            m_toolbarState.selectionModePanelHoverShape = RectSelectionTool::SelectionShape::Ellipse;
                            ImGui::SetTooltip("%s", "Circle Selection");
                            ImGui::GetWindowDrawList()->AddRect(
                                circleMin,
                                circleMax,
                                IM_COL32(255, 220, 40, 255),
                                3.0f,
                                0,
                                2.0f);
                        }

                        ImGui::SameLine();

                        if (modeWand) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.52f, 0.86f, 1.0f));
                        if (m_toolbarState.magicWandSelectIconTexture != 0)
                        {
                            ImGui::ImageButton(
                                "##selection_mode_wand_panel",
                                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(m_toolbarState.magicWandSelectIconTexture)),
                                modeIconSize);
                        }
                        else
                        {
                            ImGui::Button("W", modeIconSize);
                        }
                        if (modeWand) ImGui::PopStyleColor();
                        const ImVec2 wandMin = ImGui::GetItemRectMin();
                        const ImVec2 wandMax = ImGui::GetItemRectMax();
                        const bool wandHovered =
                            mousePos.x >= wandMin.x && mousePos.y >= wandMin.y &&
                            mousePos.x < wandMax.x && mousePos.y < wandMax.y;
                        if (wandHovered)
                        {
                            m_toolbarState.selectionModePanelHasHover = true;
                            m_toolbarState.selectionModePanelHoverShape = RectSelectionTool::SelectionShape::MagicWand;
                            ImGui::SetTooltip("%s", "Magic Wand Selection");
                            ImGui::GetWindowDrawList()->AddRect(
                                wandMin,
                                wandMax,
                                IM_COL32(255, 220, 40, 255),
                                3.0f,
                                0,
                                2.0f);
                        }

                        ImGui::SameLine();

                        if (modeLasso) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.52f, 0.86f, 1.0f));
                        if (m_toolbarState.lassoSelectIconTexture != 0)
                        {
                            ImGui::ImageButton(
                                "##selection_mode_lasso_panel",
                                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(m_toolbarState.lassoSelectIconTexture)),
                                modeIconSize);
                        }
                        else
                        {
                            ImGui::Button("L", modeIconSize);
                        }
                        if (modeLasso) ImGui::PopStyleColor();
                        const ImVec2 lassoMin = ImGui::GetItemRectMin();
                        const ImVec2 lassoMax = ImGui::GetItemRectMax();
                        const bool lassoHovered =
                            mousePos.x >= lassoMin.x && mousePos.y >= lassoMin.y &&
                            mousePos.x < lassoMax.x && mousePos.y < lassoMax.y;
                        if (lassoHovered)
                        {
                            m_toolbarState.selectionModePanelHasHover = true;
                            m_toolbarState.selectionModePanelHoverShape = RectSelectionTool::SelectionShape::Lasso;
                            ImGui::SetTooltip("%s", "Lasso Selection");
                            ImGui::GetWindowDrawList()->AddRect(
                                lassoMin,
                                lassoMax,
                                IM_COL32(255, 220, 40, 255),
                                3.0f,
                                0,
                                2.0f);
                        }

                        ImGui::SameLine();

                        if (modePolygonLasso) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.52f, 0.86f, 1.0f));
                        if (m_toolbarState.polygonLassoSelectIconTexture != 0)
                        {
                            ImGui::ImageButton(
                                "##selection_mode_polygon_lasso_panel",
                                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(m_toolbarState.polygonLassoSelectIconTexture)),
                                modeIconSize);
                        }
                        else
                        {
                            ImGui::Button("PL", modeIconSize);
                        }
                        if (modePolygonLasso) ImGui::PopStyleColor();
                        const ImVec2 polyMin = ImGui::GetItemRectMin();
                        const ImVec2 polyMax = ImGui::GetItemRectMax();
                        const bool polyHovered =
                            mousePos.x >= polyMin.x && mousePos.y >= polyMin.y &&
                            mousePos.x < polyMax.x && mousePos.y < polyMax.y;
                        if (polyHovered)
                        {
                            m_toolbarState.selectionModePanelHasHover = true;
                            m_toolbarState.selectionModePanelHoverShape = RectSelectionTool::SelectionShape::PolygonLasso;
                            ImGui::SetTooltip("%s", "Polygon Lasso Selection");
                            ImGui::GetWindowDrawList()->AddRect(
                                polyMin,
                                polyMax,
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

            if (m_toolbarState.selectionModeLongPressActive && !leftMouseDown)
            {
                if (m_toolbarState.selectionModePopupVisible && m_toolbarState.selectionModePanelHasHover)
                {
                    m_toolbarState.lastSelectionShape = m_toolbarState.selectionModePanelHoverShape;
                    m_rectSelectionTool.setSelectionShape(m_toolbarState.lastSelectionShape);
                    context->setTool(ToolType::RectSelection);
                }
                m_toolbarState.selectionModeLongPressActive = false;
                m_toolbarState.selectionModePopupVisible = false;
                m_toolbarState.selectionModePanelHasHover = false;
            }
        }

        // Line 按钮包含“长按切换子模式（Line / Curve）”的附加逻辑。
        if (item.tool == ToolType::Line)
        {
            const ImVec2 lineButtonMin = ImGui::GetItemRectMin();
            const ImVec2 lineButtonMax = ImGui::GetItemRectMax();
            const bool pressedOnLineButton = ImGui::IsItemClicked(ImGuiMouseButton_Left);

            if (pressedOnLineButton)
            {
                m_toolbarState.lineModeLongPressActive = true;
                m_toolbarState.lineModeLongPressStart = now;
                m_toolbarState.lineModePanelPosX = lineButtonMax.x + 6.0f;
                m_toolbarState.lineModePanelPosY = lineButtonMin.y;
                m_toolbarState.lineModePanelHasHover = false;
                if (!lineModeActive) context->setTool(m_toolbarState.lastLineMode);
            }

            if (m_toolbarState.lineModeLongPressActive && leftMouseDown)
            {
                const double held = now - m_toolbarState.lineModeLongPressStart;
                if (held >= static_cast<double>(m_toolbarState.lineModeLongPressThreshold))
                {
                    m_toolbarState.lineModePopupVisible = true;
                    m_toolbarState.lineModePanelHasHover = false;

                    ImGui::SetNextWindowPos(
                        ImVec2(m_toolbarState.lineModePanelPosX, m_toolbarState.lineModePanelPosY),
                        ImGuiCond_Always);

                    ImGuiWindowFlags panelFlags =
                        ImGuiWindowFlags_NoTitleBar |
                        ImGuiWindowFlags_NoResize |
                        ImGuiWindowFlags_NoSavedSettings |
                        ImGuiWindowFlags_AlwaysAutoResize |
                        ImGuiWindowFlags_NoDocking |
                        ImGuiWindowFlags_NoFocusOnAppearing |
                        ImGuiWindowFlags_NoNav;

                    const std::string panelWindowName =
                        "##LineModePanel_" + std::to_string(reinterpret_cast<uintptr_t>(this));
                    if (ImGui::Begin(panelWindowName.c_str(), nullptr, panelFlags))
                    {
                        const bool modeLine = (m_toolbarState.lastLineMode == ToolType::Line);
                        const bool modeCurve = (m_toolbarState.lastLineMode == ToolType::Curve);
                        const ImVec2 modeIconSize(24.0f, 24.0f);
                        const ImVec2 modeFramePad(3.0f, 3.0f);

                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, modeFramePad);
                        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));

                        if (modeLine) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.52f, 0.86f, 1.0f));
                        if (m_toolbarState.lineIconTexture != 0)
                        {
                            ImGui::ImageButton(
                                "##line_mode_line_panel",
                                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(m_toolbarState.lineIconTexture)),
                                modeIconSize);
                        }
                        else
                        {
                            ImGui::Button("L", modeIconSize);
                        }
                        if (modeLine) ImGui::PopStyleColor();
                        const ImVec2 lineMin = ImGui::GetItemRectMin();
                        const ImVec2 lineMax = ImGui::GetItemRectMax();
                        const bool lineHovered =
                            mousePos.x >= lineMin.x && mousePos.y >= lineMin.y &&
                            mousePos.x < lineMax.x && mousePos.y < lineMax.y;
                        if (lineHovered)
                        {
                            m_toolbarState.lineModePanelHasHover = true;
                            m_toolbarState.lineModePanelHoverMode = ToolType::Line;
                            ImGui::SetTooltip("%s", "Line");
                            ImGui::GetWindowDrawList()->AddRect(
                                lineMin,
                                lineMax,
                                IM_COL32(255, 220, 40, 255),
                                3.0f,
                                0,
                                2.0f);
                        }

                        ImGui::SameLine();

                        if (modeCurve) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.52f, 0.86f, 1.0f));
                        if (m_toolbarState.curveIconTexture != 0)
                        {
                            ImGui::ImageButton(
                                "##line_mode_curve_panel",
                                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(m_toolbarState.curveIconTexture)),
                                modeIconSize);
                        }
                        else
                        {
                            ImGui::Button("C", modeIconSize);
                        }
                        if (modeCurve) ImGui::PopStyleColor();
                        const ImVec2 curveMin = ImGui::GetItemRectMin();
                        const ImVec2 curveMax = ImGui::GetItemRectMax();
                        const bool curveHovered =
                            mousePos.x >= curveMin.x && mousePos.y >= curveMin.y &&
                            mousePos.x < curveMax.x && mousePos.y < curveMax.y;
                        if (curveHovered)
                        {
                            m_toolbarState.lineModePanelHasHover = true;
                            m_toolbarState.lineModePanelHoverMode = ToolType::Curve;
                            ImGui::SetTooltip("%s", "Curve");
                            ImGui::GetWindowDrawList()->AddRect(
                                curveMin,
                                curveMax,
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

            if (m_toolbarState.lineModeLongPressActive && !leftMouseDown)
            {
                if (m_toolbarState.lineModePopupVisible && m_toolbarState.lineModePanelHasHover)
                {
                    m_toolbarState.lastLineMode = m_toolbarState.lineModePanelHoverMode;
                    context->setTool(m_toolbarState.lastLineMode);
                }
                m_toolbarState.lineModeLongPressActive = false;
                m_toolbarState.lineModePopupVisible = false;
                m_toolbarState.lineModePanelHasHover = false;
            }
        }

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
                m_toolbarState.rectModeLongPressActive = true;
                m_toolbarState.rectModeLongPressStart = now;
                m_toolbarState.rectModePanelPosX = rectButtonMax.x + 6.0f;
                m_toolbarState.rectModePanelPosY = rectButtonMin.y;
                m_toolbarState.rectModePanelHasHover = false;

                // 短按时也应切到 Rectangle 工具（沿用上一次模式）。
                if (!rectModeActive) context->setTool(m_toolbarState.lastRectMode);
            }

            if (m_toolbarState.rectModeLongPressActive && leftMouseDown)
            {
                const double held = now - m_toolbarState.rectModeLongPressStart;
                if (held >= static_cast<double>(m_toolbarState.rectModeLongPressThreshold))
                {
                    // 达到阈值，进入 PanelVisible 态。
                    m_toolbarState.rectModePopupVisible = true;
                    // 仅在“按住过程”内重置并重新计算悬停目标，
                    // 松开当帧保留上一帧的悬停结果用于确认选择。
                    m_toolbarState.rectModePanelHasHover = false;

                    ImGui::SetNextWindowPos(
                        ImVec2(m_toolbarState.rectModePanelPosX, m_toolbarState.rectModePanelPosY),
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
                        // 面板内四个候选模式。lastRectMode 决定默认高亮。
                        const bool modeRect = (m_toolbarState.lastRectMode == ToolType::Rect);
                        const bool modeRectFilled = (m_toolbarState.lastRectMode == ToolType::RectFilled);
                        const bool modeCircle = (m_toolbarState.lastRectMode == ToolType::Circle);
                        const bool modeCircleFilled = (m_toolbarState.lastRectMode == ToolType::CircleFilled);
                        const ImVec2 modeIconSize(24.0f, 24.0f);
                        const ImVec2 modeFramePad(3.0f, 3.0f);

                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, modeFramePad);
                        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));

                        // 矩形（描边）
                        if (modeRect) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.52f, 0.86f, 1.0f));
                        if (m_toolbarState.rectIconTexture != 0)
                        {
                            ImGui::ImageButton(
                                "##rect_mode_outline_panel",
                                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(m_toolbarState.rectIconTexture)),
                                modeIconSize);
                        }
                        else
                        {
                            ImGui::Button("O", modeIconSize);
                        }
                        if (modeRect) ImGui::PopStyleColor();
                        const ImVec2 outlineMin = ImGui::GetItemRectMin();
                        const ImVec2 outlineMax = ImGui::GetItemRectMax();
                        const bool outlineHovered =
                            mousePos.x >= outlineMin.x && mousePos.y >= outlineMin.y &&
                            mousePos.x < outlineMax.x && mousePos.y < outlineMax.y;
                        if (outlineHovered)
                        {
                            m_toolbarState.rectModePanelHasHover = true;
                            m_toolbarState.rectModePanelHoverMode = ToolType::Rect;
                            ImGui::SetTooltip("%s", "Outline Rectangle");
                            ImGui::GetWindowDrawList()->AddRect(
                                outlineMin,
                                outlineMax,
                                IM_COL32(255, 220, 40, 255),
                                3.0f,
                                0,
                                2.0f);
                        }

                        ImGui::SameLine();

                        // 矩形（填充）
                        if (modeRectFilled) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.52f, 0.86f, 1.0f));
                        if (m_toolbarState.rectFilledIconTexture != 0)
                        {
                            ImGui::ImageButton(
                                "##rect_mode_filled_panel",
                                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(m_toolbarState.rectFilledIconTexture)),
                                modeIconSize);
                        }
                        else
                        {
                            ImGui::Button("F", modeIconSize);
                        }
                        if (modeRectFilled) ImGui::PopStyleColor();
                        const ImVec2 filledMin = ImGui::GetItemRectMin();
                        const ImVec2 filledMax = ImGui::GetItemRectMax();
                        const bool filledHovered =
                            mousePos.x >= filledMin.x && mousePos.y >= filledMin.y &&
                            mousePos.x < filledMax.x && mousePos.y < filledMax.y;
                        if (filledHovered)
                        {
                            m_toolbarState.rectModePanelHasHover = true;
                            m_toolbarState.rectModePanelHoverMode = ToolType::RectFilled;
                            ImGui::SetTooltip("%s", "Filled Rectangle");
                            ImGui::GetWindowDrawList()->AddRect(
                                filledMin,
                                filledMax,
                                IM_COL32(255, 220, 40, 255),
                                3.0f,
                                0,
                                2.0f);
                        }

                        ImGui::SameLine();

                        // 圆形（描边）
                        if (modeCircle) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.52f, 0.86f, 1.0f));
                        if (m_toolbarState.circleIconTexture != 0)
                        {
                            ImGui::ImageButton(
                                "##circle_mode_outline_panel",
                                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(m_toolbarState.circleIconTexture)),
                                modeIconSize);
                        }
                        else
                        {
                            ImGui::Button("OC", modeIconSize);
                        }
                        if (modeCircle) ImGui::PopStyleColor();
                        const ImVec2 circleOutlineMin = ImGui::GetItemRectMin();
                        const ImVec2 circleOutlineMax = ImGui::GetItemRectMax();
                        const bool circleOutlineHovered =
                            mousePos.x >= circleOutlineMin.x && mousePos.y >= circleOutlineMin.y &&
                            mousePos.x < circleOutlineMax.x && mousePos.y < circleOutlineMax.y;
                        if (circleOutlineHovered)
                        {
                            m_toolbarState.rectModePanelHasHover = true;
                            m_toolbarState.rectModePanelHoverMode = ToolType::Circle;
                            ImGui::SetTooltip("%s", "Outline Circle");
                            ImGui::GetWindowDrawList()->AddRect(
                                circleOutlineMin,
                                circleOutlineMax,
                                IM_COL32(255, 220, 40, 255),
                                3.0f,
                                0,
                                2.0f);
                        }

                        ImGui::SameLine();

                        // 圆形（填充）
                        if (modeCircleFilled) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.52f, 0.86f, 1.0f));
                        if (m_toolbarState.circleFilledIconTexture != 0)
                        {
                            ImGui::ImageButton(
                                "##circle_mode_filled_panel",
                                reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(m_toolbarState.circleFilledIconTexture)),
                                modeIconSize);
                        }
                        else
                        {
                            ImGui::Button("FC", modeIconSize);
                        }
                        if (modeCircleFilled) ImGui::PopStyleColor();
                        const ImVec2 circleFilledMin = ImGui::GetItemRectMin();
                        const ImVec2 circleFilledMax = ImGui::GetItemRectMax();
                        const bool circleFilledHovered =
                            mousePos.x >= circleFilledMin.x && mousePos.y >= circleFilledMin.y &&
                            mousePos.x < circleFilledMax.x && mousePos.y < circleFilledMax.y;
                        if (circleFilledHovered)
                        {
                            m_toolbarState.rectModePanelHasHover = true;
                            m_toolbarState.rectModePanelHoverMode = ToolType::CircleFilled;
                            ImGui::SetTooltip("%s", "Filled Circle");
                            ImGui::GetWindowDrawList()->AddRect(
                                circleFilledMin,
                                circleFilledMax,
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
            if (m_toolbarState.rectModeLongPressActive && !leftMouseDown)
            {
                // Commit 阶段：只要释放时有有效 hover 目标，就提交模式切换。
                if (m_toolbarState.rectModePopupVisible && m_toolbarState.rectModePanelHasHover) m_toolbarState.lastRectMode = m_toolbarState.rectModePanelHoverMode;
                if (m_toolbarState.rectModePopupVisible && m_toolbarState.rectModePanelHasHover) context->setTool(m_toolbarState.lastRectMode);
                // 无论是否切换，松开后都要清理会话状态，回到 Idle。
                m_toolbarState.rectModeLongPressActive = false;
                m_toolbarState.rectModePopupVisible = false;
                m_toolbarState.rectModePanelHasHover = false;
            }
        }

        ImGui::PopID();
    }

}


