#pragma once

#include "Events.h"

namespace Pathfinder
{

class WindowResizeEvent final : public Event
{
  public:
    WindowResizeEvent(const uint32_t width, const uint32_t height)
        : Event("WindowResizeEvent", EEventType::EVENT_TYPE_WINDOW_RESIZE), m_Width(width), m_Height(height)
    {
    }
    ~WindowResizeEvent() override = default;

    DECLARE_STATIC_EVENT_TYPE(WINDOW_RESIZE)
    NODISCARD std::string Format() const final override
    {
        std::string formatted = m_Name + ": (" + std::to_string(m_Width) + ", " + std::to_string(m_Height) + ")";
        return formatted;
    }

    NODISCARD FORCEINLINE const float GetAspectRatio() const
    {
        return m_Height == 0 ? 0.0f : static_cast<float>(m_Width) / static_cast<float>(m_Height);
    }

  private:
    uint32_t m_Width  = 0;
    uint32_t m_Height = 0;
};

class WindowCloseEvent final : public Event
{
  public:
    WindowCloseEvent() : Event("WindowCloseEvent", EEventType::EVENT_TYPE_WINDOW_CLOSE) {}
    ~WindowCloseEvent() override = default;

    DECLARE_STATIC_EVENT_TYPE(WINDOW_CLOSE)
    NODISCARD std::string Format() const final override { return m_Name; }
};

}  // namespace Pathfinder

