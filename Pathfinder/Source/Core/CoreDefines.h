#pragma once

#include "Log.h"

namespace Pathfinder
{

#if PFR_WINDOWS
#define DEBUGBREAK() __debugbreak()
#elif PFR_LINUX
#define DEBUGBREAK() __builtin_trap()
#elif PFR_MACOS
#define DEBUGBREAK() __builtin_trap()
#endif

#ifdef FORCEINLINE
#undef FORCEINLINE
#endif

#if _MSC_VER
#define FORCEINLINE __forceinline
#else
#define FORCEINLINE __attribute__((always_inline))
#endif

#define NODISCARD [[nodiscard]]

#define BIT(x) (1 << x)

extern void PathfinderShutdown();

#define PRR_DIST 0
#if PFR_RELEASE

#if PFR_DIST

#define PFR_ASSERT(x, msg) (x)

#else

#define PFR_ASSERT(x, msg)                                                                                                                 \
    {                                                                                                                                      \
        if (!(x))                                                                                                                          \
        {                                                                                                                                  \
            LOG_ERROR(msg);                                                                                                                \
            PathfinderShutdown();                                                                                                          \
        }                                                                                                                                  \
    }
#endif

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

}  // namespace Pathfinder