#include "tools/selection/SelectionGeometry.h"

#include <algorithm>
#include <cmath>

namespace selection
{
    namespace
    {
        // 将矩形限制到画布范围内；完全无效时返回 false。
        bool clampRectToCanvas(AppContext::PixelRect& rect, int canvasWidth, int canvasHeight)
        {
            if (canvasWidth <= 0 || canvasHeight <= 0) return false;

            // 先分别裁剪左上角和右下角，得到真正落在画布里的矩形范围。
            const int x0 = std::clamp(rect.x, 0, canvasWidth - 1);
            const int y0 = std::clamp(rect.y, 0, canvasHeight - 1);
            const int x1 = std::clamp(rect.x + rect.width - 1, 0, canvasWidth - 1);
            const int y1 = std::clamp(rect.y + rect.height - 1, 0, canvasHeight - 1);
            if (x1 < x0 || y1 < y0) return false;

            // 把裁剪后的两个角重新写回 x/y/width/height 形式。
            rect.x = x0;
            rect.y = y0;
            rect.width = x1 - x0 + 1;
            rect.height = y1 - y0 + 1;
            return true;
        }

        // 按 0.5 向上取整，保持像素椭圆边界的镜像对称。
        int roundHalfUp(double value)
        {
            return static_cast<int>(std::floor(value + 0.5));
        }

        // 奇偶规则：从点向右发射射线，穿过边的次数为奇数则在多边形内。
        bool isPointInsidePolygonEvenOdd(const std::vector<ImVec2>& polygon, float px, float py)
        {
            if (polygon.size() < 3) return false;

            bool inside = false;
            size_t j = polygon.size() - 1;
            // 逐条检查多边形边：每穿过一次边，就在“内/外”之间切换一次。
            for (size_t i = 0; i < polygon.size(); ++i)
            {
                // 使用像素中心判断，能让填充结果更贴近像素格子的视觉直觉。
                const float xi = polygon[i].x + 0.5f;
                const float yi = polygon[i].y + 0.5f;
                const float xj = polygon[j].x + 0.5f;
                const float yj = polygon[j].y + 0.5f;

                const bool intersect = ((yi > py) != (yj > py))
                    && (px < (xj - xi) * (py - yi) / ((yj - yi) + 1e-6f) + xi);
                if (intersect) inside = !inside;
                j = i;
            }

            return inside;
        }
    }

    bool maskHasAnySelected(const std::vector<uint8_t>& mask)
    {
        // 只要找到一个 1，就说明当前确实有选区。
        return std::find(mask.begin(), mask.end(), static_cast<uint8_t>(1)) != mask.end();
    }

