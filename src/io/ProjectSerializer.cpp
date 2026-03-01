#include "io/ProjectSerializer.h"

#include "core/Project.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <vector>

namespace
{
/****************************************************************************
=======================
Binary Layout (.pxanim)
=======================

Notation:
- u32 = uint32_t (4 bytes, current implementation writes host endianness)
- []  = fixed-size field
- <>  = variable-size field

V2 layout
---------
Offset  Size   Field
0x00    8      magic[8]            // "PXANIM1\0"
0x08    4      version(u32)        // 2
0x0C    4      width(u32)
0x10    4      height(u32)
0x14    4      frameCount(u32)
0x18    4      nameLength(u32)
0x1C    L      projectNameBytes    // L = nameLength, no trailing '\0'
0x1C+L  N      framePixels         // frameCount * (width*height*4)

Frame pixel block
-----------------
- Each pixel is uint32_t RGBA8888.
- One frame byte size = width * height * sizeof(uint32_t).
- Frames are stored sequentially from frame[0] to frame[frameCount-1].

Forward-compat guidance for V3+
------------------------------
1) Always bump `version`.
2) Append new fields after known header/tail blocks.
3) Keep old fields stable (no reordering/removal).
4) Loader should branch by version and parse incrementally.
5) If aiming cross-platform file stability, explicitly define little-endian encoding.
*************************************************************************************/

/****************************************************************************
=================================
中文对照说明（.pxanim 二进制格式）
=================================

符号约定：
- u32 = uint32_t（4 字节，当前实现按宿主机字节序直接写入）
- []  = 固定长度字段
- <>  = 可变长度字段

V2 布局
-------
偏移    大小   字段
0x00    8      magic[8]            // "PXANIM1\0"
0x08    4      version(u32)        // 2
0x0C    4      width(u32)
0x10    4      height(u32)
0x14    4      frameCount(u32)
0x18    4      nameLength(u32)
0x1C    L      projectNameBytes    // L = nameLength，不包含 '\0'
0x1C+L  N      framePixels         // frameCount * (width*height*4)

帧像素区说明
------------
- 每个像素为 uint32_t RGBA8888。
- 单帧字节数 = width * height * sizeof(uint32_t)。
- 帧按 frame[0] 到 frame[frameCount-1] 连续存储。

V3+ 扩展建议
------------
1) 每次扩展都递增 version。
2) 新字段尽量追加在已知头/尾字段之后。
3) 旧字段保持稳定（不重排、不删除）。
4) 按版本分支解析，逐级兼容。
5) 若要跨平台稳定，建议明确采用小端编码。
*************************************************************************************/

// 文件头（固定区）：
// [magic(8)][version(u32)][width(u32)][height(u32)][frameCount(u32)]
//
// 说明：
// - magic 用于快速判断文件类型是否为 .pxanim。
// - version 用于区分格式版本（当前仅支持 v2）。
// - width/height/frameCount 用于重建 Project 的基础结构。
    struct FileHeader
    {
        char magic[8];
        uint32_t version;
        uint32_t width;
        uint32_t height;
        uint32_t frameCount;
    };

    struct FileHeaderV2Tail
    {
        uint32_t nameLength;
    };

    // 魔数：用于识别文件类型；最后一位 '\0' 只是固定 8 字节的一部分。
    constexpr std::array<char, 8> kMagic = {'P', 'X', 'A', 'N', 'I', 'M', '1', '\0'};
    // v2：基础头 + 项目名长度 + 项目名字节 + 像素帧
    constexpr uint32_t kVersionV2 = 2;
} // namespace

