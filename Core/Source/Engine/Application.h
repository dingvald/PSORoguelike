#pragma once

#include "Engine/ApplicationInitContext.h"
#include "Engine/GuiContext.h"
#include "Engine/LayerStack.h"
#include "Engine/Messages/MessageBus.h"

#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace psr {

class Event;
class WindowCloseEvent;
class WindowResizeEvent;
class KeyPressedEvent;

// Drives the layer stack: opens an SDL3 window, initializes RmlUi (system
// and render interfaces, and the single shared context, wrapped in a
// GuiContext, that every layer loads its own document(s) into), and runs
// the main loop, forwarding events/updates/rendering through each layer on
// the stack every frame. This is the seed of the roguelike engine.
class Application
{
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    // Opens the window and initializes RmlUi. Must be called, and must
    // succeed, before pushing layers or calling Run().
    bool Initialize(ApplicationInitContext context);

    // Runs the main loop until the window closes. Returns a process exit
    // code (0 = success). Calls Shutdown() before returning.
    int Run();

    // Pushes a regular layer; it sits below any overlays and is updated,
    // rendered, and offered events before them.
    void PushLayer(std::unique_ptr<Layer> layer);

    // Pushes an overlay; it is updated and rendered after regular layers
    // (drawn on top) and is offered events first.
    void PushOverlay(std::unique_ptr<Layer> overlay);

    // Constructs a TLayer with args, wires up its dependencies (message bus,
    // GUI context -- see Layer), and pushes it as a regular layer.
    template <typename TLayer, typename... TArgs> TLayer& PushLayer(TArgs&&... args)
    {
        auto layer = std::make_unique<TLayer>(std::forward<TArgs>(args)...);
        TLayer& layer_ref = *layer;
        PushLayer(std::unique_ptr<Layer>(std::move(layer)));
        return layer_ref;
    }

    // As above, but pushes the layer as an overlay (see PushOverlay()).
    template <typename TLayer, typename... TArgs> TLayer& PushOverlay(TArgs&&... args)
    {
        auto layer = std::make_unique<TLayer>(std::forward<TArgs>(args)...);
        TLayer& layer_ref = *layer;
        PushOverlay(std::unique_ptr<Layer>(std::move(layer)));
        return layer_ref;
    }

    // Requests the main loop stop after the current frame (same effect as
    // the built-in Escape-quits-the-app / window-close handling).
    void RequestQuit();

    // Constructs a TLayer with args, wires its dependencies, and pushes it as
    // an overlay once it's safe to mutate the layer stack (top of the next
    // Run() frame) -- the deferred counterpart to PushOverlay<TLayer>(), for
    // callers pushing from inside a layer's OnUpdate()/OnEvent() while Run() is
    // still iterating the stack. Prefer calling this via Layer::PushOverlay().
    template <typename TLayer, typename... TArgs> void PushOverlayDeferred(TArgs&&... args)
    {
        auto layer = std::make_unique<TLayer>(std::forward<TArgs>(args)...);
        layer->AttachToBus(m_message_bus);
        layer->AttachToGuiContext(m_gui_context);
        layer->AttachToApplication(*this);

        EnqueueLayerStackChange([this, held = std::move(layer)]() mutable
                                { m_layer_stack.PushOverlay(std::move(held)); });
    }

    // Detaches and destroys layer at the next safe point (top of the next
    // Run() frame) -- see LayerStack::RemoveLayer. Safe to call from inside a
    // layer's own OnEvent()/OnUpdate(), including for that same layer (its
    // OnDetach() then runs next frame, not synchronously). Prefer calling this
    // for self-removal via Layer::RemoveSelf().
    void RemoveLayer(Layer& layer);

    // Constructs a TLayer with args and swaps it in for `from` at `from`'s
    // current stack index (see LayerStack::ReplaceLayer), once it's safe to
    // mutate the layer stack -- not immediately, since this may be called
    // from inside `from`'s own OnEvent()/OnUpdate(), while Run() is still
    // iterating the stack (and `from` is still on the call stack -- an
    // immediate swap would destroy it out from under its own still-running
    // method). The actual replacement happens at the top of the next
    // Run() frame. Prefer calling this via Layer::TransitionTo() rather
    // than directly.
    template <typename TLayer, typename... TArgs> void TransitionTo(Layer& from, TArgs&&... args)
    {
        auto new_layer = std::make_unique<TLayer>(std::forward<TArgs>(args)...);
        new_layer->AttachToBus(m_message_bus);
        new_layer->AttachToGuiContext(m_gui_context);
        new_layer->AttachToApplication(*this);

        Layer* old_layer = &from;
        EnqueueLayerStackChange([this, old_layer, layer = std::move(new_layer)]() mutable
                                { m_layer_stack.ReplaceLayer(old_layer, std::move(layer)); });
    }

private:
    void Shutdown();

    void OnEvent(Event& event);
    bool OnWindowClose(WindowCloseEvent& event);
    bool OnWindowResize(WindowResizeEvent& event);
    bool OnKeyPressed(KeyPressedEvent& event);

    // Queues a layer-stack mutation to run at the next safe point (top of
    // Run()'s frame loop) instead of immediately -- see TransitionTo().
    // move_only_function, not function: the queued closure owns a
    // unique_ptr<Layer> (the new layer), so it isn't copy-constructible --
    // std::function requires its target to be, std::move_only_function
    // doesn't.
    void EnqueueLayerStackChange(std::move_only_function<void()> change);

    struct Impl;
    std::unique_ptr<Impl> m_impl;

    MessageBus m_message_bus;
    LayerStack m_layer_stack;
    GuiContext m_gui_context;
    std::vector<std::move_only_function<void()>> m_pending_layer_stack_changes;
};

} // namespace psr
