#ifndef KEYEVENT_H
#define KEYEVENT_H

#include "Events.h"
#include "Core/Keys.h"

namespace Pathfinder
{

class KeyEvent : public Event
{

  public:
    KeyEvent() = delete;

    FORCEINLINE EKey GetKey() const { return static_cast<EKey>(m_Key); }

  protected:
    KeyEvent(const std::string& name, const EEventType eventType, const int32_t key, const int32_t scancode)
        : Event(name, eventType), m_Key(key), m_Scancode(scancode)
    {
    }
    virtual ~KeyEvent() = default;

    int32_t m_Key      = 0;
    int32_t m_Scancode = 0;
};

class KeyButtonPressedEvent final : public KeyEvent
{
  public:
    KeyButtonPressedEvent(const int32_t key, const int32_t scancode)
        : KeyEvent("KeyButtonPressedEvent", EEventType::EVENT_TYPE_KEY_BUTTON_PRESSED, key, scancode)
    {
    }
    ~KeyButtonPressedEvent() override = default;

    DECLARE_STATIC_EVENT_TYPE(KEY_BUTTON_PRESSED)
    NODISCARD std::string Format() const final override
    {
        std::string formatted = m_Name + ": (" + std::to_string(m_Key) + ", " + std::to_string(m_Scancode) + ")";
        return formatted;
    }
};

class KeyButtonRepeatedEvent final : public KeyEvent
{
  public:
    KeyButtonRepeatedEvent(const int32_t key, const int32_t scancode)
        : KeyEvent("KeyButtonRepeatedEvent", EEventType::EVENT_TYPE_KEY_BUTTON_REPEATED, key, scancode)
    {
    }
    ~KeyButtonRepeatedEvent() override = default;

    DECLARE_STATIC_EVENT_TYPE(KEY_BUTTON_REPEATED)
    NODISCARD std::string Format() const final override
    {
        std::string formatted = m_Name + ": (" + std::to_string(m_Key) + ", " + std::to_string(m_Scancode) + ")";
        return formatted;
    }
};

class KeyButtonReleasedEvent final : public KeyEvent
{
  public:
    KeyButtonReleasedEvent(const int32_t key, const int32_t scancode)
        : KeyEvent("KeyButtonReleasedEvent", EEventType::EVENT_TYPE_KEY_BUTTON_RELEASED, key, scancode)
    {
    }
    ~KeyButtonReleasedEvent() override = default;

    DECLARE_STATIC_EVENT_TYPE(KEY_BUTTON_RELEASED)
    NODISCARD std::string Format() const final override
    {
        std::string formatted = m_Name + ": (" + std::to_string(m_Key) + ", " + std::to_string(m_Scancode) + ")";
        return formatted;
    }
};

}  // namespace Pathfinder

#endif  // KEYEVENT_H
