/**
 * @file AppContext.h
 * @brief 应用程序/编辑器全局上下文
 *
 * 集中持有当前项目、选区、工具、视图等状态，供菜单、窗口、命令统一读写。
 * 所有 UI 与业务逻辑通过 AppContext 访问“当前状态”，避免数据分散与不一致。
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// 前向声明，避免在头文件中包含尚未实现的类型，减少编译依赖与循环引用
class Project;
class CommandStack;

/**
 * @brief 当前选中的绘图工具类型
 *
 * 与左侧工具栏、快捷键一一对应，后续可在 ToolPanel 中根据此枚举高亮当前工具。
 */
enum class ToolType : int
{
    Brush = 0,     // 画笔
    Eraser,        // 橡皮擦
    Eyedropper,    // 吸管
    Fill,          // 油漆桶（区域填充）
    RectSelection, // 矩形框选
    Line,          // 直线
    Curve,         // 曲线（三次贝塞尔）
    Rect,          // 矩形
    RectFilled,    // 填充矩形
    Circle,        // 圆形描边
    CircleFilled,  // 圆形填充
    Polygon,       // 多边形描边
    PolygonFilled, // 多边形填充
    Count          // 工具数量，用于遍历与边界检查
};

/**
 * @brief 应用程序/编辑器上下文
 *
 * 职责：
 * - 持有当前打开的项目（Project*），以及当前动画索引、当前帧索引。
 * - 持有当前绘图工具、当前前景色、画布缩放与平移，供画布与工具栏同步。
 * - 持有撤销/重做栈（CommandStack*），供编辑命令与菜单 Undo/Redo 使用。
 * - 可选：持有“项目是否已修改”标记，用于退出/关闭前提示保存。
 *
 * 使用约定：
 * - 项目生命周期由外部管理（如 App 或命令），AppContext 只持有指针，不负责 new/delete。
 * - 菜单、窗口、命令通过传入的 AppContext& 读写状态，不单独缓存项目名、尺寸等。
 */
class AppContext
{
public:
    /**
     * @brief 矩形选区布尔运算模式。
     *
     * - Replace：清空旧选区后写入新矩形（替换）
     * - Add：把新矩形并入当前选区（并集）
     * - Remove：从当前选区中扣除新矩形区域（差集）
     */
    enum class PixelSelectionOp : int
    {
        Replace = 0,
        Add,
        Remove
    };

    /**
     * @brief 像素矩形（画布坐标系，左上角 + 宽高）。
     */
    struct PixelRect
    {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
    };

    /**
     * @brief 时间轴帧分组结构。
     *
     * 用途：
     * - 为“多选帧右键分组”提供数据承载；
     * - 为时间轴视觉区分提供颜色；
     * - 为精灵图分组导出提供直接数据来源。
     */
    struct FrameGroup
    {
        std::string name;              // 分组名称（用户命名）
        std::vector<int> frameIndices; // 组内帧索引（0-based）
        uint32_t colorRGBA = 0xFFFFFFFF; // 组高亮颜色（RGBA8888）
    };

    AppContext();
    ~AppContext();

    // -------------------------------------------------------------------------
    // 项目与文档
    // -------------------------------------------------------------------------

    // 获取当前项目指针；无打开项目时返回 nullptr
    Project* getProject() const 
    { 
        return m_project; 
    }

    // 设置当前项目（不负责释放旧项目，由调用方管理生命周期）
    void setProject(Project* project) 
    { 
        m_project = project; 
    }

    // 是否有打开的项目
    bool hasProject() const 
    { 
        return m_project != nullptr; 
    }

    /**
     * @brief 项目是否自上次保存后有修改
     * 用于窗口标题显示 *、退出时提示保存等
     */
    bool isProjectDirty() const 
    { 
        return m_projectDirty; 
    }

    // 标记项目已修改
    void setProjectDirty(bool dirty = true, const std::string& actionLabel = "Edit");

