#include "io/ProjectJsonSerializer.h"

#include "core/Project.h"
#include "nlohmann/json.hpp"

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

namespace
{
    using json = nlohmann::json;

    // ---------------- JSON 格式常量 ----------------
    // format + version 共同决定“这是哪个格式、哪个版本”。
    constexpr const char* kFormatName = "PixelAnimatorProject";
    constexpr int kJsonVersion = 2;
    // RGBA8888(uint32_t) -> "#RRGGBBAA"
    // 这里保持与项目内像素布局一致：R 在最低字节，A 在最高字节。
    std::string rgbaToHex(uint32_t rgba)
    {
        std::ostringstream oss;
        oss << '#'
            << std::uppercase << std::hex << std::setfill('0')
            << std::setw(2) << ((rgba >> 0) & 0xFF)
            << std::setw(2) << ((rgba >> 8) & 0xFF)
            << std::setw(2) << ((rgba >> 16) & 0xFF)
            << std::setw(2) << ((rgba >> 24) & 0xFF);
        return oss.str();
    }

    // 从字符串 s 的 offset 位置解析两个十六进制字符（一个字节）。
    bool parseHexByte(const std::string& s, size_t offset, uint8_t& out)
    {
        if (offset + 2 > s.size()) return false;
        const std::string part = s.substr(offset, 2);
        unsigned int value = 0;
        std::istringstream iss(part);
        iss >> std::hex >> value;
        if (iss.fail() || !iss.eof() || value > 0xFF) return false;
        out = static_cast<uint8_t>(value);
        return true;
    }

    // 从字符串 s 中解析 "#RRGGBBAA" 格式的颜色值。
    bool hexToRgba(const std::string& text, uint32_t& outColor)
    {
        // 支持格式：#RRGGBBAA。
        // 严格长度校验可避免 "#FFF" 这类简写造成歧义。
        if (text.size() != 9 || text[0] != '#') return false;

        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0;
        uint8_t a = 0;
        if (!parseHexByte(text, 1, r) || !parseHexByte(text, 3, g)
            || !parseHexByte(text, 5, b) || !parseHexByte(text, 7, a))
        {
            return false;
        }

        outColor = (static_cast<uint32_t>(r) << 0)
            | (static_cast<uint32_t>(g) << 8)
            | (static_cast<uint32_t>(b) << 16)
            | (static_cast<uint32_t>(a) << 24);
        return true;
    }

