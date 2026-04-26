#ifndef PROJECTWINDOW_H
#define PROJECTWINDOW_H

#include "Window.h"
#include "commands/PixelClipboardCommands.h"
#include "tools/CircleTool.h"
#include "tools/CircleFilledTool.h"
#include "tools/CurveTool.h"
#include "tools/LineTool.h"
#include "tools/RectFilledTool.h"
#include "tools/RectangleTool.h"
#include "tools/RectSelectionTool.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class AppContext;
class Project;

/**
 * @brief ProjectWindow 类继承自 Window，用于管理项目窗口的渲染和状态。
 * 
 * 该类负责处理项目窗口的各个面板（如工具栏、画布、时间轴等）的渲染逻辑，
 * 并维护相关的状态信息，例如画布纹理、调色板、时间轴播放状态等。
 */
class ProjectWindow : public Window {
public:
    /**
     * @brief 构造函数，初始化 ProjectWindow 对象。
     * 
     * @param context 应用上下文指针，用于访问全局应用状态。
     * @param windowLabel 窗口标签字符串，用于标识窗口。
     * @param onFocused 窗口获得焦点时的回调函数，默认为空。
     */
    ProjectWindow(AppContext* context,
                  const std::string& windowLabel,
                  const std::function<void(AppContext*)>& onFocused = {})
        : Window("ProjectWindow"), context(context), windowLabel_(windowLabel), onFocused_(onFocused) {}

    ~ProjectWindow() override;

    /**
     * @brief 渲染项目窗口的内容。
     * 
     * 此函数负责调用各个面板的渲染方法，完成整个窗口的绘制。
     */
    void render() override;

    /**
     * @brief 获取窗口标签。
     * 
     * @return const char* 返回窗口标签的 C 风格字符串。
     */
    const char* getWindowLabel() const { return windowLabel_.c_str(); }

    /**
     * @brief 设置窗口标签。
     * 
     * @param label 新的窗口标签字符串。
     */
    void setWindowLabel(const std::string& label) { windowLabel_ = label; }

    // 启动粘贴预览（由 App 的 Paste 命令触发）。
    void beginPastePreview(const commands::PixelClipboardData& clipboard);

    // 是否处于粘贴预览态。
    bool isPastePreviewActive() const;

    // 取消粘贴预览。
    void cancelPastePreview();

private:
    // 画布纹理状态结构体，用于存储画布纹理的相关信息。
    struct CanvasTextureState
    {
        unsigned int texture = 0; // OpenGL 纹理 ID
        int width = 0;            // 纹理宽度
        int height = 0;           // 纹理高度
    };

    // 调色板状态结构体，用于存储用户自定义调色板及选中颜色的信息。
    struct PaletteState
    {
        std::vector<uint32_t> userPalette; // 用户自定义调色板颜色列表
        int selectedIndex = 0;             // 当前选中的颜色索引
        bool selectedIsUser = false;       // 标记当前选中的颜色是否来自用户调色板
    };