    // 当前项目文件路径（未保存或新建时为空）
    const std::string& getProjectFilePath() const 
    { 
        return m_projectFilePath; 
    }

    // 设置当前项目文件路径（保存/另存为/打开后更新）
    void setProjectFilePath(const std::string& path) 
    { 
        m_projectFilePath = path; 
    }

    // -------------------------------------------------------------------------
    // 动画与帧
    // -------------------------------------------------------------------------

    // 当前选中的动画索引（多动画时使用，MVP 可固定为 0）
    int getCurrentAnimationIndex() const 
    { 
        return m_currentAnimationIndex; 
    }

    // 设置当前动画索引；调用方需保证 0 <= index < 动画数量 
    void setCurrentAnimationIndex(int index) 
    { 
        m_currentAnimationIndex = index; 
    }

    // 当前选中的帧索引（时间线、画布编辑的目标帧）
    int getCurrentFrameIndex() const 
    { 
        return m_currentFrameIndex; 
    }

    // 设置当前帧索引；调用方需保证 0 <= index < 帧数量
    void setCurrentFrameIndex(int index) 
    { 
        m_currentFrameIndex = index; 
    }

    /**
     * @brief 获取当前帧多选列表。
     *
     * 约定：
     * - m_selectedFrameIndices[0] 视为“主帧”（Primary），
     *   多选状态下画布应显示这帧。
     */
    const std::vector<int>& getSelectedFrameIndices() const
    {
        return m_selectedFrameIndices;
    }

    /**
     * @brief 是否处于多选状态（选中帧数 > 1）。
     */
    bool hasMultiFrameSelection() const
    {
        return m_selectedFrameIndices.size() > 1;
    }

    /**
     * @brief 获取主选中帧索引。
     *
     * 返回策略：
     * - 若选区非空，返回 m_selectedFrameIndices[0]
     * - 否则回退到 m_currentFrameIndex
     */
    int getPrimarySelectedFrameIndex() const
    {
        if (!m_selectedFrameIndices.empty()) return m_selectedFrameIndices.front();
        return m_currentFrameIndex;
    }

    /**
     * @brief 强制设置“单选帧”。
     *
     * 常用于：
     * - 普通点击时间轴帧
     * - 画布编辑后取消多选
     * - 播放/导航按钮切帧后同步选中态
     *
     * @param frameIndex 要选中的帧索引。
     * @param frameCount 当前项目总帧数（用于边界校验）。
     */
    void setSingleFrameSelection(int frameIndex, int frameCount);

    /**
     * @brief Ctrl+点击行为：切换某一帧的选中状态。
     *
     * 规则：
     * - 已选中 -> 取消选中
     * - 未选中 -> 追加到选区末尾
     * - 至少保留一个选中帧（不会出现空选区）
     *
     * @param frameIndex 目标帧索引。
     * @param frameCount 当前项目总帧数（用于边界校验）。
     */
    void toggleFrameSelection(int frameIndex, int frameCount);

    /**
     * @brief 校正选区，移除越界帧并保证至少一个有效选中。
     *
     * @param frameCount 当前项目总帧数。
     * @param fallbackIndex 当选区为空时回退使用的帧索引。
     */
    void sanitizeFrameSelection(int frameCount, int fallbackIndex);

    /**
     * @brief 获取当前项目的帧分组列表。
     */
    const std::vector<FrameGroup>& getFrameGroups() const
    {
        return m_frameGroups;
    }

    /**
     * @brief 使用给定帧列表创建一个新分组。
     *
     * 行为约定：
     * - 会先去重并过滤越界帧；
     * - 被本次分组包含的帧，会从其他旧分组中移除（避免重叠归属混乱）；
     * - 若旧分组因此为空，会被自动删除；
     * - 新分组会追加到列表末尾。
     *
     * @param groupName 分组名称（空字符串时自动命名）。
     * @param frameIndices 分组帧列表（0-based）。
     * @param frameCount 当前总帧数（用于边界过滤）。
     * @param colorRGBA 分组颜色（RGBA8888）。
     */
    void addFrameGroup(const std::string& groupName,
                       const std::vector<int>& frameIndices,
                       int frameCount,
                       uint32_t colorRGBA);

