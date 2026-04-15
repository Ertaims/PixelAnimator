#include "ProjectWindow.h"

#include "commands/PixelClipboardCommands.h"
#include "core/AppContext.h"
#include "core/Project.h"
#include "imgui.h"
#include "tools/BrushTool.h"
#include "tools/EraserTool.h"
#include "tools/EyedropperTool.h"
#include "tools/FillTool.h"
#include "tools/LineTool.h"
#include "tools/RectFilledTool.h"
#include "tools/RectangleTool.h"
#include "tools/Tool.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{
    static constexpr int kZoomLevels[] = {1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024};
    static constexpr int kZoomLevelCount = static_cast<int>(sizeof(kZoomLevels) / sizeof(kZoomLevels[0]));

    int findZoomIndex(int zoom)
    {
        for (int i = 0; i < kZoomLevelCount; ++i)
        {
            if (kZoomLevels[i] == zoom) return i;
        }
        int best = 0;
        int bestDist = std::abs(kZoomLevels[0] - zoom);
        for (int i = 1; i < kZoomLevelCount; ++i)
        {
            const int dist = std::abs(kZoomLevels[i] - zoom);
            if (dist < bestDist)
            {
                bestDist = dist;
                best = i;
            }
        }
        return best;
    }

    // 计算一个“尽量完整显示画布”的缩放值（从预设缩放档位中选择）。
    int computeFitZoomForPanel(int canvasWidth, int canvasHeight, const ImVec2& panelAvail)
    {
        if (canvasWidth <= 0 || canvasHeight <= 0) return 1;
        const float safeW = std::max(1.0f, panelAvail.x);
        const float safeH = std::max(1.0f, panelAvail.y);
        const float fitByW = safeW / static_cast<float>(canvasWidth);
        const float fitByH = safeH / static_cast<float>(canvasHeight);
        const float fit = std::max(1.0f, std::floor(std::min(fitByW, fitByH)));
        int fitZoom = 1;
        for (int i = 0; i < kZoomLevelCount; ++i)
        {
            if (static_cast<float>(kZoomLevels[i]) <= fit) fitZoom = kZoomLevels[i];
            else
                break;
        }
        return fitZoom;
    }

    const Tool* resolveTool(ToolType toolType)
    {
        static const BrushTool kBrushTool;
        static const EraserTool kEraserTool;
        static const EyedropperTool kEyedropperTool;
        static const FillTool kFillTool;

        switch (toolType)
        {
        case ToolType::Brush:
            return &kBrushTool;
        case ToolType::Eraser:
            return &kEraserTool;
        case ToolType::Eyedropper:
            return &kEyedropperTool;
        case ToolType::Fill:
            return &kFillTool;
        default:
            return nullptr;
        }
    }

    // 将鼠标屏幕坐标映射到画布像素坐标，并夹到合法范围。
    void getClampedPixelFromMouse(const ImVec2& mousePos,
                                  const ImVec2& imagePos,
                                  int zoom,
                                  int canvasWidth,
                                  int canvasHeight,
                                  int& outX,
                                  int& outY)
    {
        const float localX = mousePos.x - imagePos.x;
        const float localY = mousePos.y - imagePos.y;
        outX = std::clamp(static_cast<int>(localX / static_cast<float>(zoom)), 0, canvasWidth - 1);
        outY = std::clamp(static_cast<int>(localY / static_cast<float>(zoom)), 0, canvasHeight - 1);
    }

    // 将项目内使用的 RGBA8888（R 低字节，A 高字节）转换为 ImGui 颜色。
    ImU32 toImGuiColor(uint32_t rgba)
    {
        const int r = static_cast<int>(rgba & 0xFFu);
        const int g = static_cast<int>((rgba >> 8) & 0xFFu);
        const int b = static_cast<int>((rgba >> 16) & 0xFFu);
        const int a = static_cast<int>((rgba >> 24) & 0xFFu);
        return IM_COL32(r, g, b, a);
    }

    // 与 toImGuiColor 类似，但允许覆盖 alpha，便于绘制半透明预览。
    ImU32 toImGuiColorWithAlpha(uint32_t rgba, int forcedAlpha)
    {
        const int r = static_cast<int>(rgba & 0xFFu);
        const int g = static_cast<int>((rgba >> 8) & 0xFFu);
        const int b = static_cast<int>((rgba >> 16) & 0xFFu);
        const int a = std::clamp(forcedAlpha, 0, 255);
        return IM_COL32(r, g, b, a);
    }

    /**
     * @brief 在两点间做离散插值，并调用工具逐点落笔。
     *
     * 设计目的：
     * - 解决“快速拖拽时两帧采样点间距过大，导致笔迹断裂”的问题；
     * - 使用整数步进（DDA），保证每个经过像素都能被工具处理一次。
     */
    bool applyInterpolatedStroke(const Tool& tool,
                                 Project::Frame& frame,
                                 int canvasWidth,
                                 int canvasHeight,
                                 int fromX,
                                 int fromY,
                                 int toX,
                                 int toY,
                                 AppContext& context,
                                 bool isMouseClicked)
    {
        const int dx = toX - fromX;
        const int dy = toY - fromY;
        const int steps = std::max(std::abs(dx), std::abs(dy));

        // 退化情况：起点终点相同，直接单点落笔。
        if (steps <= 0)
        {
            return tool.apply(frame,
                              canvasWidth,
                              canvasHeight,
                              toX,
                              toY,
                              context,
                              isMouseClicked);
        }

        bool changed = false;
        for (int i = 0; i <= steps; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            const int px = fromX + static_cast<int>(std::lround(static_cast<float>(dx) * t));
            const int py = fromY + static_cast<int>(std::lround(static_cast<float>(dy) * t));

            // 仅第一步保留“点击瞬间”语义（给 Fill/Eyedropper 等一次性工具用），
            // 后续补点统一按拖拽连续输入处理。
            const bool stepClicked = (i == 0) ? isMouseClicked : false;
            if (tool.apply(frame,
                           canvasWidth,
                           canvasHeight,
                           px,
                           py,
                           context,
                           stepClicked))
            {
                changed = true;
            }
        }
        return changed;
    }
} // namespace

