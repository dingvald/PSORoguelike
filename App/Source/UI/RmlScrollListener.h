#pragma once

#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>

#include <functional>

namespace Rml {
class Element;
} // namespace Rml

namespace psr {

// Adapts an RmlUi "scroll" event onto a std::function -- same idiom as
// RmlClickListener, just bound to "scroll" instead of "click" (kept as its
// own class rather than generalizing RmlClickListener, which many
// hotbar/menu call sites already depend on).
//
// Lifetime: same self-detach-before-document-teardown contract as
// RmlClickListener -- see that header's doc comment for why.
class RmlScrollListener : public Rml::EventListener
{
public:
    explicit RmlScrollListener(std::function<void()> on_scroll);
    ~RmlScrollListener() override;

    RmlScrollListener(const RmlScrollListener&) = delete;
    RmlScrollListener& operator=(const RmlScrollListener&) = delete;
    RmlScrollListener(RmlScrollListener&&) = delete;
    RmlScrollListener& operator=(RmlScrollListener&&) = delete;

    // Registers for "scroll" on element and remembers it for self-detach.
    void Attach(Rml::Element& element);

    void ProcessEvent(Rml::Event& /*event*/) override
    {
        // Copy before invoking -- see RmlClickListener::ProcessEvent's doc
        // comment for why.
        if (std::function<void()> on_scroll = m_on_scroll)
            on_scroll();
    }

    void OnDetach(Rml::Element* element) override
    {
        if (element == m_element)
            m_element = nullptr;
    }

private:
    std::function<void()> m_on_scroll;
    Rml::Element* m_element = nullptr;
};

} // namespace psr
