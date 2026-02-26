#ifndef PROJECTWINDOW_H
#define PROJECTWINDOW_H

#include "Window.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class AppContext;
class Project;

class ProjectWindow : public Window {
public:
    ProjectWindow(AppContext* context,
                  const std::string& windowLabel,
                  const std::function<void(AppContext*)>& onFocused = {})
        : Window("ProjectWindow"), context(context), windowLabel_(windowLabel), onFocused_(onFocused) {}
    ~ProjectWindow() override;

    void render() override;

    const char* getWindowLabel() const { return windowLabel_.c_str(); }
    void setWindowLabel(const std::string& label) { windowLabel_ = label; }

private:
    struct CanvasTextureState
    {
        unsigned int texture = 0;
        int width = 0;
        int height = 0;
    };

    struct PaletteState
    {
        std::vector<uint32_t> userPalette;
        int selectedIndex = 0;
        bool selectedIsUser = false;
    };

    struct TimelineState
    {
        float height = 200.0f;
        bool isPlaying = false;
        bool loopEnabled = true;
        float fps = 8.0f;
        uint64_t lastTick = 0;
        double accumulator = 0.0;
        unsigned int playIconTexture = 0;
        unsigned int pauseIconTexture = 0;
        bool iconsLoaded = false;
    };

    struct ToolbarState
    {
        bool iconsLoaded = false;
        unsigned int brushIconTexture = 0;
        unsigned int eraserIconTexture = 0;
        unsigned int eyedropperIconTexture = 0;
        unsigned int fillIconTexture = 0;
    };

    void ensureCanvasTexture(int width, int height);
    void uploadCanvasPixels(const std::vector<uint32_t>& pixels) const;
    void renderToolbarPanel();

    void renderLeftPanel(Project* project);
    void renderCanvasPanel(Project* project);
    void renderRightPanel(Project* project);
    void renderTimelinePanel(Project* project);

    AppContext* context = nullptr;
    std::string windowLabel_;
    std::function<void(AppContext*)> onFocused_;
    CanvasTextureState canvasTexture_;
    PaletteState paletteState_;
    TimelineState timelineState_;
    ToolbarState toolbarState_;
    int pendingCanvasWidth_ = 0;
    int pendingCanvasHeight_ = 0;
};

#endif // PROJECTWINDOW_H
