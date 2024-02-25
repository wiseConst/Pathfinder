#ifndef CORE_H
#define CORE_H

#include "PlatformDetection.h"
#include "Inheritance.h"
#include "CoreDefines.h"

#include "Memory/Memory.h"
#include "Log.h"
#include "Math.h"

#if PFR_WINDOWS
#define DEBUGBREAK() __debugbreak()
#elif PFR_LINUX
#define DEBUGBREAK() __builtin_trap()
#elif PFR_MACOS
#define DEBUGBREAK() __builtin_trap()
#endif

// TODO: Add VERIFY for release mode since assert is descended

#if PFR_RELEASE
#define PFR_ASSERT(x, msg) (x)
#elif PFR_DEBUG
#define PFR_ASSERT(x, msg)                                                                                                                 \
    {                                                                                                                                      \
        if (!(x))                                                                                                                          \
        {                                                                                                                                  \
            LOG_ERROR(msg);                                                                                                                \
            DEBUGBREAK();                                                                                                                  \
        }                                                                                                                                  \
    }
#endif

namespace Pathfinder
{

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

}  // namespace Pathfinder

#endif
