#include "HomeWindow.h"
#include "imgui.h"
#include "SDL3/SDL.h"
#include "SDL3/SDL_opengl.h"

void HomeWindow::render() {
    if (!visible) return;
    
    ImGui::Begin(name);
    
    // 添加标题
    ImGui::SetWindowFontScale(1.5f);
    ImGui::Text("Pixel Animator");
    ImGui::SetWindowFontScale(1.0f);
    
    // 添加描述性文本
    ImGui::Text("Welcome to Pixel Animator - your creative tool for pixel art animation!");
    ImGui::Spacing();
    ImGui::Text("This application allows you to create, edit, and animate pixel art sprites with ease.");
    
    // 添加快速操作按钮
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Text("Quick Actions:");
    ImGui::Spacing();
    
    if (ImGui::Button("New Project", ImVec2(150, 40))) {
        if (newProjectCallback) {
            newProjectCallback();
        }
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Open Project", ImVec2(150, 40))) {
        if (openProjectCallback) {
            openProjectCallback();
        }
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Save Project", ImVec2(150, 40))) {
        if (saveProjectCallback) {
            saveProjectCallback();
        }
    }
    
    ImGui::Spacing();
    
    if (ImGui::Button("Export Animation", ImVec2(150, 40))) {
        if (exportAnimationCallback) {
            exportAnimationCallback();
        }
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Tutorial", ImVec2(150, 40))) {
        if (tutorialCallback) {
            tutorialCallback();
        }
    }
    
    // 添加应用程序信息
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Text("Application Info:");
    ImGui::Spacing();
    
    ImGui::Text("Version: 1.0.0");
    ImGui::Text("OpenGL Version: %s", (const char*)glGetString(GL_VERSION));
    ImGui::Text("SDL Version: %d.%d.%d", SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION);
    ImGui::Text("ImGui Version: %s", IMGUI_VERSION);
    
    ImGui::End();
}