    /**
     * @brief 按当前帧总数清理分组越界帧，并删除空分组。
     */
    void sanitizeFrameGroups(int frameCount);

    /**
     * @brief 在时间轴插入新帧后，同步更新所有分组索引。
     *
     * 规则：
     * - 所有 >= insertedFrameIndex 的索引整体 +1（因为项目帧数组后移）；
     * - 若 anchorFrameIndex 所在分组存在，则把新帧 insertedFrameIndex
     *   插入到该分组中 anchorFrameIndex 的后面，保持组内相对顺序。
     *
     * @param insertedFrameIndex 新插入帧的索引（0-based）。
     * @param anchorFrameIndex 插帧参照帧（通常是“当前帧”）。
     * @param frameCount 插帧后的总帧数（用于最终校验）。
     */
    void onFrameInserted(int insertedFrameIndex, int anchorFrameIndex, int frameCount);

    /**
     * @brief 在时间轴删除帧后，同步更新所有分组索引。
     *
     * 规则：
     * - 等于 removedFrameIndex 的分组成员被删除；
     * - 大于 removedFrameIndex 的索引整体 -1；
     * - 空分组自动清理。
     *
     * @param removedFrameIndex 被删除帧索引（0-based）。
     * @param frameCount 删帧后的总帧数（用于最终校验）。
     */
    void onFrameRemoved(int removedFrameIndex, int frameCount);

    /**
     * @brief 在时间轴拖拽换序后，同步更新选区与分组索引。
     *
     * @param fromIndex 原始索引（0-based）。
     * @param toIndex 目标索引（0-based）。
     * @param frameCount 调整后的总帧数（用于最终校验）。
     */
    void onFrameMoved(int fromIndex, int toIndex, int frameCount);

    /**
     * @brief 重命名指定分组。
     */
    void renameFrameGroup(int groupIndex, const std::string& newName);

    /**
     * @brief 删除指定分组（仅删除分组关系，不删除帧）。
     */
    void removeFrameGroup(int groupIndex);

    /**
     * @brief 清空全部帧分组。
     *
     * 说明：
     * - 仅清除“分组关系”，不会删除任何实际帧数据。
     * - 常用于导入替换、重建分组等场景。
     */
    void clearFrameGroups();

    // -------------------------------------------------------------------------
    // 绘图工具与颜色
    // -------------------------------------------------------------------------

    // 当前选中的工具
    ToolType getTool() const 
    { 
        return m_tool; 
    }

    // 设置当前工具（由工具栏、快捷键调用）
    void setTool(ToolType tool) 
    { 
        m_tool = tool; 
    }

    /**
     * @brief 当前前景色，RGBA8888 格式（R 低字节，A 高字节）
     * 与 ImGui 颜色选择器、吸管工具同步
     */
    uint32_t getColorRGBA() const 
    { 
        return m_colorRGBA; 
    }

    // 设置前景色（RGBA8888）
    void setColorRGBA(uint32_t rgba) 
    { 
        m_colorRGBA = rgba; 
    }

    // 左右对称开关：开启后，画布编辑会沿垂直中轴同步镜像。
    bool isLeftRightSymmetryEnabled() const
    {
        return m_leftRightSymmetryEnabled;
    }
    void setLeftRightSymmetryEnabled(bool enabled)
    {
        m_leftRightSymmetryEnabled = enabled;
    }

    // 上下对称开关：开启后，画布编辑会沿水平中轴同步镜像。
    bool isUpDownSymmetryEnabled() const
    {
        return m_upDownSymmetryEnabled;
    }
    void setUpDownSymmetryEnabled(bool enabled)
    {
        m_upDownSymmetryEnabled = enabled;
    }

