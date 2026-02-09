/**
 * @file AppContext.h
 * @brief 应用程序/编辑器全局上下文
 *
 * 集中持有当前项目、选区、工具、视图等状态，供菜单、窗口、命令统一读写。
 * 所有 UI 与业务逻辑通过 AppContext 访问“当前状态”，避免数据分散与不一致。
 */

#pragma once

#include <cstdint>
#include <string>

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
    Line,          // 直线
    Rect,          // 矩形
    RectFilled,    // 填充矩形
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
    AppContext();
    ~AppContext();

    // -------------------------------------------------------------------------
    // 项目与文档
    // -------------------------------------------------------------------------

    // 获取当前项目指针；无打开项目时返回 nullptr
    Project* getProject() const { return project_; }

    // 设置当前项目（不负责释放旧项目，由调用方管理生命周期）
    void setProject(Project* project) { project_ = project; }

    // 是否有打开的项目
    bool hasProject() const { return project_ != nullptr; }

    /**
     * @brief 项目是否自上次保存后有修改
     * 用于窗口标题显示 *、退出时提示保存等
     */
    bool isProjectDirty() const { return projectDirty_; }

    // 标记项目已修改
    void setProjectDirty(bool dirty = true) { projectDirty_ = dirty; }

    // 当前项目文件路径（未保存或新建时为空）
    const std::string& getProjectFilePath() const { return projectFilePath_; }

    // 设置当前项目文件路径（保存/另存为/打开后更新）
    void setProjectFilePath(const std::string& path) { projectFilePath_ = path; }

    // -------------------------------------------------------------------------
    // 动画与帧
    // -------------------------------------------------------------------------

    // 当前选中的动画索引（多动画时使用，MVP 可固定为 0）
    int getCurrentAnimationIndex() const { return currentAnimationIndex_; }

    // 设置当前动画索引；调用方需保证 0 <= index < 动画数量 
    void setCurrentAnimationIndex(int index) { currentAnimationIndex_ = index; }

    // 当前选中的帧索引（时间线、画布编辑的目标帧）
    int getCurrentFrameIndex() const { return currentFrameIndex_; }

    // 设置当前帧索引；调用方需保证 0 <= index < 帧数量
    void setCurrentFrameIndex(int index) { currentFrameIndex_ = index; }

    // -------------------------------------------------------------------------
    // 绘图工具与颜色
    // -------------------------------------------------------------------------

    // 当前选中的工具
    ToolType getTool() const { return tool_; }

    // 设置当前工具（由工具栏、快捷键调用）
    void setTool(ToolType tool) { tool_ = tool; }

    /**
     * @brief 当前前景色，RGBA8888 格式（R 低字节，A 高字节）
     * 与 ImGui 颜色选择器、吸管工具同步
     */
    uint32_t getColorRGBA() const { return colorRGBA_; }

    // 设置前景色（RGBA8888）
    void setColorRGBA(uint32_t rgba) { colorRGBA_ = rgba; }

    // 画笔半径（像素），1/2/3 等，供 Brush/Eraser 使用
    int getBrushSize() const { return brushSize_; }

    // 设置画笔半径
    void setBrushSize(int size);

    // -------------------------------------------------------------------------
    // 画布视图（缩放与平移）
    // -------------------------------------------------------------------------

    // 画布缩放倍率（整数倍，如 1/2/4/8）
    int getCanvasZoom() const { return canvasZoom_; }

    // 设置画布缩放；建议限制在 [1, 2, 4, 8, 16] 等 
    void setCanvasZoom(int zoom);

    // 画布平移 X（像素，屏幕空间）
    float getCanvasPanX() const { return canvasPanX_; }

    // 画布平移 Y（像素，屏幕空间）
    float getCanvasPanY() const { return canvasPanY_; }

    // 设置画布平移
    void setCanvasPan(float x, float y) { canvasPanX_ = x; canvasPanY_ = y; }

    // 叠加平移量（用于鼠标拖拽平移）
    void addCanvasPan(float dx, float dy) { canvasPanX_ += dx; canvasPanY_ += dy; }

    // -------------------------------------------------------------------------
    // 撤销/重做
    // -------------------------------------------------------------------------

    // 获取撤销重做栈；未初始化时返回 nullptr
    CommandStack* getCommandStack() const { return commandStack_; }

    // 设置命令栈（由 App 或初始化逻辑创建并传入）
    void setCommandStack(CommandStack* stack) { commandStack_ = stack; }

    // 是否可撤销
    bool canUndo() const;

    // 是否可重做
    bool canRedo() const;

    // 执行一次撤销；内部调用 CommandStack::Undo()
    void undo();

    // 执行一次重做；内部调用 CommandStack::Redo()
    void redo();

    // -------------------------------------------------------------------------
    // 视图/UI 状态（可选，供 View 菜单、面板显隐使用）
    // -------------------------------------------------------------------------

    // 是否显示网格线
    bool isGridVisible() const { return gridVisible_; }

    // 设置网格线显隐
    void setGridVisible(bool visible) { gridVisible_ = visible; }

    // 是否开启洋葱皮
    bool isOnionSkinEnabled() const { return onionSkinEnabled_; }

    // 设置洋葱皮开关
    void setOnionSkinEnabled(bool enabled) { onionSkinEnabled_ = enabled; }

    // 是否显示时间线面板
    bool isTimelineVisible() const { return timelineVisible_; }

    // 设置时间线面板显隐
    void setTimelineVisible(bool visible) { timelineVisible_ = visible; }

private:
    // 项目与文档
    Project* project_ = nullptr;
    bool projectDirty_ = false;
    std::string projectFilePath_;

    // 动画与帧
    int currentAnimationIndex_ = 0;
    int currentFrameIndex_ = 0;

    // 工具与颜色
    ToolType tool_ = ToolType::Brush;
    uint32_t colorRGBA_ = 0xFF000000;  // 默认不透明黑
    int brushSize_ = 1;

    // 画布视图
    int canvasZoom_ = 4;       // 默认 4 倍
    float canvasPanX_ = 0.0f;
    float canvasPanY_ = 0.0f;

    // 撤销/重做（不拥有所有权，由外部创建与释放）
    CommandStack* commandStack_ = nullptr;

    // 视图选项
    bool gridVisible_ = false;
    bool onionSkinEnabled_ = false;
    bool timelineVisible_ = true;
};
