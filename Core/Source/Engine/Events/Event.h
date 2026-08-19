#pragma once

namespace psr {

enum class EventType
{
    None = 0,
    WindowClose,
    WindowResize,
    KeyPressed,
    KeyReleased,
};

enum EventCategory
{
    EventCategoryNone = 0,
    EventCategoryApplication = 1 << 0,
    EventCategoryInput = 1 << 1,
    EventCategoryKeyboard = 1 << 2,
};

#define PSR_EVENT_CLASS_TYPE(type)                                                                                     \
    static EventType GetStaticType() { return EventType::type; }                                                       \
    EventType GetEventType() const override { return GetStaticType(); }                                                \
    const char* GetName() const override { return #type; }

#define PSR_EVENT_CLASS_CATEGORY(category)                                                                             \
    int GetCategoryFlags() const override { return (category); }

// Base class for engine-level (semantic) events dispatched through the
// layer stack, e.g. window and keyboard events. Distinct from the raw
// platform events forwarded to Layer::OnNativeEvent, which carry the full
// fidelity that backends like RmlUi need (see Layer.h).
class Event
{
public:
    virtual ~Event() = default;

    virtual EventType GetEventType() const = 0;
    virtual const char* GetName() const = 0;
    virtual int GetCategoryFlags() const = 0;

    bool IsInCategory(EventCategory category) const { return (GetCategoryFlags() & category) != 0; }

    bool handled = false;
};

class EventDispatcher
{
public:
    explicit EventDispatcher(Event& event) : m_event(event) {}

    template <typename T, typename F> bool Dispatch(const F& func)
    {
        if (m_event.GetEventType() == T::GetStaticType())
        {
            m_event.handled |= func(static_cast<T&>(m_event));
            return true;
        }
        return false;
    }

private:
    Event& m_event;
};

} // namespace psr