    bool isAnySymmetryEnabled() const
    {
        return m_leftRightSymmetryEnabled || m_upDownSymmetryEnabled;
    }

    // 画笔半径（像素），1/2/3 等，供 Brush/Eraser 使用
    int getBrushSize() const 
    { 
        return m_brushSize; 
    }

    // 设置画笔半径
    void setBrushSize(int size);

    // -------------------------------------------------------------------------
    // 画布视图（缩放与平移）
    // -------------------------------------------------------------------------

    // 画布缩放倍率（整数倍，如 1/2/4/8）
    int getCanvasZoom() const 
    { 
        return m_canvasZoom; 
    }

    // 设置画布缩放；建议限制在 [1, 2, 4, 8, 16] 等 
    void setCanvasZoom(int zoom);

    // 画布平移 X（像素，屏幕空间）
    float getCanvasPanX() const 
    { 
        return m_canvasPanX; 
    }

    // 画布平移 Y（像素，屏幕空间）
    float getCanvasPanY() const 
    { 
        return m_canvasPanY; 
    }

    // 设置画布平移
    void setCanvasPan(float x, float y) 
    { 
        m_canvasPanX = x; 
        m_canvasPanY = y; 
    }

    // 叠加平移量（用于鼠标拖拽平移）
    void addCanvasPan(float dx, float dy) 
    { 
        m_canvasPanX += dx; 
        m_canvasPanY += dy; 
    }

    // -------------------------------------------------------------------------
    // 像素选区（矩形框选工具）
    // -------------------------------------------------------------------------

    /**
     * @brief 当画布尺寸变化时，确保选区掩码尺寸同步；尺寸不一致时会清空旧选区。
     */
    void ensurePixelSelectionCanvasSize(int canvasWidth, int canvasHeight);

    /**
     * @brief 是否存在任何像素被选中。
     */
    bool hasPixelSelection() const;

    /**
     * @brief 当前选区 mask 是否与指定画布尺寸匹配。
     *
     * 渲染层可用它安全复用内部 mask，避免为了显示选区每帧逐像素重新拷贝查询。
     */
    bool isPixelSelectionMaskCompatible(int canvasWidth, int canvasHeight) const;

    /**
     * @brief 获取当前选区 mask 的只读引用。
     *
     * 调用前建议先用 isPixelSelectionMaskCompatible(...) 校验尺寸。
     */
    const std::vector<uint8_t>& getPixelSelectionMask() const;

    /**
     * @brief 清空整张画布的像素选区。
     */
    void clearPixelSelection();

    /**
     * @brief 查询某个像素是否属于当前选区。
     *
     * 说明：
     * - 若当前“没有任何选区”，本函数返回 false；
     * - 画图工具应使用 canEditPixel(...)，它会在“无选区”时放行全部像素。
     */
    bool isPixelSelected(int x, int y, int canvasWidth, int canvasHeight) const;

    /**
     * @brief 判断某个像素是否允许被编辑。
     *
     * 规则：
     * - 无选区：全部可编辑；
     * - 有选区：仅选区内可编辑。
     */
    bool canEditPixel(int x, int y, int canvasWidth, int canvasHeight) const;

    /**
     * @brief 使用拖拽矩形更新选区（支持 Replace/Add/Remove）。
     *
     * @param x0 拖拽起点像素 x
     * @param y0 拖拽起点像素 y
     * @param x1 拖拽终点像素 x
     * @param y1 拖拽终点像素 y
     * @param canvasWidth 画布宽
     * @param canvasHeight 画布高
     * @param op 选区布尔操作
     * @return true 选区内容发生变化
     */
    bool applyRectPixelSelection(int x0,
                                 int y0,
                                 int x1,
                                 int y1,
                                 int canvasWidth,
                                 int canvasHeight,
                                 PixelSelectionOp op);

