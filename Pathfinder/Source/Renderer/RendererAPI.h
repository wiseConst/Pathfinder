#ifndef RENDERERAPI_H
#define RENDERERAPI_H

#include "Core/Core.h"

namespace Pathfinder
{

enum class ERendererAPI : uint8_t
{
    RENDERER_API_NONE   = 0,
    RENDERER_API_VULKAN = 1
};

class RendererAPI
{
  public:
    static FORCEINLINE const auto& Get() { return s_RendererAPI; }
    static FORCEINLINE void Set(ERendererAPI rendererApi)
    {
        if (s_RendererAPI != ERendererAPI::RENDERER_API_NONE) return;
        s_RendererAPI = rendererApi;
    }

  private:
    inline static ERendererAPI s_RendererAPI = ERendererAPI::RENDERER_API_NONE;
};

}  // namespace Pathfinder

#endif  // RENDERERAPI_H
