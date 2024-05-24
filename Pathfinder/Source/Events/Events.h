#pragma once

#include <string>

namespace Pathfinder
{

enum class EEventType : uint8_t
{
    EVENT_TYPE_NONE          = 0,
    EVENT_TYPE_WINDOW_CLOSE  = 1,
    EVENT_TYPE_WINDOW_RESIZE = 2,

    EVENT_TYPE_MOUSE_MOVED           = 3,
    EVENT_TYPE_MOUSE_SCROLLED        = 4,
    EVENT_TYPE_MOUSE_BUTTON_PRESSED  = 5,
    EVENT_TYPE_MOUSE_BUTTON_RELEASED = 6,
    EVENT_TYPE_MOUSE_BUTTON_REPEATED = 7,

    EVENT_TYPE_KEY_BUTTON_PRESSED  = 8,
    EVENT_TYPE_KEY_BUTTON_RELEASED = 9,
    EVENT_TYPE_KEY_BUTTON_REPEATED = 10
};

#define DECLARE_STATIC_EVENT_TYPE(type)                                                                                                    \
    FORCEINLINE static EEventType GetStaticType()                                                                                          \
    {                                                                                                                                      \
        return EEventType::EVENT_TYPE_##type;                                                                                              \
    }

class Event : private Uncopyable, private Unmovable
{
  public:
    NODISCARD virtual std::string Format() const = 0;
    FORCEINLINE EEventType GetType() const { return m_EventType; }
    FORCEINLINE bool IsHandled() const { return m_bIsHandled; }

    NODISCARD FORCEINLINE bool IsKeyEvent() const
    {
        return m_EventType >= EEventType::EVENT_TYPE_KEY_BUTTON_PRESSED && m_EventType <= EEventType::EVENT_TYPE_KEY_BUTTON_REPEATED;
    }

    NODISCARD FORCEINLINE bool IsMouseEvent() const
    {
        return m_EventType >= EEventType::EVENT_TYPE_MOUSE_MOVED && m_EventType <= EEventType::EVENT_TYPE_MOUSE_BUTTON_REPEATED;
    }

  protected:
    std::string m_Name     = s_DEFAULT_STRING;
    bool m_bIsHandled      = false;
    EEventType m_EventType = EEventType::EVENT_TYPE_NONE;

    friend class EventDispatcher;

    Event() = delete;
    Event(const std::string& name, const EEventType eventType) : m_Name(name), m_EventType(eventType) {}
    virtual ~Event() = default;
};

class EventDispatcher final : private Uncopyable, private Unmovable
{
  public:
    explicit EventDispatcher(Event& event) : m_Event(event) {}
    ~EventDispatcher() = default;

    template <typename T, typename F> 
    FORCEINLINE void Dispatch(F&& func)
    {
        if (m_Event.GetType() != T::GetStaticType() || m_Event.m_bIsHandled) return;

        m_Event.m_bIsHandled = func(static_cast<T&>(m_Event));
    }

  private:
    Event& m_Event;

    EventDispatcher() = delete;
};

}  // namespace Pathfinder
