#pragma once

namespace Pathfinder
{

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

}  // namespace Pathfinder