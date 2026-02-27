#ifndef PROJECTWINDOW_H
#define PROJECTWINDOW_H

#include "Window.h"
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

private:
    // 画布纹理状态结构体，用于存储画布纹理的相关信息。
    struct CanvasTextureState
    {
        unsigned int texture = 0; ///< OpenGL 纹理 ID。
        int width = 0;            ///< 纹理宽度。
        int height = 0;           ///< 纹理高度。
    };

    // 调色板状态结构体，用于存储用户自定义调色板及选中颜色的信息。
    struct PaletteState
    {
        std::vector<uint32_t> userPalette; ///< 用户自定义调色板颜色列表。
        int selectedIndex = 0;             ///< 当前选中的颜色索引。
        bool selectedIsUser = false;       ///< 标记当前选中的颜色是否来自用户调色板。
    };

    // 时间轴状态结构体，用于管理动画播放相关状态。
    struct TimelineState
    {
        float height = 200.0f;              ///< 时间轴面板的高度。
        bool isPlaying = false;             ///< 播放状态标志。
        bool loopEnabled = true;            ///< 是否启用循环播放。
        float fps = 8.0f;                   ///< 动画帧率。
        uint64_t lastTick = 0;              ///< 上一次更新的时间戳。
        double accumulator = 0.0;           ///< 时间累加器，用于帧同步。
        unsigned int playIconTexture = 0;   ///< 播放图标纹理 ID。
        unsigned int pauseIconTexture = 0;  ///< 暂停图标纹理 ID。
        bool iconsLoaded = false;           ///< 图标是否已加载。
    };

    
    // 工具栏状态结构体，用于管理工具栏图标的状态
    struct ToolbarState
    {
        bool iconsLoaded = false;               // 图标是否已加载
        unsigned int brushIconTexture = 0;      // 画笔图标纹理 ID
        unsigned int eraserIconTexture = 0;     // 橡皮擦图标纹理 ID
        unsigned int eyedropperIconTexture = 0; // 取色器图标纹理 ID
        unsigned int fillIconTexture = 0;       // 填充工具图标纹理 ID
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
    int pendingCanvasWidth_ = 0;                    // 待处理的画布宽度
    int pendingCanvasHeight_ = 0;                   // 待处理的画布高度
};

#endif // PROJECTWINDOW_H