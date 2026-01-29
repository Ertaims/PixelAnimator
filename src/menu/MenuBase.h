#pragma once

#include <string>
#include <functional>

// 菜单基类
class MenuBase {
protected:
    std::string name;         // 菜单名称
    std::string shortcut;     // 快捷键
    bool enabled;             // 是否启用

public:
    // 构造函数
    MenuBase(const std::string& name, const std::string& shortcut = "", bool enabled = true);
    
    // 析构函数
    virtual ~MenuBase() = default;
    
    // 获取菜单名称
    const std::string& getName() const;
    
    // 获取快捷键
    const std::string& getShortcut() const;
    
    // 设置是否启用
    void setEnabled(bool enabled);
    
    // 获取是否启用
    bool isEnabled() const;
    
    // 渲染菜单
    virtual void render() = 0;
};