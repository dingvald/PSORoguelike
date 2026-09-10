#include "UI/InfoPopup.h"

#include "UI/RmlClickListener.h"

#include <RmlUi/Core.h>

namespace psr {

void InfoPopup::Bind(Rml::ElementDocument& document)
{
    m_document = &document;
    m_message = document.GetElementById("info-popup-message");

    if (Rml::Element* ok = document.GetElementById("info-popup-ok"))
    {
        auto listener = std::make_unique<RmlClickListener>([this] { Close(); });
        listener->Attach(*ok);
        m_listeners.push_back(std::move(listener));
    }
}

void InfoPopup::Unbind()
{
    m_listeners.clear();
    m_document = nullptr;
    m_message = nullptr;
}

void InfoPopup::Open(const std::string& message)
{
    if (!m_document)
        return;
    if (m_message)
        m_message->SetInnerRML(message);
    m_open = true;
    m_document->Show();
}

void InfoPopup::Close()
{
    m_open = false;
    if (m_document)
        m_document->Hide();
}

} // namespace psr
