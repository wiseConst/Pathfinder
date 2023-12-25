#ifndef EVENTS_H
#define EVENTS_H

#include <string>

namespace Pathfinder
{

enum class EEventType : uint8_t
{
    EVENT_TYPE_NONE = 0,
    EVENT_TYPE_WINDOW_CLOSE,
    EVENT_TYPE_WINDOW_RESIZE,

    EVENT_TYPE_MOUSE_MOVED,
    EVENT_TYPE_MOUSE_SCROLLED,
    EVENT_TYPE_MOUSE_BUTTON_PRESSED,
    EVENT_TYPE_MOUSE_BUTTON_RELEASED,
    EVENT_TYPE_MOUSE_BUTTON_REPEATED,

    EVENT_TYPE_KEY_BUTTON_PRESSED,
    EVENT_TYPE_KEY_BUTTON_RELEASED,
    EVENT_TYPE_KEY_BUTTON_REPEATED
};

#define DECLARE_STATIC_EVENT_TYPE(type)                                                                                                    \
    FORCEINLINE static EEventType GetStaticType() { return EEventType::EVENT_TYPE_##type; }

class Event : private Uncopyable, private Unmovable
{
  public:
    Event()                                      = delete;
    NODISCARD virtual std::string Format() const = 0;

    FORCEINLINE EEventType GetType() const { return m_EventType; }

  protected:
    Event(const std::string& name, const EEventType eventType) : m_Name(name), m_EventType(eventType) {}
    virtual ~Event() = default;

    std::string m_Name     = "None";
    EEventType m_EventType = EEventType::EVENT_TYPE_NONE;
    bool m_bIsHandled      = false;

    friend class EventDispatcher;
};

class EventDispatcher final : private Uncopyable, private Unmovable
{
  public:
    explicit EventDispatcher(Event& event) : m_Event(event) {}
    EventDispatcher()           = delete;
    ~EventDispatcher() override = default;

    template <typename T, typename F> void Dispatch(F&& func)
    {
        if (m_Event.GetType() != T::GetStaticType() || m_Event.m_bIsHandled) return;

        func(static_cast<T&>(m_Event));
        m_Event.m_bIsHandled = true;
    }

  private:
    Event& m_Event;
};

}  // namespace Pathfinder

#endif  // EVENTS_H
