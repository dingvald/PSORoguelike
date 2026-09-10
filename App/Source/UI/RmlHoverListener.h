#pragma once

#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>

#include <functional>

namespace Rml {
class Element;
} // namespace Rml

namespace psr {

// Adapts RmlUi "mouseover"/"mouseout" events onto a pair of std::functions,
// same "wire a row to a lambda without deriving Rml::EventListener per call
// site" role RmlClickListener.h fills for "click" -- see that header's doc
// comment for the lifetime contract (deferred ElementDocument::Close()
// teardown), which this class follows identically.
class RmlHoverListener : public Rml::EventListener
{
public:
    RmlHoverListener(std::function<void()> on_enter, std::function<void()> on_leave);
    ~RmlHoverListener() override;

    RmlHoverListener(const RmlHoverListener&) = delete;
    RmlHoverListener& operator=(const RmlHoverListener&) = delete;
    RmlHoverListener(RmlHoverListener&&) = delete;
    RmlHoverListener& operator=(RmlHoverListener&&) = delete;

    // Registers for both "mouseover" and "mouseout" on element and remembers
    // it for self-detach.
    void Attach(Rml::Element& element);

    void ProcessEvent(Rml::Event& event) override
    {
        // Copy before invoking -- see RmlClickListener::ProcessEvent's doc
        // comment for why (on_enter/on_leave rebuilding this listener's own
        // owning list mid-call would otherwise destroy `this` underneath us).
        if (event.GetType() == "mouseover")
        {
            if (std::function<void()> on_enter = m_on_enter)
                on_enter();
        }
        else if (event.GetType() == "mouseout")
        {
            if (std::function<void()> on_leave = m_on_leave)
                on_leave();
        }
    }

    void OnDetach(Rml::Element* element) override
    {
        if (element == m_element)
            m_element = nullptr;
    }

private:
    std::function<void()> m_on_enter;
    std::function<void()> m_on_leave;
    Rml::Element* m_element = nullptr;
};

} // namespace psr
