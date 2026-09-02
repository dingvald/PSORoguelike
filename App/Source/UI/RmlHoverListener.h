#pragma once

#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>

#include <functional>

namespace Rml {
class Element;
} // namespace Rml

namespace psr {

// Adapts RmlUi "mouseover"/"mouseout" events onto a pair of std::functions --
// the hover analog of RmlClickListener (see its own doc comment for the
// self-detach lifetime rationale, identical here: ElementDocument::Close()
// tears elements down on a later Context::Update(), not synchronously, so
// this listener must detach itself from its element before it is destroyed).
class RmlHoverListener : public Rml::EventListener
{
public:
    RmlHoverListener(std::function<void()> on_enter, std::function<void()> on_leave);
    ~RmlHoverListener() override;

    RmlHoverListener(const RmlHoverListener&) = delete;
    RmlHoverListener& operator=(const RmlHoverListener&) = delete;
    RmlHoverListener(RmlHoverListener&&) = delete;
    RmlHoverListener& operator=(RmlHoverListener&&) = delete;

    // Registers for "mouseover"/"mouseout" on element and remembers it for
    // self-detach.
    void Attach(Rml::Element& element);

    void ProcessEvent(Rml::Event& event) override
    {
        // Copy before invoking -- same reentrancy hazard RmlClickListener's
        // own ProcessEvent guards against: the callback is free to rebuild
        // the list this listener lives in, destroying `this` mid-call.
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
