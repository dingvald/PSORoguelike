#include "Engine/Layer.h"

#include <RmlUi/Core.h>

#include <SDL3/SDL.h>

namespace psr {

Layer::Layer(std::string name) : m_name(std::move(name)) {}

Layer::~Layer()
{
    if (m_document)
        m_document->Close();
    if (m_message_bus)
        m_message_bus->UnsubscribeAll(m_message_queue);
}

} // namespace psr