    /**
     * @brief 使用拖拽椭圆更新选区（支持 Replace/Add/Remove）。
     *
     * 说明：
     * - 外部通常通过“矩形框选工具的圆形模式”调用该接口；
     * - 椭圆由拖拽矩形的外接框定义，内部像素会被纳入选区；
     * - 与矩形选区一样，选区为空时会先清空旧选区再写入。
     */
    bool applyEllipsePixelSelection(int x0,
                                    int y0,
                                    int x1,
                                    int y1,
                                    int canvasWidth,
                                    int canvasHeight,
                                    PixelSelectionOp op);

    /**
     * @brief 使用“任意掩码”更新像素选区（支持 Replace/Add/Remove）。
     *
     * 说明：
     * - 该接口用于魔棒等“非矩形/非椭圆”选择工具；
     * - inputMask 尺寸必须与 canvasWidth*canvasHeight 一致；
     * - 掩码中非 0 值视为“命中像素”。
     */
    bool applyMaskPixelSelection(const std::vector<uint8_t>& inputMask,
                                 int canvasWidth,
                                 int canvasHeight,
                                 PixelSelectionOp op);

    /**
     * @brief 获取当前选区外接矩形。
     *
     * @param outRect 输出矩形；无选区时保持不变
     * @return true 成功获取（存在选区），false 表示当前无选区
     */
    bool getPixelSelectionBounds(PixelRect& outRect) const;

    /**
     * @brief 整体平移当前选区。
     *
     * 超出画布边界的像素会被裁掉。
     * @return true 选区内容发生变化
     */
    bool movePixelSelection(int dx, int dy);

    /**
     * @brief 以外接矩形几何变换方式缩放选区。
     *
     * @param fromRect 变换前外接矩形（通常为拖拽开始时）
     * @param toRect 变换后外接矩形（通常由手柄拖拽实时计算）
     * @return true 选区内容发生变化
     */
    bool transformPixelSelectionByRect(const PixelRect& fromRect,
                                       const PixelRect& toRect,
                                       bool flipX = false,
                                       bool flipY = false);

    // -------------------------------------------------------------------------
    // 撤销/重做
    // -------------------------------------------------------------------------

    // 获取撤销重做栈；未初始化时返回 nullptr
    CommandStack* getCommandStack() const 
    { 
        return m_commandStack; 
    }

    // 设置命令栈（由 App 或初始化逻辑创建并传入）
    void setCommandStack(CommandStack* stack) 
    { 
        m_commandStack = stack; 
    }

    // 是否可撤销
    bool canUndo() const;

    // 是否可重做
    bool canRedo() const;

    // 执行一次撤销；内部调用 CommandStack::Undo()
    void undo();

    // 执行一次重做；内部调用 CommandStack::Redo()
    void redo();

    /**
     * @brief 重置撤销/重做历史，并以当前状态作为基线快照。
     *
     * 使用时机：
     * - 新建项目后
     * - 打开项目后
     *
     * @param initialLabel 基线历史项显示名称。
     */
    void resetUndoRedoHistory(const std::string& initialLabel = "Initial");

    /**
     * @brief 获取 Undo History 总条目数（包含基线项）。
     */
    int getUndoHistoryCount() const;

    /**
     * @brief 获取当前历史指针位置（0-based）。
     */
    int getUndoHistoryCurrentIndex() const;

    /**
     * @brief 获取“已保存状态”对应的历史指针位置（0-based）。
     *
     * 说明：
     * - 当当前指针与已保存指针不一致时，项目视为 dirty。
     */
    int getUndoHistorySavedIndex() const;

    /**
     * @brief 获取指定历史条目的显示标签。
     */
    std::string getUndoHistoryLabel(int index) const;

    /**
     * @brief 跳转到指定历史条目（支持在 Undo History 面板中点击跳转）。
     */
    void jumpToUndoHistoryIndex(int index);

