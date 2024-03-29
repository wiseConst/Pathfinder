#ifndef MOUSEEVENT_H
#define MOUSEEVENT_H

#include "Events.h"
#include "Core/Keys.h"

namespace Pathfinder
{

class MouseMovedEvent final : public Event
{
  public:
    MouseMovedEvent(const uint32_t xpos, const uint32_t ypos)
        : Event("MouseMovedEvent", EEventType::EVENT_TYPE_MOUSE_MOVED), m_Xpos(xpos), m_Ypos(ypos)
    {
    }
    ~MouseMovedEvent() override = default;

    DECLARE_STATIC_EVENT_TYPE(MOUSE_MOVED)
    NODISCARD std::string Format() const final override
    {
        std::string formatted = m_Name + ": (" + std::to_string(m_Xpos) + ", " + std::to_string(m_Ypos) + ")";
        return formatted;
    }

    FORCEINLINE auto GetPosX() const { return m_Xpos; }
    FORCEINLINE auto GetPosY() const { return m_Ypos; }

  private:
    uint32_t m_Xpos = 0;
    uint32_t m_Ypos = 0;
};

class MouseScrolledEvent final : public Event
{
  public:
    MouseScrolledEvent(const double xOffset, const double yOffset)
        : Event("MouseScrolledEvent", EEventType::EVENT_TYPE_MOUSE_SCROLLED), m_OffsetX(xOffset), m_OffsetY(yOffset)
    {
    }
    ~MouseScrolledEvent() override = default;

    DECLARE_STATIC_EVENT_TYPE(MOUSE_SCROLLED)
    NODISCARD std::string Format() const final override
    {
        std::string formatted = m_Name + ": (" + std::to_string(m_OffsetX) + ", " + std::to_string(m_OffsetY) + ")";
        return formatted;
    }

    FORCEINLINE auto GetOffsetX() const { return m_OffsetX; }
    FORCEINLINE auto GetOffsetY() const { return m_OffsetY; }

  private:
    double m_OffsetX = 0;
    double m_OffsetY = 0;
};

class MouseButtonPressedEvent final : public Event
{
  public:
    explicit MouseButtonPressedEvent(const int32_t key)
        : Event("MouseButtonPressedEvent", EEventType::EVENT_TYPE_MOUSE_BUTTON_PRESSED), m_Key(key)
    {
    }
    ~MouseButtonPressedEvent() override = default;

    DECLARE_STATIC_EVENT_TYPE(MOUSE_BUTTON_PRESSED)
    NODISCARD std::string Format() const final override
    {
        std::string formatted = m_Name + ": (" + std::to_string(m_Key) + ")";
        return formatted;
    }

    FORCEINLINE EKey GetKey() const { return static_cast<EKey>(m_Key); }

  private:
    int32_t m_Key = 0;
};

class MouseButtonReleasedEvent final : public Event
{
  public:
    explicit MouseButtonReleasedEvent(const int32_t key)
        : Event("MouseButtonReleasedEvent", EEventType::EVENT_TYPE_MOUSE_BUTTON_RELEASED), m_Key(key)
    {
    }
    ~MouseButtonReleasedEvent() override = default;

    DECLARE_STATIC_EVENT_TYPE(MOUSE_BUTTON_RELEASED)
    NODISCARD std::string Format() const final override
    {
        std::string formatted = m_Name + ": (" + std::to_string(m_Key) + ")";
        return formatted;
    }

    FORCEINLINE EKey GetKey() const { return static_cast<EKey>(m_Key); }

  private:
    int32_t m_Key = 0;
};

}  // namespace Pathfinder

#endif  // MOUSEEVENT_H
