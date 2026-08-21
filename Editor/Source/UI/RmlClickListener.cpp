#include "UI/RmlClickListener.h"

#include <RmlUi/Core/Element.h>

#include <utility>

namespace psr {

RmlClickListener::RmlClickListener(std::function<void()> on_click) : m_on_click(std::move(on_click)) {}

RmlClickListener::~RmlClickListener()
{
    // Detach while the element is still alive (see the header's lifetime note).
    // OnDetach() has already cleared m_element if RmlUi tore the element down first.
    if (m_element)
        m_element->RemoveEventListener("click", this);
}

void RmlClickListener::Attach(Rml::Element& element)
{
    element.AddEventListener("click", this);
    m_element = &element;
}

RmlEventListener::RmlEventListener(std::string event_type, std::function<void(Rml::Event&)> on_event)
    : m_event_type(std::move(event_type)), m_on_event(std::move(on_event))
{
}

RmlEventListener::~RmlEventListener()
{
    if (m_element)
        m_element->RemoveEventListener(m_event_type, this);
}

void RmlEventListener::Attach(Rml::Element& element)
{
    element.AddEventListener(m_event_type, this);
    m_element = &element;
}

} // namespace psr
