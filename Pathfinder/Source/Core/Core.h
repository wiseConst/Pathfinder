#pragma once

#include "PlatformDetection.h"
#include "Inheritance.h"
#include "Intrinsics.h"
#include "CoreDefines.h"
#include "UUID.h"

#include <Memory/Memory.h>
#include "Log.h"
#include "Math.h"

namespace Pathfinder
{

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

template <typename T> 
class Pool final : private Uncopyable, private Unmovable
{
  public:
    Pool() noexcept  = default;
    ~Pool() override = default;

    using ID = std::uint64_t;

    NODISCARD FORCEINLINE ID Add(T&& element) noexcept
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

    class PoolIterator final
    {
      public:
        PoolIterator(Pool<T>& pool, ID id) noexcept : m_Pool(pool), m_ID(id) { NextPresentElement(); }
        ~PoolIterator() = default;

        NODISCARD FORCEINLINE T& operator*() { return m_Pool.Get(m_ID); }
        FORCEINLINE void operator++()
        {
            ++m_ID;
            NextPresentElement();
        }

        FORCEINLINE bool operator!=(const PoolIterator& other) const { return m_ID != other.m_ID; }

      private:
        FORCEINLINE void NextPresentElement()
        {
            for (; m_ID < m_Pool.GetSize() && !m_Pool.IsPresent(m_ID); ++m_ID)
                ;
        }

        Pool<T>& m_Pool;
        ID m_ID = 0;
    };

    NODISCARD FORCEINLINE auto begin() { return PoolIterator(*this, 0); }
    NODISCARD FORCEINLINE auto end() { return PoolIterator(*this, GetSize()); }

  private:
    std::vector<T> m_Data;
    std::vector<bool> m_bIsPresent;  // From standard C++98 guarantees, bit-packing => memory-wise
    std::vector<ID> m_FreeIDs;
};

}  // namespace Pathfinder