    /**
     * @brief 获取当前撤销历史最大条目数上限。
     */
    int getUndoHistoryMaxEntries() const;

    /**
     * @brief 设置撤销历史最大条目数上限。
     *
     * @param maxEntries 最大条目数（最小为 1）。
     */
    void setUndoHistoryMaxEntries(int maxEntries);

    // -------------------------------------------------------------------------
    // 视图/UI 状态（可选，供 View 菜单、面板显隐使用）
    // -------------------------------------------------------------------------

    // 是否显示网格线
    bool isGridVisible() const 
    { 
        return m_gridVisible; 
    }

    // 设置网格线显隐
    void setGridVisible(bool visible) 
    { 
        m_gridVisible = visible; 
    }

    // 是否开启洋葱皮
    bool isOnionSkinEnabled() const 
    {
        return m_onionSkinEnabled;
    }

    // 设置洋葱皮开关
    void setOnionSkinEnabled(bool enabled) 
    {
        m_onionSkinEnabled = enabled;
    }

    // 是否保留前后帧原本颜色，仅通过透明度显示洋葱皮。
    bool isOnionSkinPreserveOriginalColors() const
    {
        return m_onionSkinPreserveOriginalColors;
    }

    // 设置洋葱皮原色透明模式；关闭时使用前/后帧 tint 颜色辅助区分方向。
    void setOnionSkinPreserveOriginalColors(bool enabled)
    {
        m_onionSkinPreserveOriginalColors = enabled;
    }

    // 洋葱皮前帧显示数量
    int getOnionSkinPreviousFrames() const
    {
        return m_onionSkinPreviousFrames;
    }

    // 设置洋葱皮前帧显示数量
    void setOnionSkinPreviousFrames(int count)
    {
        m_onionSkinPreviousFrames = std::max(0, std::min(10, count));
    }

    // 洋葱皮后帧显示数量
    int getOnionSkinNextFrames() const
    {
        return m_onionSkinNextFrames;
    }

    // 设置洋葱皮后帧显示数量
    void setOnionSkinNextFrames(int count)
    {
        m_onionSkinNextFrames = std::max(0, std::min(10, count));
    }

    // 洋葱皮前帧透明度
    int getOnionSkinPreviousAlpha() const
    {
        return m_onionSkinPreviousAlpha;
    }

    // 设置洋葱皮前帧透明度
    void setOnionSkinPreviousAlpha(int alpha)
    {
        m_onionSkinPreviousAlpha = std::max(0, std::min(255, alpha));
    }

    // 洋葱皮后帧透明度
    int getOnionSkinNextAlpha() const
    {
        return m_onionSkinNextAlpha;
    }

    // 设置洋葱皮后帧透明度
    void setOnionSkinNextAlpha(int alpha)
    {
        m_onionSkinNextAlpha = std::max(0, std::min(255, alpha));
    }

    // 洋葱皮前帧颜色
    uint32_t getOnionSkinPreviousColor() const
    {
        return m_onionSkinPreviousColor;
    }

    // 设置洋葱皮前帧颜色
    void setOnionSkinPreviousColor(uint32_t color)
    {
        m_onionSkinPreviousColor = color;
    }

    // 洋葱皮后帧颜色
    uint32_t getOnionSkinNextColor() const
    {
        return m_onionSkinNextColor;
    }

    // 设置洋葱皮后帧颜色
    void setOnionSkinNextColor(uint32_t color)
    {
        m_onionSkinNextColor = color;
    }

    // 是否显示时间线面板
    bool isTimelineVisible() const 
    { 
        return m_timelineVisible; 
    }

    // 设置时间线面板显隐
    void setTimelineVisible(bool visible) 
    { 
        m_timelineVisible = visible; 
    }

