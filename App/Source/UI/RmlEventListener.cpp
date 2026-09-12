#include "UI/RmlEventListener.h"

#include <RmlUi/Core/Element.h>

#include <utility>

namespace psr {

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
