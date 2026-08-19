#pragma once

#include "Engine/Events/Event.h"

namespace psr {

class WindowCloseEvent : public Event
{
public:
    WindowCloseEvent() = default;

    PSR_EVENT_CLASS_TYPE(WindowClose)
    PSR_EVENT_CLASS_CATEGORY(EventCategoryApplication)
};

class WindowResizeEvent : public Event
{
public:
    WindowResizeEvent(int width, int height) : m_width(width), m_height(height) {}

    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

    PSR_EVENT_CLASS_TYPE(WindowResize)
    PSR_EVENT_CLASS_CATEGORY(EventCategoryApplication)

private:
    int m_width;
    int m_height;
};

} // namespace psr
