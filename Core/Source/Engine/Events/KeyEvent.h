#pragma once

#include "Engine/Events/Event.h"

namespace psr {

class KeyEvent : public Event
{
public:
    int GetKeyCode() const { return m_key_code; }

    PSR_EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)

protected:
    explicit KeyEvent(int key_code) : m_key_code(key_code) {}

    int m_key_code;
};

class KeyPressedEvent : public KeyEvent
{
public:
    KeyPressedEvent(int key_code, bool repeat) : KeyEvent(key_code), m_repeat(repeat) {}

    bool IsRepeat() const { return m_repeat; }

    PSR_EVENT_CLASS_TYPE(KeyPressed)

private:
    bool m_repeat;
};

class KeyReleasedEvent : public KeyEvent
{
public:
    explicit KeyReleasedEvent(int key_code) : KeyEvent(key_code) {}

    PSR_EVENT_CLASS_TYPE(KeyReleased)
};

} // namespace psr
