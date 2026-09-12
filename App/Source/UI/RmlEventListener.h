#pragma once

#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>

#include <functional>
#include <string>

namespace Rml {
class Element;
} // namespace Rml

namespace psr {

// RmlClickListener's generalisation to any event type ("mousedown",
// "mousemove", "mouseup", "mousescroll", ...) whose handler needs the fired
// Rml::Event& itself (e.g. to read a mouse position or wheel delta) rather
// than just a bare notification. Mirrors Editor/Source/UI/RmlClickListener.h's
// own RmlEventListener -- App can't include Editor sources (wrong dependency
// direction), so this class is duplicated here rather than relocated, same
// reasoning RmlClickListener.h's own header note already gives.
//
// Lifetime: same self-detach discipline as RmlClickListener -- see its
// header note.
class RmlEventListener : public Rml::EventListener
{
public:
    RmlEventListener(std::string event_type, std::function<void(Rml::Event&)> on_event);
    ~RmlEventListener() override;

    RmlEventListener(const RmlEventListener&) = delete;
    RmlEventListener& operator=(const RmlEventListener&) = delete;
    RmlEventListener(RmlEventListener&&) = delete;
    RmlEventListener& operator=(RmlEventListener&&) = delete;

    void Attach(Rml::Element& element);

    void ProcessEvent(Rml::Event& event) override
    {
        if (std::function<void(Rml::Event&)> on_event = m_on_event)
            on_event(event);
    }

    void OnDetach(Rml::Element* element) override
    {
        if (element == m_element)
            m_element = nullptr;
    }

private:
    std::string m_event_type;
    std::function<void(Rml::Event&)> m_on_event;
    Rml::Element* m_element = nullptr;
};

} // namespace psr
