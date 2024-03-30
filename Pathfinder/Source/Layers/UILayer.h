#ifndef UILAYER_H
#define UILAYER_H

#include "Core/Core.h"
#include "Layer.h"

namespace Pathfinder
{

// NOTE: Per-RHI UI Layer.
class UILayer : public Layer
{
  public:
    virtual ~UILayer() override = default;

    virtual void Init()    = 0;
    virtual void Destroy() = 0;

    virtual void OnEvent(Event& e)               = 0;
    virtual void OnUpdate(const float deltaTime) = 0;
    virtual void OnUIRender()                    = 0;

    virtual void BeginRender() = 0;
    virtual void EndRender()   = 0;

    NODISCARD static Unique<UILayer> Create();

  protected:
    UILayer() : Layer("UILayer") {}
};

}  // namespace Pathfinder

#endif