    // 时间轴状态结构体，用于管理动画播放相关状态。
    struct TimelineState
    {
        float height = 200.0f;                 // 时间轴面板高度
        bool isPlaying = false;                // 播放状态标志
        bool loopEnabled = true;               // 是否启用循环播放
        float fps = 8.0f;                      // 动画帧率
        uint64_t lastTick = 0;                 // 上一次更新时间戳
        double accumulator = 0.0;              // 时间累加器，用于帧同步
        unsigned int playIconTexture = 0;      // 播放图标纹理 ID
        unsigned int pauseIconTexture = 0;     // 暂停图标纹理 ID
        bool iconsLoaded = false;              // 图标是否已加载
        bool openCreateGroupNamePopup = false; // 是否请求打开“创建分组”命名弹窗
        char pendingGroupName[64] = "Group 1"; // 分组命名输入缓存（弹窗内编辑）
        std::vector<int> pendingGroupFrames;   // 待创建分组的帧索引快照（0-based）
        bool openRenameGroupPopup = false;     // 是否请求打开“重命名分组”弹窗
        int renameGroupIndex = -1;             // 待重命名分组索引
        char renameGroupName[64] = "";         // 重命名输入缓存
        int draggingFrameIndex = -1;           // 当前拖拽源帧索引（-1 表示无拖拽）
    };

    
    // 工具栏状态结构体，用于管理工具栏图标的状态
    struct ToolbarState
    {
        bool iconsLoaded = false;                                   // 图标是否已加载
        unsigned int brushIconTexture = 0;                          // 画笔图标纹理 ID
        unsigned int eraserIconTexture = 0;                         // 橡皮擦图标纹理 ID
        unsigned int eyedropperIconTexture = 0;                     // 取色器图标纹理 ID
        unsigned int fillIconTexture = 0;                           // 填充工具图标纹理 ID
        unsigned int rectSelectIconTexture = 0;                     // 矩形框选图标纹理 ID
        unsigned int circleSelectIconTexture = 0;                   // 圆形框选图标纹理 ID
        unsigned int magicWandSelectIconTexture = 0;                // 魔棒框选图标纹理 ID
        unsigned int lassoSelectIconTexture = 0;                    // 套索框选图标纹理 ID
        unsigned int polygonLassoSelectIconTexture = 0;             // 多边形套索图标纹理 ID
        unsigned int lineIconTexture = 0;                           // 直线工具图标纹理 ID
        unsigned int curveIconTexture = 0;                          // 曲线工具图标纹理 ID
        unsigned int rectIconTexture = 0;                           // 矩形绘制工具图标纹理 ID
        unsigned int rectFilledIconTexture = 0;                     // 填充矩形工具图标纹理 ID
        unsigned int circleIconTexture = 0;                         // 圆形绘制工具图标纹理 ID
        unsigned int circleFilledIconTexture = 0;                   // 填充圆形工具图标纹理 ID
        unsigned int symmetryLeftRightIconTexture = 0;              // 左右对称工具图标纹理 ID
        unsigned int symmetryUpDownIconTexture = 0;                 // 上下对称工具图标纹理 ID
        bool selectionModePopupVisible = false;                     // 框选模式面板是否可见（长按触发，非阻塞）
        RectSelectionTool::SelectionShape lastSelectionShape = RectSelectionTool::SelectionShape::Rectangle; // 记录上一次框选模式
        bool selectionModeLongPressActive = false;                  // 当前是否处于框选按钮长按会话
        double selectionModeLongPressStart = 0.0;                   // 长按开始时间戳（ImGui::GetTime）
        float selectionModeLongPressThreshold = 0.22f;              // 触发模式面板的长按阈值（秒）
        float selectionModePanelPosX = 0.0f;                        // 模式面板屏幕 X（锚定在按钮右侧）
        float selectionModePanelPosY = 0.0f;                        // 模式面板屏幕 Y
        bool selectionModePanelHasHover = false;                    // 当前帧是否悬停在某个模式图标上
        RectSelectionTool::SelectionShape selectionModePanelHoverShape = RectSelectionTool::SelectionShape::Rectangle; // 当前悬停候选模式
        bool lineModePopupVisible = false;                          // 线工具模式面板是否可见（长按触发，非阻塞）
        ToolType lastLineMode = ToolType::Line;                     // 记录上一次线模式（Line / Curve）
        bool lineModeLongPressActive = false;                       // 当前是否处于 Line 按钮长按会话
        double lineModeLongPressStart = 0.0;                        // 长按开始时间戳
        float lineModeLongPressThreshold = 0.22f;                   // 触发模式面板的长按阈值（秒）
        float lineModePanelPosX = 0.0f;                             // 模式面板屏幕 X（锚定在按钮右侧）
        float lineModePanelPosY = 0.0f;                             // 模式面板屏幕 Y
        bool lineModePanelHasHover = false;                         // 当前帧是否悬停在线模式候选图标上
        ToolType lineModePanelHoverMode = ToolType::Line;           // 当前悬停候选线模式
        bool rectModePopupVisible = false;                          // 矩形模式面板是否可见（长按触发，非阻塞）
        ToolType lastRectMode = ToolType::Rect;                     // 记录上一次矩形模式（Rect / RectFilled / Circle / CircleFilled）
        bool rectModeLongPressActive = false;                       // 当前是否处于 Rectangle 按钮长按会话
        double rectModeLongPressStart = 0.0;                        // 长按开始时间戳（ImGui::GetTime）
        float rectModeLongPressThreshold = 0.22f;                   // 触发模式面板的长按阈值（秒）
        float rectModePanelPosX = 0.0f;                             // 模式面板屏幕 X（锚定在按钮右侧）
        float rectModePanelPosY = 0.0f;                             // 模式面板屏幕 Y
        bool rectModePanelHasHover = false;                         // 当前帧是否悬停在某个模式图标上
        ToolType rectModePanelHoverMode = ToolType::Rect;           // 当前悬停候选模式
    };

    // 图层面板状态：缓存图层操作按钮纹理，避免每帧重复从磁盘加载。
    struct LayerPanelState
    {
        bool iconsLoaded = false;
        unsigned int newLayerIconTexture = 0;       // 新建图层
        unsigned int deleteIconTexture = 0;         // 删除图层
        unsigned int upIconTexture = 0;             // 上移图层
        unsigned int downIconTexture = 0;           // 下移图层
        unsigned int showIconTexture = 0;           // 图层可见
        unsigned int hideIconTexture = 0;           // 图层隐藏
        unsigned int lockIconTexture = 0;           // 图层锁定
        unsigned int unlockIconTexture = 0;         // 图层解锁
        std::vector<int> selectedLayerIndices;      // 图层面板当前多选结果（Ctrl 点击维护）
        bool openRenamePopup = false;               // 是否请求打开重命名弹窗
        int renameLayerIndex = -1;                  // 正在重命名的图层索引
        char renameLayerName[64] = "";              // 重命名输入缓存
    };

