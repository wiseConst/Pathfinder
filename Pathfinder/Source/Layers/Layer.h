#pragma once

#include "Core/Core.h"

namespace Pathfinder
{

class Event;

class Layer : private Uncopyable, private Unmovable
{
  public:
    Layer(const std::string_view debugName) : m_DebugName(debugName) { LOG_INFO("Created layer \"{}\".", m_DebugName); }
    virtual ~Layer() { LOG_INFO("Destroyed layer \"{}\".", m_DebugName); };

    virtual void Init()    = 0;
    virtual void Destroy() = 0;

    virtual void OnEvent(Event& e)               = 0;
    virtual void OnUpdate(const float deltaTime) = 0;
    virtual void OnUIRender()                    = 0;

  protected:
    std::string_view m_DebugName = s_DEFAULT_STRING;
};

}  // namespace Pathfinder
