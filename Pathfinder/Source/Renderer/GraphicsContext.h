#ifndef GRAPHICSCONTEXT_H
#define GRAPHICSCONTEXT_H

#include "RendererAPI.h"
#include "Core/Core.h"
#include "RendererCoreDefines.h"

namespace Pathfinder
{

class GraphicsContext : private Uncopyable, private Unmovable
{
  public:
    virtual ~GraphicsContext() { s_Instance = nullptr; }

    template <typename T> NODISCARD FORCEINLINE static T& Get()
    {
        PFR_ASSERT(s_Instance, "Graphics context instance is not valid!");
        return static_cast<T&>(*s_Instance);
    }
    NODISCARD FORCEINLINE static auto& Get()
    {
        PFR_ASSERT(s_Instance, "Graphics context instance is not valid!");
        return *s_Instance;
    }

    static Unique<GraphicsContext> Create(const ERendererAPI rendererApi);

    virtual void FillMemoryBudgetStats(std::vector<MemoryBudget>& memoryBudgets) = 0;

    virtual void Begin() = 0;
    virtual void End()   = 0;

  protected:
    static inline GraphicsContext* s_Instance = nullptr;

    GraphicsContext() noexcept = default;
    virtual void Destroy()     = 0;
};

}  // namespace Pathfinder

#endif  // GRAPHICSCONTEXT_H
