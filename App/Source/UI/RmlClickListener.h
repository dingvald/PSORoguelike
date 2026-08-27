#pragma once

#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>

#include <functional>

namespace Rml {
class Element;
} // namespace Rml

namespace psr {

// Adapts an RmlUi "click" event onto a std::function, so a layer can wire a
// button/slot to a lambda without itself deriving from Rml::EventListener and
// switching on element ids. Mirrors Editor/Source/UI/RmlClickListener.h --
// App can't include Editor sources (wrong dependency direction), so this
// class is duplicated here rather than relocated (moving it to Core would
// mean touching every one of Editor's existing include sites blind).
//
// Lifetime: ElementDocument::Close() is DEFERRED -- the document's elements are
// torn down on a later Context::Update(), not synchronously. So a listener must
// detach itself from its element before it is destroyed, otherwise that deferred
// teardown calls OnDetach() on freed memory. This listener tracks its element
// (Attach) and removes itself in its destructor while the element is still
// alive; OnDetach() clears the tracked pointer if RmlUi detaches us first.
class RmlClickListener : public Rml::EventListener
{
public:
    explicit RmlClickListener(std::function<void()> on_click);
    ~RmlClickListener() override;

    RmlClickListener(const RmlClickListener&) = delete;
    RmlClickListener& operator=(const RmlClickListener&) = delete;
    RmlClickListener(RmlClickListener&&) = delete;
    RmlClickListener& operator=(RmlClickListener&&) = delete;

    // Registers for "click" on element and remembers it for self-detach.
    void Attach(Rml::Element& element);

    void ProcessEvent(Rml::Event& /*event*/) override
    {
        // Copy before invoking: on_click is free to rebuild the list this
        // listener lives in, which destroys `this` -- including the
        // m_on_click member -- while ProcessEvent is still on the call stack.
        // The local copy keeps the callback (and its captures) alive
        // independent of `this`.
        if (std::function<void()> on_click = m_on_click)
            on_click();
    }

    void OnDetach(Rml::Element* element) override
    {
        if (element == m_element)
            m_element = nullptr;
    }

private:
    std::function<void()> m_on_click;
    Rml::Element* m_element = nullptr;
};

} // namespace psr
