#ifndef KEYEVENT_H
#define KEYEVENT_H

#include "Events.h"

namespace Pathfinder
{

class KeyButtonPressedEvent final : public Event
{
  public:
    KeyButtonPressedEvent(const int32_t key, const int32_t scancode)
        : Event("KeyButtonPressedEvent", EEventType::EVENT_TYPE_KEY_BUTTON_PRESSED), m_Key(key), m_Scancode(scancode)
    {
    }
    ~KeyButtonPressedEvent() override = default;

    DECLARE_STATIC_EVENT_TYPE(KEY_BUTTON_PRESSED)
    NODISCARD std::string Format() const final override
    {
        std::string formatted = m_Name + ": (" + std::to_string(m_Key) + ", " + std::to_string(m_Scancode) + ")";
        return formatted;
    }

    FORCEINLINE auto GetKey() const { return m_Key; }

  private:
    int32_t m_Key      = 0;
    int32_t m_Scancode = 0;
};

class KeyButtonRepeatedEvent final : public Event
{
  public:
    KeyButtonRepeatedEvent(const int32_t key, const int32_t scancode)
        : Event("KeyButtonRepeatedEvent", EEventType::EVENT_TYPE_KEY_BUTTON_REPEATED), m_Key(key), m_Scancode(scancode)
    {
    }
    ~KeyButtonRepeatedEvent() override = default;

    DECLARE_STATIC_EVENT_TYPE(KEY_BUTTON_REPEATED)
    NODISCARD std::string Format() const final override
    {
        std::string formatted = m_Name + ": (" + std::to_string(m_Key) + ", " + std::to_string(m_Scancode) + ")";
        return formatted;
    }

    FORCEINLINE auto GetKey() const { return m_Key; }

  private:
    int32_t m_Key      = 0;
    int32_t m_Scancode = 0;
};

class KeyButtonReleasedEvent final : public Event
{
  public:
    KeyButtonReleasedEvent(const int32_t key, const int32_t scancode)
        : Event("KeyButtonReleasedEvent", EEventType::EVENT_TYPE_KEY_BUTTON_RELEASED), m_Key(key), m_Scancode(scancode)
    {
    }
    ~KeyButtonReleasedEvent() override = default;

    DECLARE_STATIC_EVENT_TYPE(KEY_BUTTON_RELEASED)
    NODISCARD std::string Format() const final override
    {
        std::string formatted = m_Name + ": (" + std::to_string(m_Key) + ", " + std::to_string(m_Scancode) + ")";
        return formatted;
    }

    FORCEINLINE auto GetKey() const { return m_Key; }

  private:
    int32_t m_Key      = 0;
    int32_t m_Scancode = 0;
};

}  // namespace Pathfinder

#endif  // KEYEVENT_H