    /**
     * @brief 连续笔划状态（用于解决快速拖拽时断线问题）。
     *
     * 说明：
     * - 当 Brush/Eraser 按住左键拖拽时，记录上一帧的像素点；
     * - 下一帧在“上一点 -> 当前点”之间做插值补点，保证笔迹连续。
     */
    struct StrokeContinuityState
    {
        bool active = false;                        // 当前是否处于连续笔划拖拽中
        bool changedDuringStroke = false;           // 本次笔划过程中是否实际修改过像素
        int lastX = 0;                              // 上一次采样到的像素 X
        int lastY = 0;                              // 上一次采样到的像素 Y
        ToolType tool = ToolType::Brush;            // 启动笔划时的工具类型（Brush/Eraser）
    };

    /**
     * @brief 对称绘制状态。
     *
     * 说明：
     * - 对称现在是“开关”而不是独立工具；
     * - 拖拽开始时缓存一份像素快照；
     * - 每帧把本次编辑相对快照产生的差异镜像到另一侧，
     *   因此线条、矩形、圆形等实时预览工具也能复用对称效果。
     */
    struct SymmetryEditState
    {
        bool active = false;
        std::vector<uint32_t> basePixels;
    };

    // 粘贴预览状态（每个项目窗口独立）。
    struct PastePreviewState
    {
        bool active = false;                             // 当前是否处于粘贴预览模式
        commands::PixelClipboardData clipboard;          // 预览使用的剪贴板快照
        int originX = 0;                                 // 预览锚点（画布像素坐标）
        int originY = 0;                                 // 预览锚点（画布像素坐标）
    };

    /**
     * @brief 确保画布纹理存在并具有指定尺寸。
     * 
     * @param width 目标宽度。
     * @param height 目标高度。
     */
    void ensureCanvasTexture(int width, int height);

    // 将像素数据上传到画布纹理
    void uploadCanvasPixels(const std::vector<uint32_t>& pixels) const;

    // 渲染工具栏面板
    void renderToolbarPanel();

    // 渲染左侧面板
    void renderLeftPanel(Project* project);

    // 渲染画布面板
    void renderCanvasPanel(Project* project);

    // 渲染右侧面板
    void renderRightPanel(Project* project);

    // 渲染时间轴面板。
    void renderTimelinePanel(Project* project);

    AppContext* context = nullptr;                  // 应用上下文指针
    std::string windowLabel_;                       // 窗口标签字符串
    std::function<void(AppContext*)> onFocused_;    // 窗口获得焦点时的回调函数
    CanvasTextureState canvasTexture_;              // 画布纹理状态
    PaletteState paletteState_;                     // 调色板状态
    TimelineState timelineState_;                   // 时间轴状态
    ToolbarState toolbarState_;                     // 工具栏状态
    LayerPanelState layerPanelState_;               // 图层面板状态
    StrokeContinuityState strokeState_;             // 连续笔划状态（每个项目窗口独立）
    SymmetryEditState symmetryEditState_;           // 对称绘制状态（每个项目窗口独立）
    LineTool lineTool_;                             // 直线工具实例（每个项目窗口独立）
    CurveTool curveTool_;                           // 曲线工具实例（每个项目窗口独立）
    RectangleTool rectangleTool_;                   // 矩形工具实例（每个项目窗口独立）
    RectFilledTool rectFilledTool_;                 // 填充矩形工具实例（每个项目窗口独立）
    CircleTool circleTool_;                         // 圆形工具实例（每个项目窗口独立）
    CircleFilledTool circleFilledTool_;             // 填充圆形工具实例（每个项目窗口独立）
    RectSelectionTool rectSelectionTool_;           // 矩形框选工具实例（每个项目窗口独立）
    PastePreviewState pastePreviewState_;           // 粘贴预览状态（每个项目窗口独立）
    int lastCanvasWidth_ = 0;                       // 上一帧渲染时的画布宽度（用于自动适配缩放）
    int lastCanvasHeight_ = 0;                      // 上一帧渲染时的画布高度（用于自动适配缩放）
    int pendingCanvasWidth_ = 0;                    // 待处理的画布宽度
    int pendingCanvasHeight_ = 0;                   // 待处理的画布高度
};

#endif // PROJECTWINDOW_H
