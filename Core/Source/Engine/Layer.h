#pragma once

#include "Engine/Application.h"
#include "Engine/GuiContext.h"
#include "Engine/Messages/MessageBus.h"
#include "Engine/Messages/MessageQueue.h"

#include <SDL3/SDL.h>

#include <cassert>
#include <string>
#include <utility>

namespace Rml {
class ElementDocument;
} // namespace Rml

namespace psr {

class Event;

// Base class for a single layer in the Application's layer stack. Layers
// receive updates, rendering, and events every frame in the order they sit
// on the stack (see LayerStack for push/pop ordering).
//
// For ongoing communication, layers only talk to one another through
// Messages (see Subscribe()/Publish() below) -- never by holding a standing
// reference to another layer. TransitionTo() below is the one deliberate
// exception: a one-shot "replace this screen with a new one" handoff, not a
// continuing dependency, so a layer naming another layer's concrete type
// there is fine.
class Layer
{
public:
    explicit Layer(std::string name = "Layer");
    virtual ~Layer();

    Layer(const Layer&) = delete;
    Layer& operator=(const Layer&) = delete;
    Layer(Layer&&) = delete;
    Layer& operator=(Layer&&) = delete;

    // OnAttach() is where a layer should register fonts and load its own
    // document(s) via LoadDocument() -- the GUI context is available by
    // then (see AttachToGuiContext()).

    virtual void OnAttach() {}
    virtual void OnDetach() {}
    virtual void OnUpdate(float delta_time) { (void)delta_time; }
    virtual void OnRender(SDL_Renderer* renderer) { (void)renderer; }
    virtual void OnEvent(Event& event) { (void)event; }

    // Hook for layers wrapping a native input backend that needs
    // full-fidelity platform events the semantic Event hierarchy doesn't
    // model (text composition, touch, mouse wheel deltas, etc). Return false
    // to stop the event from reaching lower layers.
    virtual bool OnNativeEvent(const SDL_Event& event)
    {
        (void)event;
        return true;
    }

    const std::string& GetName() const { return m_name; }

    // Dispatches every message queued since the last call to the handlers
    // registered via Subscribe(). Not called automatically -- call it from
    // OnUpdate(), or from a thread this layer owns, whenever it is ready to
    // process its inbox.
    void HandleQueuedMessages() { m_message_queue.HandleQueuedMessages(); }

protected:
    // Routes messages of type TMessage published anywhere on the bus to
    // instance's handler, delivered into this layer's queue for later
    // processing via HandleQueuedMessages(). TMessage is deducible from the
    // handler's parameter type.
    template <typename TMessage, typename TInstance>
    void Subscribe(void (TInstance::*handler)(const TMessage&), TInstance* instance)
    {
        m_message_queue.RegisterHandler<TMessage>([instance, handler](const TMessage& message)
                                                  { (instance->*handler)(message); });
        if (m_message_bus)
            m_message_bus->Subscribe<TMessage>(m_message_queue);
    }

    // Broadcasts message to every layer subscribed to TMessage.
    template <typename TMessage> void Publish(TMessage message)
    {
        if (m_message_bus)
            m_message_bus->Publish<TMessage>(std::move(message));
    }

    // This layer's RmlUi document, set by the last call to LoadDocument().
    // nullptr until LoadDocument() succeeds.
    Rml::ElementDocument* GetDocument() const { return m_document; }

    GuiContext::LockedAccess GetLockedGuiContext() const
    {
        assert(m_gui_context && "Layer::AttachToGuiContext() must be called before GetLockedGuiContext()");
        return m_gui_context->Lock();
    }

    // The bus this layer publishes/subscribes through -- for handing to
    // collaborators (e.g. SystemContext) that need to publish messages
    // themselves without holding a reference back to this layer.
    MessageBus& GetMessageBus() const
    {
        assert(m_message_bus && "Layer::AttachToBus() must be called before GetMessageBus()");
        return *m_message_bus;
    }

    // Requests the application quit after the current frame -- see
    // Application::RequestQuit().
    void RequestQuit() const
    {
        assert(m_application && "Layer::AttachToApplication() must be called before RequestQuit()");
        m_application->RequestQuit();
    }

    // Pushes a new TLayer and replaces this layer with it at the same stack
    // index, once it's safe to mutate the layer stack -- see
    // Application::TransitionTo(). This layer's OnDetach() runs, then the
    // new layer's OnAttach() runs.
    template <typename TLayer, typename... TArgs> void TransitionTo(TArgs&&... args)
    {
        assert(m_application && "Layer::AttachToApplication() must be called before TransitionTo()");
        m_application->TransitionTo<TLayer>(*this, std::forward<TArgs>(args)...);
    }

    // Pushes a new TLayer overlay above every regular layer, at the next safe
    // point -- see Application::PushOverlayDeferred(). Unlike TransitionTo(),
    // this layer stays on the stack; the overlay is added on top of it.
    template <typename TLayer, typename... TArgs> void PushOverlay(TArgs&&... args)
    {
        assert(m_application && "Layer::AttachToApplication() must be called before PushOverlay()");
        m_application->PushOverlayDeferred<TLayer>(std::forward<TArgs>(args)...);
    }

    // Removes this layer from the stack at the next safe point -- see
    // Application::RemoveLayer(). This layer's OnDetach() runs next frame, not
    // synchronously, so it's safe to call from inside this layer's own
    // OnEvent()/OnUpdate().
    void RemoveSelf()
    {
        assert(m_application && "Layer::AttachToApplication() must be called before RemoveSelf()");
        m_application->RemoveLayer(*this);
    }

private:
    friend class Application;

    // Registers this layer with the bus it will publish/subscribe through.
    // Called by Application when the layer is pushed, before OnAttach().
    void AttachToBus(MessageBus& bus) { m_message_bus = &bus; }

    // Registers the shared GUI context this layer will load its document(s)
    // into via LoadDocument(). Called by Application when the layer is
    // pushed, before OnAttach().
    void AttachToGuiContext(GuiContext& context) { m_gui_context = &context; }

    // Registers the Application this layer was pushed onto, for
    // RequestQuit()/TransitionTo() above. Called by Application when the
    // layer is pushed, before OnAttach().
    void AttachToApplication(Application& application) { m_application = &application; }

    std::string m_name;
    MessageBus* m_message_bus = nullptr;
    MessageQueue m_message_queue;
    GuiContext* m_gui_context = nullptr;
    Application* m_application = nullptr;
    Rml::ElementDocument* m_document = nullptr;
};

} // namespace psr
