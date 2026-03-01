#include "io/ProjectSerializer.h"

#include "core/Project.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <vector>

namespace
{
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

constexpr std::array<char, 8> kMagic = {'P', 'X', 'A', 'N', 'I', 'M', '1', '\0'};
constexpr uint32_t kVersionV1 = 1;
constexpr uint32_t kVersionV2 = 2;
} // namespace

bool ProjectSerializer::save(const Project& project, const std::string& path, std::string* errorMessage)
{
    std::ofstream out(path, std::ios::binary);
    if (!out)
    {
        if (errorMessage)
            *errorMessage = "Failed to open file for writing: " + path;
        return false;
    }

    FileHeader header{};
    std::copy(kMagic.begin(), kMagic.end(), header.magic);
    header.version = kVersionV2;
    header.width = static_cast<uint32_t>(project.getWidth());
    header.height = static_cast<uint32_t>(project.getHeight());
    header.frameCount = static_cast<uint32_t>(project.getFrameCount());
    const FileHeaderV2Tail tail{static_cast<uint32_t>(project.getName().size())};

    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    if (!out)
    {
        if (errorMessage)
            *errorMessage = "Failed to write file header.";
        return false;
    }

    out.write(reinterpret_cast<const char*>(&tail), sizeof(tail));
    if (!out)
    {
        if (errorMessage)
            *errorMessage = "Failed to write v2 header tail.";
        return false;
    }

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
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        if (errorMessage)
            *errorMessage = "Failed to open file for reading: " + path;
        return nullptr;
    }

    FileHeader header{};
    in.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!in)
    {
        if (errorMessage)
            *errorMessage = "Failed to read file header.";
        return nullptr;
    }

    if (!std::equal(kMagic.begin(), kMagic.end(), header.magic))
    {
        if (errorMessage)
            *errorMessage = "Invalid file magic.";
        return nullptr;
    }

    if (header.version != kVersionV1 && header.version != kVersionV2)
    {
        if (errorMessage)
            *errorMessage = "Unsupported file version.";
        return nullptr;
    }

    auto project = std::make_unique<Project>(
        static_cast<int>(header.width),
        static_cast<int>(header.height),
        static_cast<int>(header.frameCount),
        0x00000000);

    uint32_t nameLength = 0;
    if (header.version >= kVersionV2)
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

    const size_t expectedPixelCount = static_cast<size_t>(header.width) * static_cast<size_t>(header.height);
    for (int i = 0; i < project->getFrameCount(); ++i)
    {
        Project::Frame& frame = project->getFrame(i);
        if (frame.pixels.size() != expectedPixelCount)
            frame.pixels.resize(expectedPixelCount);

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
