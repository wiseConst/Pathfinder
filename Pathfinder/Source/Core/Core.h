#ifndef CORE_H
#define CORE_H

#include "PlatformDetection.h"
#include "Inheritance.h"
#include "Intrinsics.h"
#include "CoreDefines.h"
#include "UUID.h"

#include "Memory/Memory.h"
#include "Log.h"
#include "Math.h"

namespace Pathfinder
{

static constexpr const char* s_DEFAULT_STRING = "NONE";

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
