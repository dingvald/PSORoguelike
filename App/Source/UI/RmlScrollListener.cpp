#include "UI/RmlScrollListener.h"

#include <RmlUi/Core/Element.h>

#include <utility>

namespace psr {

RmlScrollListener::RmlScrollListener(std::function<void()> on_scroll) : m_on_scroll(std::move(on_scroll)) {}

RmlScrollListener::~RmlScrollListener()
{
    // Detach while the element is still alive (see the header's lifetime note).
    // OnDetach() has already cleared m_element if RmlUi tore the element down first.
    if (m_element)
        m_element->RemoveEventListener("scroll", this);
}

void RmlScrollListener::Attach(Rml::Element& element)
{
    element.AddEventListener("scroll", this);
    m_element = &element;
}

} // namespace psr
