#include "ProjectWindow.h"

#include "core/AppContext.h"
#include "core/Project.h"
#include "imgui.h"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <utility>
#include <vector>

namespace
{
void paintAt(Project::Frame& frame, int canvasW, int canvasH, int x, int y, int brushSize, uint32_t color)
{
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

void floodFill(Project::Frame& frame, int width, int height, int startX, int startY, uint32_t newColor)
{
    if (startX < 0 || startY < 0 || startX >= width || startY >= height)
        return;

    const size_t startIndex = static_cast<size_t>(startY) * static_cast<size_t>(width) + static_cast<size_t>(startX);
    const uint32_t oldColor = frame.pixels[startIndex];
    if (oldColor == newColor)
        return;

    std::deque<std::pair<int, int>> q;
    q.emplace_back(startX, startY);
    frame.pixels[startIndex] = newColor;

    const int dx[4] = {1, -1, 0, 0};
    const int dy[4] = {0, 0, 1, -1};

    while (!q.empty())
    {
        const auto [x, y] = q.front();
        q.pop_front();

        for (int i = 0; i < 4; ++i)
        {
            const int nx = x + dx[i];
            const int ny = y + dy[i];
            if (nx < 0 || ny < 0 || nx >= width || ny >= height)
                continue;

            const size_t nIdx = static_cast<size_t>(ny) * static_cast<size_t>(width) + static_cast<size_t>(nx);
            if (frame.pixels[nIdx] != oldColor)
                continue;

            frame.pixels[nIdx] = newColor;
            q.emplace_back(nx, ny);
        }
    }
}
} // namespace

void ProjectWindow::renderCanvasPanel(Project* project)
{
    const int width = project->getWidth();
    const int height = project->getHeight();
    int zoom = context->getCanvasZoom();
    const int frameIndex = context->getCurrentFrameIndex();
    const int frameCount = project->getFrameCount();
    ImGui::Text("Canvas  %dx%d   Zoom %dx   Frame %d/%d", width, height, zoom, frameIndex + 1, frameCount);
    ImGui::Separator();

    Project::Frame& frame = project->getFrame(frameIndex);
    ensureCanvasTexture(width, height);
    uploadCanvasPixels(frame.pixels);

    const ImVec2 panelPos = ImGui::GetCursorScreenPos();
    const ImVec2 panelAvail = ImGui::GetContentRegionAvail();
    const float imageW = static_cast<float>(width * zoom);
    const float imageH = static_cast<float>(height * zoom);
    const ImVec2 centerOffset((panelAvail.x - imageW) * 0.5f, (panelAvail.y - imageH) * 0.5f);
    const float panX = context->getCanvasPanX();
    const float panY = context->getCanvasPanY();
    const ImVec2 imagePos(panelPos.x + centerOffset.x + panX, panelPos.y + centerOffset.y + panY);

    const ImVec2 hitboxSize(std::max(1.0f, panelAvail.x), std::max(1.0f, panelAvail.y));
    ImGui::InvisibleButton(
        "##CanvasHitbox",
        hitboxSize,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle | ImGuiButtonFlags_MouseButtonRight);

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

    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
    {
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        context->addCanvasPan(delta.x, delta.y);
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 imageMin = imagePos;
    const ImVec2 imageMax = ImVec2(imagePos.x + imageW, imagePos.y + imageH);

    if (context->isCheckerboardBackgroundEnabled())
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
    else
    {
        drawList->AddRectFilled(imageMin, imageMax, IM_COL32(255, 255, 255, 255));
    }

    drawList->AddImage(
        reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(canvasTexture_.texture)),
        imageMin,
        imageMax,
        ImVec2(0, 0),
        ImVec2(1, 1));
    drawList->AddRect(imageMin, imageMax, IM_COL32(180, 180, 180, 255));

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

    const ImVec2 mousePos = ImGui::GetMousePos();
    const bool hovered =
        mousePos.x >= imagePos.x &&
        mousePos.y >= imagePos.y &&
        mousePos.x < (imagePos.x + imageW) &&
        mousePos.y < (imagePos.y + imageH);

    if (hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        const float localX = mousePos.x - imagePos.x;
        const float localY = mousePos.y - imagePos.y;
        const int pixelX = std::clamp(static_cast<int>(localX / zoom), 0, width - 1);
        const int pixelY = std::clamp(static_cast<int>(localY / zoom), 0, height - 1);

        const ToolType tool = context->getTool();
        if (tool == ToolType::Eyedropper)
        {
            const size_t idx = static_cast<size_t>(pixelY) * static_cast<size_t>(width) + static_cast<size_t>(pixelX);
            context->setColorRGBA(frame.pixels[idx]);
        }
        else if (tool == ToolType::Fill && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            floodFill(frame, width, height, pixelX, pixelY, context->getColorRGBA());
            context->setProjectDirty(true);
        }
        else
        {
            uint32_t paintColor = context->getColorRGBA();
            if (tool == ToolType::Eraser)
                paintColor = 0x00000000;

            paintAt(frame, width, height, pixelX, pixelY, context->getBrushSize(), paintColor);
            context->setProjectDirty(true);
        }
    }

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