    void applyRectOpToMask(std::vector<uint8_t>& ioMask,
                           int canvasWidth,
                           int canvasHeight,
                           const AppContext::PixelRect& rect,
                           AppContext::PixelSelectionOp op)
    {
        // 无效参数
        if (ioMask.size() != static_cast<size_t>(canvasWidth) * static_cast<size_t>(canvasHeight)) return;
        if (canvasWidth <= 0 || canvasHeight <= 0 || rect.width <= 0 || rect.height <= 0) return;

        // 用户可能从画布外拖进来，所以先把矩形裁到画布范围内。
        AppContext::PixelRect clamped = rect;
        if (!clampRectToCanvas(clamped, canvasWidth, canvasHeight)) return;

        // Replace 表示“重新选择”，因此先清空旧选区。
        if (op == AppContext::PixelSelectionOp::Replace)
        {
            std::fill(ioMask.begin(), ioMask.end(), static_cast<uint8_t>(0));
        }

        // 遍历裁剪后的矩形区域，把每个格子按操作类型写成选中/取消选中。
        for (int y = clamped.y; y < clamped.y + clamped.height; ++y)
        {
            const size_t rowOffset = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth);
            for (int x = clamped.x; x < clamped.x + clamped.width; ++x)
            {
                const size_t idx = rowOffset + static_cast<size_t>(x);
                if (op == AppContext::PixelSelectionOp::Remove) ioMask[idx] = 0;
                else ioMask[idx] = 1;
            }
        }
    }

    void applyEllipseOpToMask(std::vector<uint8_t>& ioMask,
                              int canvasWidth,
                              int canvasHeight,
                              const AppContext::PixelRect& rect,
                              AppContext::PixelSelectionOp op)
    {
        if (ioMask.size() != static_cast<size_t>(canvasWidth) * static_cast<size_t>(canvasHeight)) return;
        if (canvasWidth <= 0 || canvasHeight <= 0 || rect.width <= 0 || rect.height <= 0) return;

        // 只裁剪最终写入范围；椭圆形状仍按原始外接矩形计算，避免贴边时被压缩。
        AppContext::PixelRect clamped = rect;
        if (!clampRectToCanvas(clamped, canvasWidth, canvasHeight)) return;

        // Replace 表示用新椭圆完全替换旧选区。
        if (op == AppContext::PixelSelectionOp::Replace)
        {
            std::fill(ioMask.begin(), ioMask.end(), static_cast<uint8_t>(0));
        }

        // 原始外接矩形决定椭圆中心和半径。
        const int minX = rect.x;
        const int minY = rect.y;
        const int maxX = rect.x + rect.width - 1;
        const int maxY = rect.y + rect.height - 1;

        // 宽度为 1 时，椭圆退化成竖线。
        if (rect.width == 1)
        {
            for (int y = clamped.y; y < clamped.y + clamped.height; ++y)
            {
                const size_t idx = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth) + static_cast<size_t>(clamped.x);
                if (op == AppContext::PixelSelectionOp::Remove) ioMask[idx] = 0;
                else ioMask[idx] = 1;
            }
            return;
        }

        // 高度为 1 时，椭圆退化成横线。
        if (rect.height == 1)
        {
            const size_t rowOffset = static_cast<size_t>(clamped.y) * static_cast<size_t>(canvasWidth);
            for (int x = clamped.x; x < clamped.x + clamped.width; ++x)
            {
                const size_t idx = rowOffset + static_cast<size_t>(x);
                if (op == AppContext::PixelSelectionOp::Remove) ioMask[idx] = 0;
                else ioMask[idx] = 1;
            }
            return;
        }

        const double centerX = (static_cast<double>(minX) + static_cast<double>(maxX)) * 0.5;
        const double centerY = (static_cast<double>(minY) + static_cast<double>(maxY)) * 0.5;
        const double radiusInset = 0.25;    // 
        const double radiusX = std::max(0.5, static_cast<double>(rect.width - 1) * 0.5 - radiusInset);
        const double radiusY = std::max(0.5, static_cast<double>(rect.height - 1) * 0.5 - radiusInset);

        // 每一行记录椭圆最左/最右边界，最后按行填满中间区域。
        std::vector<int> rowLeft(static_cast<size_t>(clamped.height), canvasWidth);
        std::vector<int> rowRight(static_cast<size_t>(clamped.height), -1);

        // 边界点记录函数
        auto markBoundaryPoint = [&](int x, int y)
        {
            if (y < clamped.y || y >= clamped.y + clamped.height) return;

            const size_t rowIndex = static_cast<size_t>(y - clamped.y);
            rowLeft[rowIndex] = std::min(rowLeft[rowIndex], x);
            rowRight[rowIndex] = std::max(rowRight[rowIndex], x);
        };

        // 按 X 方向采样上下边界点。
        for (int x = minX; x <= maxX; ++x)
        {
            const double normalizedX = (static_cast<double>(x) - centerX) / radiusX;
            if (std::abs(normalizedX) > 1.0) continue;

            const double yOffset = radiusY * std::sqrt(std::max(0.0, 1.0 - normalizedX * normalizedX));
            const int topY = roundHalfUp(centerY - yOffset);
            const int bottomY = minY + maxY - topY;
            markBoundaryPoint(x, topY);
            markBoundaryPoint(x, bottomY);
        }

        // 再按 Y 方向采样左右边界点，补齐陡峭区域的空洞。
        for (int y = minY; y <= maxY; ++y)
        {
            const double normalizedY = (static_cast<double>(y) - centerY) / radiusY;
            if (std::abs(normalizedY) > 1.0) continue;

            const double xOffset = radiusX * std::sqrt(std::max(0.0, 1.0 - normalizedY * normalizedY));
            const int leftX = roundHalfUp(centerX - xOffset);
            const int rightX = minX + maxX - leftX;
            markBoundaryPoint(leftX, y);
            markBoundaryPoint(rightX, y);
        }

        // 对每一行，把左右边界之间的像素全部写入 mask。
        for (int y = clamped.y; y < clamped.y + clamped.height; ++y)
        {
            const size_t rowIndex = static_cast<size_t>(y - clamped.y);
            if (rowRight[rowIndex] < rowLeft[rowIndex]) continue;

            const int fillStartX = std::max(clamped.x, rowLeft[rowIndex]);
            const int fillEndX = std::min(clamped.x + clamped.width - 1, rowRight[rowIndex]);
            if (fillEndX < fillStartX) continue;

            const size_t rowOffset = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth);
            for (int x = fillStartX; x <= fillEndX; ++x)
            {
                const size_t idx = rowOffset + static_cast<size_t>(x);
                if (op == AppContext::PixelSelectionOp::Remove) ioMask[idx] = 0;
                else ioMask[idx] = 1;
            }
        }
    }

    void mergeMaskIntoSelection(std::vector<uint8_t>& targetMask,
                                const std::vector<uint8_t>& sourceMask,
                                AppContext::PixelSelectionOp operation)
    {
        if (targetMask.size() != sourceMask.size()) return;

        // Replace 的含义是“先清空旧选区，再把 sourceMask 写进去”。
        if (operation == AppContext::PixelSelectionOp::Replace)
        {
            std::fill(targetMask.begin(), targetMask.end(), static_cast<uint8_t>(0));
        }

        // sourceMask 里为 1 的地方才参与合并；为 0 的地方不影响目标选区。
        for (size_t i = 0; i < targetMask.size(); ++i)
        {
            if (sourceMask[i] == 0) continue;
            if (operation == AppContext::PixelSelectionOp::Remove) targetMask[i] = 0;
            else targetMask[i] = 1;
        }
    }

    void appendLineToPath(std::vector<ImVec2>& path, int x0, int y0, int x1, int y1)
    {
        // Bresenham 整数画线：不使用浮点插值，保证经过的是实际像素格。
        int dx = std::abs(x1 - x0);
        int sx = (x0 < x1) ? 1 : -1;
        int dy = -std::abs(y1 - y0);
        int sy = (y0 < y1) ? 1 : -1;
        int err = dx + dy;
        int x = x0;
        int y = y0;

        while (true)
        {
            // 避免重复压入同一个点，路径更干净。
            if (path.empty()
                || static_cast<int>(path.back().x) != x
                || static_cast<int>(path.back().y) != y)
            {
                path.emplace_back(static_cast<float>(x), static_cast<float>(y));
            }
            if (x == x1 && y == y1) break;

            // 根据误差项决定下一步走 X、走 Y，或斜着两个方向都走。
            const int e2 = err * 2;
            if (e2 >= dy)
            {
                err += dy;
                x += sx;
            }
            if (e2 <= dx)
            {
                err += dx;
                y += sy;
            }
        }
    }

    bool buildLassoMaskFromPath(const std::vector<ImVec2>& inputPath,
                                int canvasWidth,
                                int canvasHeight,
                                std::vector<uint8_t>& outMask)
    {
        outMask.assign(static_cast<size_t>(canvasWidth) * static_cast<size_t>(canvasHeight), static_cast<uint8_t>(0));
        if (canvasWidth <= 0 || canvasHeight <= 0 || inputPath.size() < 2) return false;

        // 套索需要闭合区域：如果用户松手点不在起点，就自动补一条回到起点的线。
        std::vector<ImVec2> polygon = inputPath;
        const int firstX = static_cast<int>(polygon.front().x);
        const int firstY = static_cast<int>(polygon.front().y);
        const int lastX = static_cast<int>(polygon.back().x);
        const int lastY = static_cast<int>(polygon.back().y);
        if (firstX != lastX || firstY != lastY) appendLineToPath(polygon, lastX, lastY, firstX, firstY);
        if (polygon.size() < 3) return false;

        // 先算路径包围盒，后面只扫描这块区域，避免整张画布都做点内判断。
        int minX = canvasWidth - 1;
        int minY = canvasHeight - 1;
        int maxX = 0;
        int maxY = 0;
        bool hasValid = false;
        for (const ImVec2& p : polygon)
        {
            const int x = std::clamp(static_cast<int>(p.x), 0, canvasWidth - 1);
            const int y = std::clamp(static_cast<int>(p.y), 0, canvasHeight - 1);
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
            hasValid = true;
        }
        if (!hasValid) return false;

        // 检查包围盒中的每个像素中心是否在多边形内，在就标成选中。
        for (int y = minY; y <= maxY; ++y)
        {
            const size_t rowOffset = static_cast<size_t>(y) * static_cast<size_t>(canvasWidth);
            for (int x = minX; x <= maxX; ++x)
            {
                const float cx = static_cast<float>(x) + 0.5f;
                const float cy = static_cast<float>(y) + 0.5f;
                if (isPointInsidePolygonEvenOdd(polygon, cx, cy)) outMask[rowOffset + static_cast<size_t>(x)] = 1;
            }
        }

        // 边界像素也标记为选中，保证轮廓不会断裂。
        for (const ImVec2& p : polygon)
        {
            const int x = std::clamp(static_cast<int>(p.x), 0, canvasWidth - 1);
            const int y = std::clamp(static_cast<int>(p.y), 0, canvasHeight - 1);
            outMask[static_cast<size_t>(y) * static_cast<size_t>(canvasWidth) + static_cast<size_t>(x)] = 1;
        }

        return maskHasAnySelected(outMask);
    }

    bool isCloseToFirstVertex(const std::vector<ImVec2>& vertices, int x, int y, int thresholdPixels)
    {
        if (vertices.empty()) return false;

        // 使用方形距离判断即可：对像素工具来说直观、便宜，也方便点击闭合。
        const int fx = static_cast<int>(vertices.front().x);
        const int fy = static_cast<int>(vertices.front().y);
        return std::abs(fx - x) <= thresholdPixels && std::abs(fy - y) <= thresholdPixels;
    }

    bool buildPolygonMaskFromVertices(const std::vector<ImVec2>& vertices,
                                      int canvasWidth,
                                      int canvasHeight,
                                      std::vector<uint8_t>& outMask)
    {
        if (vertices.size() < 3) return false;

        // 多边形套索记录的是顶点；这里先把相邻顶点之间补成连续像素边。
        std::vector<ImVec2> pathPixels;
        pathPixels.reserve(vertices.size() * 4);
        pathPixels.push_back(vertices.front());

        for (size_t i = 1; i < vertices.size(); ++i)
        {
            const int x0 = static_cast<int>(vertices[i - 1].x);
            const int y0 = static_cast<int>(vertices[i - 1].y);
            const int x1 = static_cast<int>(vertices[i].x);
            const int y1 = static_cast<int>(vertices[i].y);
            appendLineToPath(pathPixels, x0, y0, x1, y1);
        }

        // 最后一条边：从最后一个顶点连回第一个顶点，形成闭合多边形。
        const int lx = static_cast<int>(vertices.back().x);
        const int ly = static_cast<int>(vertices.back().y);
        const int fx = static_cast<int>(vertices.front().x);
        const int fy = static_cast<int>(vertices.front().y);
        appendLineToPath(pathPixels, lx, ly, fx, fy);

        return buildLassoMaskFromPath(pathPixels, canvasWidth, canvasHeight, outMask);
    }
}