    // 画布背景模式：true=棋盘背景，false=纯白背景
    bool isCheckerboardBackgroundEnabled() const
    {
        return m_checkerboardBackground;
    }

    void setCheckerboardBackgroundEnabled(bool enabled)
    {
        m_checkerboardBackground = enabled;
    }

private:
    /**
     * @brief 单条撤销历史记录。
     *
     * 采用“快照式”而非命令式：
     * - 实现简单、覆盖面广（像素、帧、分组、选区可一次性回滚）；
     * - 对 MVP 阶段非常稳妥，后续可替换为增量命令栈优化内存。
     */
    struct UndoHistoryEntry
    {
        std::string label;                       // 历史条目名称（例如 Paint / Resize Canvas）
        std::shared_ptr<Project> projectSnapshot; // 项目快照（深拷贝）
        int currentAnimationIndex = 0;           // 快照时动画索引
        int currentFrameIndex = 0;               // 快照时当前帧索引
        std::vector<int> selectedFrameIndices;   // 快照时帧多选状态
        std::vector<FrameGroup> frameGroups;     // 快照时帧分组
        int selectionCanvasWidth = 0;            // 像素选区掩码宽度
        int selectionCanvasHeight = 0;           // 像素选区掩码高度
        std::vector<uint8_t> selectionMask;      // 像素选区掩码数据
        bool selectionHasAny = false;            // 快照时是否存在像素选区
    };

    UndoHistoryEntry captureUndoHistoryEntry(const std::string& label) const;
    void applyUndoHistoryEntry(const UndoHistoryEntry& entry);
    bool isEquivalentToCurrentState(const UndoHistoryEntry& entry) const;
    void trimUndoHistoryToLimit();
    void rebuildPixelSelectionMeta();

    // 项目与文档
    Project* m_project = nullptr;
    bool m_projectDirty = false;
    std::string m_projectFilePath;

    // 动画与帧
    int m_currentAnimationIndex = 0;
    int m_currentFrameIndex = 0;
    std::vector<int> m_selectedFrameIndices = {0};
    std::vector<FrameGroup> m_frameGroups;

    // 工具与颜色
    ToolType m_tool = ToolType::Brush;
    uint32_t m_colorRGBA = 0xFF000000;  // 默认不透明黑
    int m_brushSize = 1;
    bool m_leftRightSymmetryEnabled = false;
    bool m_upDownSymmetryEnabled = false;

    // 画布视图
    int m_canvasZoom = 4;       // 默认 4 倍
    float m_canvasPanX = 0.0f;
    float m_canvasPanY = 0.0f;

    // 像素选区掩码（1 表示选中，0 表示未选中）
    int m_pixelSelectionCanvasWidth = 0;
    int m_pixelSelectionCanvasHeight = 0;
    std::vector<uint8_t> m_pixelSelectionMask;
    bool m_pixelSelectionHasAny = false;
    PixelRect m_pixelSelectionBounds;

    // 撤销/重做（不拥有所有权，由外部创建与释放）
    CommandStack* m_commandStack = nullptr;

    // 快照式撤销历史（每个项目窗口上下文独立）
    std::vector<UndoHistoryEntry> m_undoHistory;
    int m_undoHistoryCurrentIndex = -1;
    int m_undoHistorySavedIndex = -1;
    int m_undoHistoryMaxEntries = 200;

    // 视图选项
    bool m_gridVisible = false;
    bool m_onionSkinEnabled = false;
    bool m_onionSkinPreserveOriginalColors = false;
    int m_onionSkinPreviousFrames = 3;
    int m_onionSkinNextFrames = 3;
    int m_onionSkinPreviousAlpha = 50;
    int m_onionSkinNextAlpha = 50;
    uint32_t m_onionSkinPreviousColor = 0xFF0000FF; // 红色
    uint32_t m_onionSkinNextColor = 0x0000FFFF; // 蓝色
    bool m_timelineVisible = true;
    bool m_checkerboardBackground = true;
};

