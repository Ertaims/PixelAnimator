#pragma once

#include <vector>

class AppContext;
class Project;

/**
 * @brief 右侧属性栏中的图层面板。
 *
 * 该类只负责图层列表、图层按钮、重命名弹窗和透明度滑杆，
 * 让 ProjectWindow 不再直接维护大量图层 UI 状态。
 */
class LayerPanel
{
public:
    // 绘制图层面板，并把用户操作同步到 Project / AppContext。
    void render(Project& project, AppContext& context);

    // 释放图层按钮图标纹理；由 ProjectWindow 在销毁 OpenGL 资源时调用。
    void releaseTextures();

private:
    // 面板运行时状态：图标纹理、Ctrl 多选结果、重命名弹窗输入缓存。
    struct State
    {
        bool iconsLoaded = false;
        unsigned int newLayerIconTexture = 0;
        unsigned int deleteIconTexture = 0;
        unsigned int upIconTexture = 0;
        unsigned int downIconTexture = 0;
        unsigned int showIconTexture = 0;
        unsigned int hideIconTexture = 0;
        unsigned int lockIconTexture = 0;
        unsigned int unlockIconTexture = 0;
        std::vector<int> selectedLayerIndices;
        bool openRenamePopup = false;
        int renameLayerIndex = -1;
        char renameLayerName[64] = "";
    };

    // 首次渲染时加载按钮图标，后续复用纹理 ID。
    void ensureIconTextures();

    State m_state;
};
