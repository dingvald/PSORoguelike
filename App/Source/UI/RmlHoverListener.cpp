#include "UI/RmlHoverListener.h"

#include <RmlUi/Core/Element.h>

#include <utility>

namespace psr {

RmlHoverListener::RmlHoverListener(std::function<void()> on_enter, std::function<void()> on_leave)
    : m_on_enter(std::move(on_enter)), m_on_leave(std::move(on_leave))
{
}

RmlHoverListener::~RmlHoverListener()
{
    // Detach while the element is still alive (see the header's lifetime note).
    // OnDetach() has already cleared m_element if RmlUi tore the element down first.
    if (m_element)
    {
        m_element->RemoveEventListener("mouseover", this);
        m_element->RemoveEventListener("mouseout", this);
    }
}

void RmlHoverListener::Attach(Rml::Element& element)
{
    element.AddEventListener("mouseover", this);
    element.AddEventListener("mouseout", this);
    m_element = &element;
}

} // namespace psr
