#pragma once

#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>

#include <functional>
#include <string>

namespace Rml {
class Element;
} // namespace Rml

namespace psr {

// Adapts an RmlUi "click" event onto a std::function, so an editor layer can
// wire a button to a lambda without itself deriving from Rml::EventListener and
// switching on element ids.
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

// RmlClickListener's generalisation to any event type ("change", "dragstart",
// "dragover", "dragdrop", "mousedown", "mousemove", ...) whose handler needs
// the fired Rml::Event& itself (e.g. to read an input's current value, or a
// drag's pointer position) rather than just a bare notification. Same
// self-detach lifetime discipline as RmlClickListener -- see its header note.
class RmlEventListener : public Rml::EventListener
{
public:
    RmlEventListener(std::string event_type, std::function<void(Rml::Event&)> on_event);
    ~RmlEventListener() override;

    RmlEventListener(const RmlEventListener&) = delete;
    RmlEventListener& operator=(const RmlEventListener&) = delete;
    RmlEventListener(RmlEventListener&&) = delete;
    RmlEventListener& operator=(RmlEventListener&&) = delete;

    void Attach(Rml::Element& element);

    void ProcessEvent(Rml::Event& event) override
    {
        if (std::function<void(Rml::Event&)> on_event = m_on_event)
            on_event(event);
    }

    void OnDetach(Rml::Element* element) override
    {
        if (element == m_element)
            m_element = nullptr;
    }

private:
    std::string m_event_type;
    std::function<void(Rml::Event&)> m_on_event;
    Rml::Element* m_element = nullptr;
};

} // namespace psr
