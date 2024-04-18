#ifndef COREUTILS_H
#define COREUTILS_H

#include "Core/Core.h"
#include "Core/Threading.h"
#include <concepts>

namespace Pathfinder
{

template <typename T>
    requires requires(T t) {
        t.resize(std::size_t{});
        t.data();
    }
static T LoadData(const std::string_view filePath)
{
    if (filePath.empty())
    {
        PFR_ASSERT(false, "FilePath is empty! Nothing to load data from!");
        return T{};
    }

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

static void SaveData(const std::string_view filePath, const void* data, const int64_t dataSize)
{
    if (filePath.empty())
    {
        PFR_ASSERT(false, "FilePath is empty! Nothing to save data to!");
        return;
    }

    if (!data || dataSize == 0)
    {
        LOG_WARN("Invalid data or dataSize!");
        return;
    }

    std::ofstream file(filePath.data(), std::ios::out | std::ios::binary | std::ofstream::trunc);
    if (!file.is_open())
    {
        LOG_WARN("Failed to open file \"%s\"!", filePath.data());
        return;
    }

    file.write(static_cast<const char*>(data), dataSize);
    file.close();
}

}  // namespace Pathfinder

#endif  // COREUTILS_H
