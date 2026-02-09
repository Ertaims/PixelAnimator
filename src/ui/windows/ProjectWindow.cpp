#include "ProjectWindow.h"
#include "imgui.h"

void ProjectWindow::render() {
    if (!visible) return;
    
    ImGui::Begin(name);
    
    // 添加项目信息
    ImGui::Text("Project Settings");
    ImGui::Separator();
    ImGui::Spacing();
    
    // 项目名称
    ImGui::Text("Project Name:");
    ImGui::SameLine();
    ImGui::Text(projectName);
    ImGui::Spacing();
    
    // 项目尺寸
    ImGui::Text("Project Size:");
    ImGui::SameLine();
    ImGui::Text("%dx%d", width, height);
    ImGui::Spacing();
    
    // 其他项目设置
    ImGui::Text("Canvas Settings:");
    ImGui::Spacing();
    
    static int newWidth = width;
    static int newHeight = height;
    
    ImGui::InputInt("Width", &newWidth);
    ImGui::InputInt("Height", &newHeight);
    
    if (ImGui::Button("Apply Size Change")) {
        if (newWidth > 0 && newHeight > 0) {
            width = newWidth;
            height = newHeight;
        }
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // 项目统计信息
    ImGui::Text("Project Statistics:");
    ImGui::Spacing();
    
    // 这里可以添加更多项目相关的统计信息
    ImGui::Text("Layers: 1");
    ImGui::Text("Frames: 1");
    ImGui::Text("Total Pixels: %d", width * height);
    
    ImGui::End();
}