bool ProjectSerializer::save(const Project& project, const std::string& path, std::string* errorMessage)
{
    // 1) 打开输出流（二进制）。失败通常是路径无效或目录不存在/无权限。
    std::ofstream out(path, std::ios::binary);
    if (!out)
    {
        if (errorMessage)
            *errorMessage = "Failed to open file for writing: " + path;
        return false;
    }

    // 2) 组装头信息。
    // 当前保存一律写为 v2，便于保留项目名。
    FileHeader header{};
    std::copy(kMagic.begin(), kMagic.end(), header.magic);
    header.version = kVersionV2;
    header.width = static_cast<uint32_t>(project.getWidth());
    header.height = static_cast<uint32_t>(project.getHeight());
    header.frameCount = static_cast<uint32_t>(project.getFrameCount());
    // v2 扩展头：项目名长度（字节数，不含结尾 '\0'）。
    const FileHeaderV2Tail tail{static_cast<uint32_t>(project.getName().size())};

    // 3) 写基础头。
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    if (!out)
    {
        if (errorMessage)
            *errorMessage = "Failed to write file header.";
        return false;
    }

    // 4) 写 v2 扩展头（nameLength）。
    out.write(reinterpret_cast<const char*>(&tail), sizeof(tail));
    if (!out)
    {
        if (errorMessage)
            *errorMessage = "Failed to write v2 header tail.";
        return false;
    }

    // 5) 写项目名字节（若有）。
    //    这里直接按原始字节写入，不做编码转换。
    if (tail.nameLength > 0)
    {
        out.write(project.getName().data(), static_cast<std::streamsize>(tail.nameLength));
        if (!out)
        {
            if (errorMessage)
                *errorMessage = "Failed to write project name.";
            return false;
        }
    }

    // 6) 依次写每一帧像素。
    // 像素按 uint32_t 连续数组写入，顺序与 frame.pixels 内存布局一致。
    for (int i = 0; i < project.getFrameCount(); ++i)
    {
        const Project::Frame& frame = project.getFrame(i);
        const size_t byteCount = frame.pixels.size() * sizeof(uint32_t);
        out.write(reinterpret_cast<const char*>(frame.pixels.data()), static_cast<std::streamsize>(byteCount));
        if (!out)
        {
            if (errorMessage)
                *errorMessage = "Failed to write frame pixels.";
            return false;
        }
    }

    return true;
}

std::unique_ptr<Project> ProjectSerializer::load(const std::string& path, std::string* errorMessage)
{
    // 1) 打开输入流（二进制）。
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        if (errorMessage)
            *errorMessage = "Failed to open file for reading: " + path;
        return nullptr;
    }

    // 2) 读基础头。
    FileHeader header{};
    in.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!in)
    {
        if (errorMessage)
            *errorMessage = "Failed to read file header.";
        return nullptr;
    }

    // 3) 校验魔数，防止把其他文件当作项目文件解析。
    if (!std::equal(kMagic.begin(), kMagic.end(), header.magic))
    {
        if (errorMessage)
            *errorMessage = "Invalid file magic.";
        return nullptr;
    }

    // 4) 仅接受当前实现支持的版本（v2）。
    if (header.version != kVersionV2)
    {
        if (errorMessage)
            *errorMessage = "Unsupported file version. Only v2 is supported.";
        return nullptr;
    }

    // 5) 先按头信息创建空白项目容器（像素先初始化为透明）。
    auto project = std::make_unique<Project>(
        static_cast<int>(header.width),
        static_cast<int>(header.height),
        static_cast<int>(header.frameCount),
        0x00000000);

    // 6) 读取 v2 扩展信息（nameLength）。
    uint32_t nameLength = 0;
    {
        FileHeaderV2Tail tail{};
        in.read(reinterpret_cast<char*>(&tail), sizeof(tail));
        if (!in)
        {
            if (errorMessage)
                *errorMessage = "Failed to read v2 header tail.";
            return nullptr;
        }
        nameLength = tail.nameLength;
    }

    // 7) 读取项目名（若存在）。
    if (nameLength > 0)
    {
        std::vector<char> nameBytes(nameLength);
        in.read(nameBytes.data(), static_cast<std::streamsize>(nameBytes.size()));
        if (!in)
        {
            if (errorMessage)
                *errorMessage = "Failed to read project name.";
            return nullptr;
        }
        project->setName(std::string(nameBytes.begin(), nameBytes.end()));
    }

    // 8) 逐帧读取像素数据。
    // expectedPixelCount 用于防御性修正帧大小，避免异常文件导致长度不一致。
    const size_t expectedPixelCount = static_cast<size_t>(header.width) * static_cast<size_t>(header.height);
    for (int i = 0; i < project->getFrameCount(); ++i)
    {
        Project::Frame& frame = project->getFrame(i);
        if (frame.pixels.size() != expectedPixelCount)
            frame.pixels.resize(expectedPixelCount);

        // 每帧固定读取 width*height 个 uint32_t 像素。
        const size_t byteCount = frame.pixels.size() * sizeof(uint32_t);
        in.read(reinterpret_cast<char*>(frame.pixels.data()), static_cast<std::streamsize>(byteCount));
        if (!in)
        {
            if (errorMessage)
                *errorMessage = "Failed to read frame pixels.";
            return nullptr;
        }
    }

    return project;
}
