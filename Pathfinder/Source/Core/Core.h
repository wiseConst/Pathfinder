#pragma once

#include "PlatformDetection.h"
#include "Inheritance.h"
#include "Intrinsics.h"
#include "CoreDefines.h"
#include "UUID.h"

#include <Memory/Memory.h>
#include "Log.h"
#include "Math.h"

#include <chrono>
#include <concepts>

#include <ankerl/unordered_dense.h>

namespace Pathfinder
{

template <class Key, class T, class Hash = ankerl::unordered_dense::hash<Key>, class KeyEqual = std::equal_to<Key>>
using UnorderedMap = ankerl::unordered_dense::map<Key, T, Hash, KeyEqual>;

template <class Key, class Hash = ankerl::unordered_dense::hash<Key>, class KeyEqual = std::equal_to<Key>>
using UnorderedSet = ankerl::unordered_dense::set<Key, Hash, KeyEqual>;

struct WindowResizeData
{
    uint32_t Width{};
    uint32_t Height{};
};
using ResizeCallback = std::function<void(const WindowResizeData&)>;

static constexpr const char* s_DEFAULT_STRING   = "NONE";
static constexpr std::string_view s_ENGINE_NAME = "PATHFINDER";

static constexpr uint16_t s_WORKER_THREAD_COUNT = 16;

template <typename T> using Weak = std::weak_ptr<T>;

template <typename T> using Unique = std::unique_ptr<T>;
template <typename T, typename... Args> NODISCARD FORCEINLINE constexpr Unique<T> MakeUnique(Args&&... args)
{
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template <typename T> using Shared = std::shared_ptr<T>;
template <typename T, typename... Args> NODISCARD FORCEINLINE constexpr Shared<T> MakeShared(Args&&... args)
{
    return std::make_shared<T>(std::forward<Args>(args)...);
}

template <typename T> using Optional = std::optional<T>;
template <typename T, typename... Args> NODISCARD FORCEINLINE constexpr Optional<T> MakeOptional(Args&&... args)
{
    return std::make_optional<T>(std::forward<Args>(args)...);
}

template <typename T> class Pool final : private Uncopyable, private Unmovable
{
  public:
    Pool() noexcept = default;
    ~Pool()         = default;

    using ID = std::uint64_t;

    NODISCARD FORCEINLINE ID Add(const T& element) noexcept
    {
        ID id = {};
        if (!m_FreeIDs.empty())
        {
            id = m_FreeIDs.back();
            m_FreeIDs.pop_back();
            m_Data[id]       = element;
            m_bIsPresent[id] = true;
        }
        else
        {
            id = m_Data.size();
            m_Data.emplace_back(element);
            m_bIsPresent.emplace_back(true);
        }

        return id;
    }

    NODISCARD FORCEINLINE ID Add(T&& element) noexcept
    {
        ID id = {};
        if (!m_FreeIDs.empty())
        {
            id = m_FreeIDs.back();
            m_FreeIDs.pop_back();
            m_Data[id]       = std::forward<T>(element);
            m_bIsPresent[id] = true;
        }
        else
        {
            id = m_Data.size();
            m_Data.emplace_back(std::forward<T>(element));
            m_bIsPresent.emplace_back(true);
        }

        return id;
    }

    FORCEINLINE void Release(const ID& id) noexcept
    {
        PFR_ASSERT(id < m_Data.size() && m_bIsPresent.at(id), "Object is not present in pool!");
        m_bIsPresent[id] = false;
        m_FreeIDs.emplace_back(id);
    }

    NODISCARD FORCEINLINE T& Get(const ID& id) noexcept
    {
        PFR_ASSERT(id < m_Data.size() && m_bIsPresent.at(id), "Object is not present in pool!");
        return m_Data.at(id);
    }

    NODISCARD FORCEINLINE const auto GetSize() const { return m_Data.size(); }
    NODISCARD FORCEINLINE bool IsPresent(const ID& id) const noexcept { return id < m_bIsPresent.size() && m_bIsPresent.at(id); }

    class Iterator final
    {
      public:
        Iterator(Pool<T>& pool, ID id) noexcept : m_Pool(pool), m_ID(id) { NextPresentElement(); }
        ~Iterator() = default;

        NODISCARD FORCEINLINE T& operator*() { return m_Pool.Get(m_ID); }
        FORCEINLINE void operator++()
        {
            ++m_ID;
            NextPresentElement();
        }

        FORCEINLINE bool operator!=(const Iterator& other) const { return m_ID != other.m_ID; }

      private:
        FORCEINLINE void NextPresentElement()
        {
            for (; m_ID < m_Pool.GetSize() && !m_Pool.IsPresent(m_ID); ++m_ID)
                ;
        }

        Pool<T>& m_Pool;
        ID m_ID = 0;
    };

    NODISCARD FORCEINLINE auto begin() { return Iterator(*this, 0); }
    NODISCARD FORCEINLINE auto end() { return Iterator(*this, GetSize()); }

    FORCEINLINE void Clear() { m_Data.clear(), m_bIsPresent.clear(), m_FreeIDs.clear(); }

  private:
    std::vector<T> m_Data;
    std::vector<bool> m_bIsPresent;  // From standard C++98 guarantees, bit-packing => memory-wise
    std::vector<ID> m_FreeIDs;
};

class Timer final
{
  public:
    Timer() noexcept = default;
    ~Timer()         = default;

  NODISCARD FORCEINLINE double GetElapsedSeconds() const { return GetElapsedMilliseconds() / 1000; }

  NODISCARD FORCEINLINE double GetElapsedMilliseconds() const
    {
        const auto elapsed = std::chrono::duration<double, std::milli>(Now() - m_StartTime);
        return elapsed.count();
    }

  NODISCARD FORCEINLINE static std::chrono::time_point<std::chrono::high_resolution_clock> Now()
    {
        return std::chrono::high_resolution_clock::now();
    }

  private:
    std::chrono::time_point<std::chrono::high_resolution_clock> m_StartTime = Now();
};

template <typename T>
    requires requires(T t) {
        t.resize(std::size_t{});
        t.data();
    }
NODISCARD static T LoadData(const std::string_view& filePath)
{
    PFR_ASSERT(!filePath.empty(), "FilePath is empty! Nothing to load data from!");

    std::ifstream file(filePath.data(), std::ios::in | std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        LOG_WARN("Failed to open file \"{}\"!", filePath.data());
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
        LOG_WARN("Failed to open file \"{}\"!", filePath.data());
        return;
    }

    file.write(reinterpret_cast<const char*>(data), dataSize);
    file.close();
}

struct ProfilerTask
{
    double StartTime = 0.0;  // Milliseconds
    double EndTime   = 0.0;  // Milliseconds
    std::string Tag  = s_DEFAULT_STRING;
    glm::vec3 Color{1.f};

    // Milliseconds
    NODISCARD FORCEINLINE auto GetLength() const { return EndTime - StartTime; }
};

}  // namespace Pathfinder
