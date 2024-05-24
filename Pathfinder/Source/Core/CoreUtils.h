#pragma once

#include <Core/Core.h>
#include <concepts>

namespace Pathfinder
{

template <typename T>
    requires requires(T t) {
        t.resize(std::size_t{});
        t.data();
    }
static T LoadData(const std::string_view& filePath)
{
    PFR_ASSERT(!filePath.empty(), "FilePath is empty! Nothing to load data from!");

    std::ifstream file(filePath.data(), std::ios::in | std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        LOG_WARN("Failed to open file \"%s\"!", filePath.data());
        return T{};
    }

    const auto fileSize = file.tellg();
    T buffer            = {};
    buffer.resize(fileSize / sizeof(buffer[0]));

    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

    file.close();
    return buffer;
}

// NOTE: dataSize [bytes]
static void SaveData(const std::string_view& filePath, const void* data, const int64_t dataSize)
{
    PFR_ASSERT(!filePath.empty(), "FilePath is empty! Nothing to save data to!");

    if (!data || dataSize == 0)
    {
        LOG_WARN("Invalid data or dataSize!");
        return;
    }

    std::ofstream file(filePath.data(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file.is_open())
    {
        LOG_WARN("Failed to open file \"%s\"!", filePath.data());
        return;
    }

    file.write(reinterpret_cast<const char*>(data), dataSize);
    file.close();
}

}  // namespace Pathfinder
