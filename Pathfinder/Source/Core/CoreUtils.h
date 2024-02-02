#ifndef COREUTILS_H
#define COREUTILS_H

#include "Core/Core.h"
#include "Core/Threading.h"

namespace Pathfinder
{

template <typename T> static T LoadData(const std::string_view filePath)
{
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
    std::ofstream file(filePath.data(), std::ios::out | std::ios::binary | std::ofstream::trunc);
    if (!file.is_open())
    {
        LOG_WARN("Failed to open file \"%s\"!", filePath.data());
        return;
    }

    if (!data || dataSize == 0)
    {
        LOG_WARN("Invalid data!");
        file.close();
        return;
    }
    file.write(static_cast<const char*>(data), dataSize);

    file.close();
}

}  // namespace Pathfinder

#endif  // COREUTILS_H