// 画布面板
void ProjectWindow::renderCanvasPanel(Project* project)
{
    const int width = project->getWidth();
    const int height = project->getHeight();
    int zoom = context->getCanvasZoom();
    const ImVec2 panelPos = ImGui::GetCursorScreenPos();
    const ImVec2 panelAvail = ImGui::GetContentRegionAvail();

    // 画布尺寸变化时自动适配一个可视化更合理的缩放，并重置平移。
    if (width != lastCanvasWidth_ || height != lastCanvasHeight_)
    {
        const int fitZoom = computeFitZoomForPanel(width, height, panelAvail);
        context->setCanvasZoom(fitZoom);
        context->setCanvasPan(0.0f, 0.0f);
        zoom = context->getCanvasZoom();
        lastCanvasWidth_ = width;
        lastCanvasHeight_ = height;
    }

    // 同步选区掩码尺寸，确保画布尺寸变化后选区状态一致。
    context->ensurePixelSelectionCanvasSize(width, height);

    // 多选状态下，画布始终显示“主选中帧”（选区第一帧）。
    context->sanitizeFrameSelection(project->getFrameCount(), context->getCurrentFrameIndex());
    int frameIndex = context->getPrimarySelectedFrameIndex();
    frameIndex = std::clamp(frameIndex, 0, std::max(0, project->getFrameCount() - 1));
    context->setCurrentFrameIndex(frameIndex);
    const int frameCount = project->getFrameCount();

    Project::Frame& frame = project->getFrame(frameIndex);
    ensureCanvasTexture(width, height);
    uploadCanvasPixels(frame.pixels);

    auto computeImageMetrics = [&](int zoomValue, float panX, float panY, float& outImageW, float& outImageH, ImVec2& outCenterOffset, ImVec2& outImagePos) {
        outImageW = static_cast<float>(width * zoomValue);
        outImageH = static_cast<float>(height * zoomValue);
        outCenterOffset = ImVec2((panelAvail.x - outImageW) * 0.5f, (panelAvail.y - outImageH) * 0.5f);
        // 将画布左上角对齐到整数像素，避免亚像素位置导致边缘出现细线伪影。
        outImagePos = ImVec2(
            std::round(panelPos.x + outCenterOffset.x + panX),
            std::round(panelPos.y + outCenterOffset.y + panY));
    };

    float panX = context->getCanvasPanX();
    float panY = context->getCanvasPanY();
    float imageW = 0.0f;
    float imageH = 0.0f;
    ImVec2 centerOffset(0.0f, 0.0f);
    ImVec2 imagePos(0.0f, 0.0f);
    computeImageMetrics(zoom, panX, panY, imageW, imageH, centerOffset, imagePos);

    const ImVec2 hitboxSize(std::max(1.0f, panelAvail.x), std::max(1.0f, panelAvail.y));
    ImGui::InvisibleButton(
        "##CanvasHitbox",
        hitboxSize,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle | ImGuiButtonFlags_MouseButtonRight);

    const bool canvasHitboxHovered = ImGui::IsItemHovered();
    const ImVec2 mousePos = ImGui::GetMousePos();
    const bool hoveredBeforeZoom =
        mousePos.x >= imagePos.x &&
        mousePos.y >= imagePos.y &&
        mousePos.x < (imagePos.x + imageW) &&
        mousePos.y < (imagePos.y + imageH);

    if (canvasHitboxHovered)
    {
        const float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f)
        {
            int zoomIndex = findZoomIndex(zoom);
            zoomIndex = std::clamp(zoomIndex + (wheel > 0.0f ? 1 : -1), 0, kZoomLevelCount - 1);
            const int newZoom = kZoomLevels[zoomIndex];
            if (newZoom != zoom)
            {
                // 鼠标锚点缩放：缩放前后让“同一画布像素”保持在同一屏幕位置，消除闪烁/跳动感。
                const ImVec2 anchorScreen = hoveredBeforeZoom
                    ? mousePos
                    : ImVec2(panelPos.x + panelAvail.x * 0.5f, panelPos.y + panelAvail.y * 0.5f);
                const float anchorPixelX = (anchorScreen.x - imagePos.x) / static_cast<float>(zoom);
                const float anchorPixelY = (anchorScreen.y - imagePos.y) / static_cast<float>(zoom);

                float newImageW = 0.0f;
                float newImageH = 0.0f;
                ImVec2 newCenterOffset(0.0f, 0.0f);
                ImVec2 newImagePos(0.0f, 0.0f);
                computeImageMetrics(newZoom, panX, panY, newImageW, newImageH, newCenterOffset, newImagePos);

                const float newPanX = anchorScreen.x
                    - panelPos.x
                    - newCenterOffset.x
                    - anchorPixelX * static_cast<float>(newZoom);
                const float newPanY = anchorScreen.y
                    - panelPos.y
                    - newCenterOffset.y
                    - anchorPixelY * static_cast<float>(newZoom);

                context->setCanvasZoom(newZoom);
                context->setCanvasPan(newPanX, newPanY);
                zoom = context->getCanvasZoom();
                panX = context->getCanvasPanX();
                panY = context->getCanvasPanY();
                computeImageMetrics(zoom, panX, panY, imageW, imageH, centerOffset, imagePos);
            }
        }
    }

    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
    {
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        context->addCanvasPan(delta.x, delta.y);
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 imageMin = imagePos;
    const ImVec2 imageMax(imagePos.x + imageW, imagePos.y + imageH);

    if (context->isCheckerboardBackgroundEnabled())
    {
        const ImU32 c1 = IM_COL32(70, 70, 70, 255);
        const ImU32 c2 = IM_COL32(90, 90, 90, 255);
        // 背景按“固定画布像素块”绘制：
        // - 16x16 画布 -> 1x1 背景块；
        // - 32x32 画布 -> 2x2 背景块；
        // - 以此类推（每 16 画布像素为一个背景块）。
        const int tilePixels = 16;
        for (int ty = 0; ty < height; ty += tilePixels)
        {
            for (int tx = 0; tx < width; tx += tilePixels)
            {
                const int nextX = std::min(width, tx + tilePixels);
                const int nextY = std::min(height, ty + tilePixels);
                const int tileX = tx / tilePixels;
                const int tileY = ty / tilePixels;
                const ImU32 col = ((tileX + tileY) % 2 == 0) ? c1 : c2;
                const ImVec2 p0(
                    imageMin.x + static_cast<float>(tx * zoom),
                    imageMin.y + static_cast<float>(ty * zoom));
                const ImVec2 p1(
                    imageMin.x + static_cast<float>(nextX * zoom),
                    imageMin.y + static_cast<float>(nextY * zoom));
                drawList->AddRectFilled(p0, p1, col);
            }
        }
    }
    else
    {
        drawList->AddRectFilled(imageMin, imageMax, IM_COL32(255, 255, 255, 255));
    }

    // 洋葱皮功能：渲染前后帧的半透明预览
    if (context->isOnionSkinEnabled() && frameCount > 1)
    {
        // 获取洋葱皮设置
        const int previousFrames = context->getOnionSkinPreviousFrames();
        const int nextFrames = context->getOnionSkinNextFrames();
        const int basePreviousAlpha = context->getOnionSkinPreviousAlpha();
        const int baseNextAlpha = context->getOnionSkinNextAlpha();
        const uint32_t previousColor = context->getOnionSkinPreviousColor();
        const uint32_t nextColor = context->getOnionSkinNextColor();

        // 提取前帧颜色通道
        const int prevR = static_cast<int>(previousColor & 0xFFu);
        const int prevG = static_cast<int>((previousColor >> 8) & 0xFFu);
        const int prevB = static_cast<int>((previousColor >> 16) & 0xFFu);

        // 提取后帧颜色通道
        const int nextR = static_cast<int>(nextColor & 0xFFu);
        const int nextG = static_cast<int>((nextColor >> 8) & 0xFFu);
        const int nextB = static_cast<int>((nextColor >> 16) & 0xFFu);

        // 渲染之前的帧
        for (int i = 1; i <= previousFrames; ++i)
        {
            const int prevFrameIndex = frameIndex - i;
            if (prevFrameIndex < 0) break;

            const Project::Frame& prevFrame = project->getFrame(prevFrameIndex);
            // 透明度渐变：离当前帧越远，透明度越低
            const float alphaFactor = static_cast<float>(previousFrames - i + 1) / static_cast<float>(previousFrames + 1);
            const int alpha = static_cast<int>(basePreviousAlpha * alphaFactor);

            // 渲染前帧的半透明像素
            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    const size_t index = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
                    const uint32_t pixel = prevFrame.pixels[index];
                    if (pixel == 0) continue; // 跳过透明像素

                    // 提取像素颜色并与前帧颜色混合
                    const int r = static_cast<int>(pixel & 0xFFu);
                    const int g = static_cast<int>((pixel >> 8) & 0xFFu);
                    const int b = static_cast<int>((pixel >> 16) & 0xFFu);

                    // 颜色混合：使用前帧颜色作为滤镜
                    const int mixedR = (r + prevR) / 2;
                    const int mixedG = (g + prevG) / 2;
                    const int mixedB = (b + prevB) / 2;
                    const ImU32 color = IM_COL32(mixedR, mixedG, mixedB, alpha);

                    const ImVec2 p0(
                        imagePos.x + static_cast<float>(x * zoom),
                        imagePos.y + static_cast<float>(y * zoom));
                    const ImVec2 p1(
                        imagePos.x + static_cast<float>((x + 1) * zoom),
                        imagePos.y + static_cast<float>((y + 1) * zoom));
                    drawList->AddRectFilled(p0, p1, color);
                }
            }
        }

        // 渲染之后的帧
        for (int i = 1; i <= nextFrames; ++i)
        {
            const int nextFrameIndex = frameIndex + i;
            if (nextFrameIndex >= frameCount) break;

            const Project::Frame& nextFrame = project->getFrame(nextFrameIndex);
            // 透明度渐变：离当前帧越远，透明度越低
            const float alphaFactor = static_cast<float>(nextFrames - i + 1) / static_cast<float>(nextFrames + 1);
            const int alpha = static_cast<int>(baseNextAlpha * alphaFactor);

            // 渲染后帧的半透明像素
            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    const size_t index = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
                    const uint32_t pixel = nextFrame.pixels[index];
                    if (pixel == 0) continue; // 跳过透明像素

                    // 提取像素颜色并与后帧颜色混合
                    const int r = static_cast<int>(pixel & 0xFFu);
                    const int g = static_cast<int>((pixel >> 8) & 0xFFu);
                    const int b = static_cast<int>((pixel >> 16) & 0xFFu);

                    // 颜色混合：使用后帧颜色作为滤镜
                    const int mixedR = (r + nextR) / 2;
                    const int mixedG = (g + nextG) / 2;
                    const int mixedB = (b + nextB) / 2;
                    const ImU32 color = IM_COL32(mixedR, mixedG, mixedB, alpha);

                    const ImVec2 p0(
                        imagePos.x + static_cast<float>(x * zoom),
                        imagePos.y + static_cast<float>(y * zoom));
                    const ImVec2 p1(
                        imagePos.x + static_cast<float>((x + 1) * zoom),
                        imagePos.y + static_cast<float>((y + 1) * zoom));
                    drawList->AddRectFilled(p0, p1, color);
                }
            }
        }
    }

    drawList->AddImage(
        reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(canvasTexture_.texture)),
        imageMin,
        imageMax,
        ImVec2(0, 0),
        ImVec2(1, 1));
    // 将边框向外扩 1px，避免边框压在画布内容内侧。
    const ImVec2 borderMin(imageMin.x - 1.0f, imageMin.y - 1.0f);
    const ImVec2 borderMax(imageMax.x + 1.0f, imageMax.y + 1.0f);
    drawList->AddRect(borderMin, borderMax, IM_COL32(180, 180, 180, 255));

    if (context->isGridVisible() && zoom >= 4)
    {
        const ImU32 gridColor = IM_COL32(80, 80, 80, 120);
        for (int x = 1; x < width; ++x)
        {
            const float gx = imagePos.x + static_cast<float>(x * zoom);
            drawList->AddLine(ImVec2(gx, imagePos.y), ImVec2(gx, imagePos.y + imageH), gridColor);
        }
        for (int y = 1; y < height; ++y)
        {
            const float gy = imagePos.y + static_cast<float>(y * zoom);
            drawList->AddLine(ImVec2(imagePos.x, gy), ImVec2(imagePos.x + imageW, gy), gridColor);
        }
    }

    const bool anyPopupOpen = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
    // 矩形模式切换弹窗是“非阻塞 popup”：打开时不应禁用窗口内其它功能。
    const bool blockingPopupOpen = anyPopupOpen && !toolbarState_.rectModePopupVisible;
    const bool hovered =
        mousePos.x >= imagePos.x &&
        mousePos.y >= imagePos.y &&
        mousePos.x < (imagePos.x + imageW) &&
        mousePos.y < (imagePos.y + imageH);

    int mousePixelX = 0;
    int mousePixelY = 0;
    getClampedPixelFromMouse(mousePos, imagePos, zoom, width, height, mousePixelX, mousePixelY);

    // 粘贴预览交互：
    // - Ctrl+V/菜单 Paste 后进入该模式；
    // - 鼠标移动定位，左键确认，右键或 Esc 取消；
    // - 预览期间暂停普通绘图输入，防止误画。
    bool blockNormalToolInput = false;
    if (pastePreviewState_.active)
    {
        blockNormalToolInput = true;
        if (!pastePreviewState_.clipboard.isValid())
        {
            cancelPastePreview();
            blockNormalToolInput = false;
        }
        else
        {
            if (hovered)
            {
                pastePreviewState_.originX = mousePixelX;
                pastePreviewState_.originY = mousePixelY;
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)
                || (canvasHitboxHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)))
            {
                cancelPastePreview();
                blockNormalToolInput = false;
            }
            else if (!blockingPopupOpen && canvasHitboxHovered && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                std::string pasteError;
                if (commands::PasteSelectionCommand::execute(
                        *context,
                        pastePreviewState_.clipboard,
                        &pasteError,
                        pastePreviewState_.originX,
                        pastePreviewState_.originY))
                {
                    if (context->hasMultiFrameSelection()) context->setSingleFrameSelection(frameIndex, frameCount);
                    context->setProjectDirty(true, "Paste");
                }
                cancelPastePreview();
                blockNormalToolInput = false;
            }
        }
    }

    // 将矩形框选工具作为独立类处理输入与叠加渲染。
    if (!blockNormalToolInput && context->getTool() == ToolType::RectSelection)
    {
        bool selectionPixelsChanged = false;
        rectSelectionTool_.handleInteraction(
            *context,
            frame,
            mousePos,
            canvasHitboxHovered,
            hovered,
            blockingPopupOpen,
            imagePos,
            zoom,
            width,
            height,
            selectionPixelsChanged);

        // 框选工具对像素产生变换（平移/缩放）后，同样需要标记项目已修改。
        if (selectionPixelsChanged)
        {
            if (context->hasMultiFrameSelection()) context->setSingleFrameSelection(frameIndex, frameCount);
            context->setProjectDirty(true, "Selection Transform");
        }
    }
    else
    {
        // 切换到其它工具时，清理框选交互临时态，避免残留拖拽预览。
        rectSelectionTool_.resetInteractionState();
    }

    // 将直线工具作为独立类处理输入与实时预览。
    if (!blockNormalToolInput && context->getTool() == ToolType::Line)
    {
        bool linePixelsCommitted = false;
        lineTool_.handleInteraction(
            *context,
            frame,
            canvasHitboxHovered,
            hovered,
            blockingPopupOpen,
            mousePixelX,
            mousePixelY,
            width,
            height,
            linePixelsCommitted);
        if (linePixelsCommitted)
        {
            if (context->hasMultiFrameSelection()) context->setSingleFrameSelection(frameIndex, frameCount);
            context->setProjectDirty(true, "Line");
        }
    }
    else
    {
        // 切换到其它工具时，清理直线工具预览状态并恢复快照（若有）。
        lineTool_.resetInteractionState(&frame);
    }

    // 将矩形描边工具作为独立类处理输入与实时预览。
    if (!blockNormalToolInput && context->getTool() == ToolType::Rect)
    {
        bool rectPixelsCommitted = false;
        rectangleTool_.handleInteraction(
            *context,
            frame,
            canvasHitboxHovered,
            hovered,
            blockingPopupOpen,
            mousePixelX,
            mousePixelY,
            width,
            height,
            rectPixelsCommitted);
        if (rectPixelsCommitted)
        {
            if (context->hasMultiFrameSelection()) context->setSingleFrameSelection(frameIndex, frameCount);
            context->setProjectDirty(true, "Rectangle");
        }
    }
    else
    {
        rectangleTool_.resetInteractionState(&frame);
    }

    // 将填充矩形工具作为独立类处理输入与实时预览。
    if (!blockNormalToolInput && context->getTool() == ToolType::RectFilled)
    {
        bool rectFilledPixelsCommitted = false;
        rectFilledTool_.handleInteraction(
            *context,
            frame,
            canvasHitboxHovered,
            hovered,
            anyPopupOpen,
            mousePixelX,
            mousePixelY,
            width,
            height,
            rectFilledPixelsCommitted);
        if (rectFilledPixelsCommitted)
        {
            if (context->hasMultiFrameSelection()) context->setSingleFrameSelection(frameIndex, frameCount);
            context->setProjectDirty(true, "Filled Rectangle");
        }
    }
    else
    {
        rectFilledTool_.resetInteractionState(&frame);
    }

    // 常规像素编辑工具仅在非 RectSelection 下处理。
    const ToolType activeTool = context->getTool();
    const bool isBrushLikeTool = (activeTool == ToolType::Brush || activeTool == ToolType::Eraser);
    if (!blockNormalToolInput
        && !blockingPopupOpen
        && canvasHitboxHovered
        && hovered
        && ImGui::IsMouseDown(ImGuiMouseButton_Left)
        && context->getTool() != ToolType::RectSelection
        && context->getTool() != ToolType::Line
        && context->getTool() != ToolType::Rect
        && context->getTool() != ToolType::RectFilled)
    {
        const Tool* tool = resolveTool(context->getTool());
        if (tool)
        {
            bool changed = false;
            const bool justClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);

            if (isBrushLikeTool)
            {
                /**
                 * 连续笔划策略：
                 * - 首帧：初始化笔划锚点并正常落笔；
                 * - 后续帧：在上一点与当前点之间插值补点，避免快速拖拽断线。
                 */
                if (!strokeState_.active || justClicked || strokeState_.tool != activeTool)
                {
                    strokeState_.active = true;
                    strokeState_.changedDuringStroke = false;
                    strokeState_.lastX = mousePixelX;
                    strokeState_.lastY = mousePixelY;
                    strokeState_.tool = activeTool;
                }

                changed = applyInterpolatedStroke(
                    *tool,
                    frame,
                    width,
                    height,
                    strokeState_.lastX,
                    strokeState_.lastY,
                    mousePixelX,
                    mousePixelY,
                    *context,
                    justClicked);

                if (changed) strokeState_.changedDuringStroke = true;
                strokeState_.lastX = mousePixelX;
                strokeState_.lastY = mousePixelY;
            }
            else
            {
                // 非连续笔划工具维持原有单点输入逻辑。
                changed = tool->apply(
                    frame,
                    width,
                    height,
                    mousePixelX,
                    mousePixelY,
                    *context,
                    justClicked);
            }

            if (changed)
            {
                // 在多选状态下发生实际编辑时，自动退出多选并保留当前帧单选。
                if (context->hasMultiFrameSelection()) context->setSingleFrameSelection(frameIndex, frameCount);
                // 连续笔划工具在“松开鼠标”时一次性提交 Undo 记录，避免按像素切分历史。
                if (!isBrushLikeTool) context->setProjectDirty(true, "Paint");
            }
        }
    }
    else
    {
        // 鼠标抬起时，把本次 Brush/Eraser 连续拖拽作为一个原子操作提交历史。
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && strokeState_.active && strokeState_.changedDuringStroke)
        {
            const char* actionLabel = (strokeState_.tool == ToolType::Eraser) ? "Eraser Stroke" : "Brush Stroke";
            context->setProjectDirty(true, actionLabel);
        }

        // 鼠标抬起或切换到其它逻辑分支时，结束连续笔划会话。
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || !isBrushLikeTool)
        {
            strokeState_.active = false;
            strokeState_.changedDuringStroke = false;
        }
    }

    // 选区叠加层（包含蚂蚁线）由框选工具类负责绘制。
    rectSelectionTool_.renderOverlay(*context, drawList, imagePos, zoom, blockingPopupOpen);
    // 直线工具叠加层（拖拽辅助线）由直线工具类负责绘制。
    // lineTool_.renderOverlay(*context, drawList, imagePos, zoom, blockingPopupOpen);
    // 矩形工具叠加层（当前为空实现，预留扩展）。
    // rectangleTool_.renderOverlay(*context, drawList, imagePos, zoom, blockingPopupOpen);
    // rectFilledTool_.renderOverlay(*context, drawList, imagePos, zoom, blockingPopupOpen);

    // 粘贴预览叠加层：显示剪贴板像素的半透明结果与目标边界。
    if (pastePreviewState_.active && pastePreviewState_.clipboard.isValid())
    {
        drawList->PushClipRect(imageMin, imageMax, true);
        const commands::PixelClipboardData& clip = pastePreviewState_.clipboard;
        for (int ly = 0; ly < clip.height; ++ly)
        {
            const int dy = pastePreviewState_.originY + ly;
            if (dy < 0 || dy >= height) continue;

            for (int lx = 0; lx < clip.width; ++lx)
            {
                const size_t srcIndex = static_cast<size_t>(ly) * static_cast<size_t>(clip.width) + static_cast<size_t>(lx);
                if (clip.mask[srcIndex] == 0) continue;

                const int dx = pastePreviewState_.originX + lx;
                if (dx < 0 || dx >= width) continue;

                const ImVec2 p0(
                    imagePos.x + static_cast<float>(dx * zoom),
                    imagePos.y + static_cast<float>(dy * zoom));
                const ImVec2 p1(
                    imagePos.x + static_cast<float>((dx + 1) * zoom),
                    imagePos.y + static_cast<float>((dy + 1) * zoom));
                drawList->AddRectFilled(p0, p1, toImGuiColorWithAlpha(clip.pixels[srcIndex], 170));
            }
        }

        const ImVec2 previewMin(
            imagePos.x + static_cast<float>(pastePreviewState_.originX * zoom),
            imagePos.y + static_cast<float>(pastePreviewState_.originY * zoom));
        const ImVec2 previewMax(
            imagePos.x + static_cast<float>((pastePreviewState_.originX + clip.width) * zoom),
            imagePos.y + static_cast<float>((pastePreviewState_.originY + clip.height) * zoom));
        drawList->AddRect(previewMin, previewMax, IM_COL32(255, 255, 255, 220), 0.0f, 0, 1.0f);
        drawList->PopClipRect();

        const ImVec2 tipPos(imageMin.x + 8.0f, imageMin.y + 8.0f);
        drawList->AddText(tipPos, IM_COL32(255, 255, 255, 220), "Paste Preview: LMB Apply, RMB/Esc Cancel");
    }

    // 鼠标高亮框（弹窗期间隐藏）。
    if (!pastePreviewState_.active
        && !blockingPopupOpen
        && canvasHitboxHovered
        && hovered
        && context->getTool() != ToolType::RectSelection)
    {
        // 高亮预览与笔刷大小联动：
        // - Brush / Eraser / Line 使用 brushSize（与实际落笔区域一致）；
        // - 其他工具维持 1 像素高亮。
        int previewRadius = 0;
        const ToolType activeTool = context->getTool();
        if (activeTool == ToolType::Brush || activeTool == ToolType::Eraser || activeTool == ToolType::Line) previewRadius = std::max(0, context->getBrushSize() - 1);

        const int minX = std::max(0, mousePixelX - previewRadius);
        const int maxX = std::min(width - 1, mousePixelX + previewRadius);
        const int minY = std::max(0, mousePixelY - previewRadius);
        const int maxY = std::min(height - 1, mousePixelY + previewRadius);

        const ImVec2 hlMin(imagePos.x + static_cast<float>(minX * zoom), imagePos.y + static_cast<float>(minY * zoom));
        const ImVec2 hlMax(imagePos.x + static_cast<float>((maxX + 1) * zoom), imagePos.y + static_cast<float>((maxY + 1) * zoom));
        const ToolType currentTool = context->getTool();
        const uint32_t currentColor = context->getColorRGBA();
        // 这里增加裁剪区，确保边界像素高亮不会在画布外出现“细线/毛边”。
        drawList->PushClipRect(imageMin, imageMax, true);
        if (currentTool == ToolType::Eraser)
        {
            // 橡皮擦高亮：仅绘制黑色边框，不做填充。
            drawList->AddRect(hlMin, hlMax, IM_COL32(0, 0, 0, 255), 0.0f, 0, 1.0f);
        }
        else
        {
            // 其它工具：使用当前所选颜色进行实心填充高亮。
            drawList->AddRectFilled(hlMin, hlMax, toImGuiColor(currentColor));
        }
        drawList->PopClipRect();
    }
}


