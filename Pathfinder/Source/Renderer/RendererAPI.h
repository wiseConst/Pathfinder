#ifndef RENDERERAPI_H
#define RENDERERAPI_H

#include "Core/Core.h"

namespace Pathfinder
{

enum class ERendererAPI : uint8_t
{
    RENDERER_API_VULKAN = 0,
};

class RendererAPI : private Uncopyable, private Unmovable
{
  public:
    static FORCEINLINE const auto& Get() { return s_RendererAPI; }
    static FORCEINLINE void Set(const ERendererAPI rendererApi)
    {
        if (s_RendererAPI == rendererApi) return;
        s_RendererAPI = rendererApi;
    }

  private:
    inline static ERendererAPI s_RendererAPI = ERendererAPI::RENDERER_API_VULKAN;

    RendererAPI()           = default;
    ~RendererAPI() override = default;
};

}  // namespace Pathfinder

#endif  // RENDERERAPI_H