    // 生成 UTC 时间戳（ISO8601），用于 meta.createdAt / updatedAt。
    std::string utcNowIso8601()
    {
        std::time_t now = std::time(nullptr);
        std::tm tmUtc{};
    #if defined(_WIN32)
        gmtime_s(&tmUtc, &now);
    #else
        gmtime_r(&now, &tmUtc);
    #endif
        std::ostringstream oss;
        oss << std::put_time(&tmUtc, "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    }

    // 统一错误写回入口，减少重复 if(errorMessage) 判断。
    void assignError(std::string* errorMessage, const std::string& message)
    {
        if (errorMessage) *errorMessage = message;
    }
}

bool ProjectJsonSerializer::save(const Project& project, const std::string& path, std::string* errorMessage)
{
    // 构建根对象与格式标识。
    //    load() 会严格校验 format/version，保证格式可控。
    json root;
    root["format"] = kFormatName;
    root["version"] = kJsonVersion;

    // 写入元信息。
    //    当前 createdAt/updatedAt 都写当前时间；后续可从项目状态中读取真实值。
    const std::string now = utcNowIso8601();
    root["meta"] = {
        {"name", project.getName()},
        {"createdAt", now},
        {"updatedAt", now}
    };

    // 写入画布基础数据。
    root["canvas"] = {
        {"width", project.getWidth()},
        {"height", project.getHeight()},
        {"background", {{"mode", "checkerboard"}}}
    };

    // 写入时间轴概要信息。
    root["timeline"] = {
        {"fps", project.getTimelineFps()},
        {"frameCount", project.getFrameCount()}
    };

    // 写入图层结构。
    root["layerState"] = {
        {"activeLayerIndex", project.getActiveLayerIndex()}
    };
    json layers = json::array();
    for (int layerIndex = 0; layerIndex < project.getLayerCount(); ++layerIndex)
    {
        const Project::LayerInfo& layer = project.getLayerInfo(layerIndex);
        layers.push_back({
            {"index", layerIndex},
            {"name", layer.name},
            {"visible", layer.visible},
            {"locked", layer.locked},
            {"opacity", layer.opacity}
        });
    }
    root["layers"] = std::move(layers);

    // 写入帧数组。
    //    v2 开始每帧写入每图层像素，保证多图层项目能完整恢复。
    json frames = json::array();
    for (int i = 0; i < project.getFrameCount(); ++i)
    {
        json frameLayers = json::array();
        for (int layerIndex = 0; layerIndex < project.getLayerCount(); ++layerIndex)
        {
            json pixels = json::array();
            for (uint32_t color : project.getLayerPixels(i, layerIndex))
                pixels.push_back(rgbaToHex(color));

            frameLayers.push_back({
                {"layerIndex", layerIndex},
                {"pixels", std::move(pixels)}
            });
        }

        frames.push_back({
            {"index", i},
            {"duration", 1},
            {"layers", std::move(frameLayers)}
        });
    }
    root["frames"] = std::move(frames);

    // 写入可选 UI 状态字段（当前为默认模板值）。
    //    这些字段暂不反向驱动 Project 核心结构，但保留可扩展空间。
    root["palette"] = {
        "#00000000", "#FFFFFFFF", "#FF0000FF", "#00FF00FF", "#0000FFFF"
    };
    root["toolState"] = {
        {"activeTool", "brush"},
        {"brushSize", 1}
    };

    // 落盘输出（2 空格缩进，便于人工阅读）。
    std::ofstream out(path);
    if (!out)
    {
        assignError(errorMessage, "Failed to open JSON file for writing: " + path);
        return false;
    }
    out << root.dump(2) << '\n';
    if (!out)
    {
        assignError(errorMessage, "Failed to write JSON content.");
        return false;
    }
    return true;
}

std::unique_ptr<Project> ProjectJsonSerializer::load(const std::string& path, std::string* errorMessage)
{
    // 打开并解析 JSON。
    std::ifstream in(path);
    if (!in)
    {
        assignError(errorMessage, "Failed to open JSON file for reading: " + path);
        return nullptr;
    }

    json root;
    try
    {
        in >> root;
    }
    catch (const std::exception& e)
    {
        assignError(errorMessage, std::string("Failed to parse JSON: ") + e.what());
        return nullptr;
    }

    if (!root.is_object())
    {
        assignError(errorMessage, "Invalid JSON root: expected object.");
        return nullptr;
    }

    // 格式识别：先校验 format，再校验 version。
    //    这样可以更清晰地区分“文件不是本格式”与“版本不兼容”。
    const std::string format = root.value("format", "");
    if (format != kFormatName)
    {
        assignError(errorMessage, "Invalid JSON format.");
        return nullptr;
    }

    const int version = root.value("version", 0);
    if (version != 1 && version != kJsonVersion)
    {
        assignError(errorMessage, "Unsupported JSON version.");
        return nullptr;
    }

    // 读取画布尺寸。
    //    尺寸非法时直接失败，避免后续创建异常 Project。
    const json& canvas = root["canvas"];
    const int width = canvas.value("width", 0);
    const int height = canvas.value("height", 0);
    if (width <= 0 || height <= 0)
    {
        assignError(errorMessage, "Invalid canvas size in JSON.");
        return nullptr;
    }

    // 读取帧信息并做一致性校验。
    //    优先使用 timeline.frameCount；若缺失则回退到 frames.size()。
    const json timeline = root.value("timeline", json::object());
    const int frameCountFromTimeline = timeline.value("frameCount", 0);
    const int timelineFps = timeline.value("fps", 8);
    const json& frames = root["frames"];
    if (!frames.is_array())
    {
        assignError(errorMessage, "Invalid frames field: expected array.");
        return nullptr;
    }

    const int frameCount = frameCountFromTimeline > 0
        ? frameCountFromTimeline
        : static_cast<int>(frames.size());
    if (frameCount <= 0)
    {
        assignError(errorMessage, "Invalid frame count in JSON.");
        return nullptr;
    }
    if (static_cast<int>(frames.size()) != frameCount)
    {
        assignError(errorMessage, "Frame count mismatch between timeline and frames array.");
        return nullptr;
    }

    // 构建 Project，并恢复项目名。
    auto project = std::make_unique<Project>(width, height, frameCount, 0x00000000);
    const std::string name = root.value("meta", json::object()).value("name", "Untitled");
    project->setName(name);
    project->setTimelineFps(timelineFps);

    const size_t expectedPixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);

    // v1：旧格式，只有单图层 pixels。
    if (version == 1)
    {
        for (int i = 0; i < frameCount; ++i)
        {
            const json& frameObj = frames[static_cast<size_t>(i)];
            if (!frameObj.is_object() || !frameObj.contains("pixels") || !frameObj["pixels"].is_array())
            {
                assignError(errorMessage, "Invalid frame object at index " + std::to_string(i) + ".");
                return nullptr;
            }

            const json& pixelsArray = frameObj["pixels"];
            if (pixelsArray.size() != expectedPixelCount)
            {
                assignError(errorMessage, "Pixel count mismatch at frame " + std::to_string(i) + ".");
                return nullptr;
            }

            Project::Frame& frame = project->getFrame(i);
            frame.pixels.resize(expectedPixelCount);
            for (size_t p = 0; p < expectedPixelCount; ++p)
            {
                if (!pixelsArray[p].is_string())
                {
                    assignError(errorMessage,
                                "Invalid pixel type at frame " + std::to_string(i)
                                    + ", pixel " + std::to_string(p) + ".");
                    return nullptr;
                }

                uint32_t color = 0;
                const std::string text = pixelsArray[p].get<std::string>();
                if (!hexToRgba(text, color))
                {
                    assignError(errorMessage,
                                "Invalid pixel color at frame " + std::to_string(i)
                                    + ", pixel " + std::to_string(p) + ": " + text);
                    return nullptr;
                }
                frame.pixels[p] = color;
            }
        }
        return project;
    }

    // v2：读取图层结构。
    const json& layers = root["layers"];
    if (!layers.is_array() || layers.empty())
    {
        assignError(errorMessage, "Invalid layers field: expected non-empty array.");
        return nullptr;
    }

    while (project->getLayerCount() < static_cast<int>(layers.size()))
    {
        project->addLayer();
    }

    for (size_t layerIndex = 0; layerIndex < layers.size(); ++layerIndex)
    {
        const json& layerObj = layers[layerIndex];
        if (!layerObj.is_object())
        {
            assignError(errorMessage, "Invalid layer object at index " + std::to_string(layerIndex) + ".");
            return nullptr;
        }

        project->renameLayer(static_cast<int>(layerIndex), layerObj.value("name", "Layer"));
        project->setLayerVisible(static_cast<int>(layerIndex), layerObj.value("visible", true));
        project->setLayerLocked(static_cast<int>(layerIndex), layerObj.value("locked", false));
        project->setLayerOpacity(static_cast<int>(layerIndex), layerObj.value("opacity", 1.0f));
    }

    const json layerState = root.value("layerState", json::object());
    project->setActiveLayerIndex(layerState.value("activeLayerIndex", 0));

    // 逐帧逐层解析像素。
    for (int i = 0; i < frameCount; ++i)
    {
        const json& frameObj = frames[static_cast<size_t>(i)];
        if (!frameObj.is_object() || !frameObj.contains("layers") || !frameObj["layers"].is_array())
        {
            assignError(errorMessage, "Invalid frame layers at index " + std::to_string(i) + ".");
            return nullptr;
        }

        const json& frameLayers = frameObj["layers"];
        if (static_cast<int>(frameLayers.size()) != project->getLayerCount())
        {
            assignError(errorMessage, "Layer count mismatch at frame " + std::to_string(i) + ".");
            return nullptr;
        }

        for (int layerIndex = 0; layerIndex < project->getLayerCount(); ++layerIndex)
        {
            const json& frameLayerObj = frameLayers[static_cast<size_t>(layerIndex)];
            if (!frameLayerObj.is_object() || !frameLayerObj.contains("pixels") || !frameLayerObj["pixels"].is_array())
            {
                assignError(errorMessage,
                            "Invalid frame layer object at frame " + std::to_string(i)
                                + ", layer " + std::to_string(layerIndex) + ".");
                return nullptr;
            }

            const json& pixelsArray = frameLayerObj["pixels"];
            if (pixelsArray.size() != expectedPixelCount)
            {
                assignError(errorMessage,
                            "Pixel count mismatch at frame " + std::to_string(i)
                                + ", layer " + std::to_string(layerIndex) + ".");
                return nullptr;
            }

            std::vector<uint32_t>& pixels = project->getLayerPixels(i, layerIndex);
            pixels.resize(expectedPixelCount);
            for (size_t p = 0; p < expectedPixelCount; ++p)
            {
                if (!pixelsArray[p].is_string())
                {
                    assignError(errorMessage,
                                "Invalid pixel type at frame " + std::to_string(i)
                                    + ", layer " + std::to_string(layerIndex)
                                    + ", pixel " + std::to_string(p) + ".");
                    return nullptr;
                }

                uint32_t color = 0;
                const std::string text = pixelsArray[p].get<std::string>();
                if (!hexToRgba(text, color))
                {
                    assignError(errorMessage,
                                "Invalid pixel color at frame " + std::to_string(i)
                                    + ", layer " + std::to_string(layerIndex)
                                    + ", pixel " + std::to_string(p) + ": " + text);
                    return nullptr;
                }
                pixels[p] = color;
            }
        }
    }

    return project;
}

