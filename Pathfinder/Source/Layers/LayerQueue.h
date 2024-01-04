#ifndef LAYERQUEUE_H
#define LAYERQUEUE_H

#include "Core/Core.h"
#include "Layer.h"
#include <vector>

namespace Pathfinder
{

class LayerQueue final : private Uncopyable, private Unmovable
{
  public:
    LayerQueue() = default;
    ~LayerQueue() override { Destroy(); }

    FORCEINLINE void Init()
    {
        for (const auto& layer : m_LayerQueue)
        {
            PFR_ASSERT(layer, "Layer is not valid!");

            layer->Init();
        }
    }

    FORCEINLINE void Destroy()
    {
        for (const auto& layer : m_LayerQueue)
        {
            PFR_ASSERT(layer, "Layer is not valid!");

            layer->Destroy();
        }
    }

    FORCEINLINE void OnEvent(Event& e)
    {
        for (const auto& layer : m_LayerQueue)
        {
            PFR_ASSERT(layer, "Layer is not valid!");

            layer->OnEvent(e);
        }
    }

    FORCEINLINE void OnUpdate(const float deltaTime)
    {
        for (const auto& layer : m_LayerQueue)
        {
            PFR_ASSERT(layer, "Layer is not valid!");

            layer->OnUpdate(deltaTime);
        }
    }

    FORCEINLINE void OnUIRender()
    {
        for (const auto& layer : m_LayerQueue)
        {
            PFR_ASSERT(layer, "Layer is not valid!");

            layer->OnUIRender();
        }
    }

    FORCEINLINE void Push(Unique<Layer> layer)
    {
        PFR_ASSERT(layer, "Layer is not valid!");
        m_LayerQueue.push_back(std::move(layer));
    }

  private:
    std::vector<Unique<Layer>> m_LayerQueue;
};

}  // namespace Pathfinder

#endif  // LAYERQUEUE_H
