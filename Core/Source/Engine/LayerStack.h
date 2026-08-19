#pragma once

#include <memory>
#include <vector>

namespace psr {

class Layer;

// Owns the Application's layers in stack order: regular layers occupy the
// front of the vector, overlays always sit behind them at the back so they
// update/render last (drawn on top) and receive events first.
class LayerStack
{
public:
    LayerStack() = default;
    ~LayerStack();

    LayerStack(const LayerStack&) = delete;
    LayerStack& operator=(const LayerStack&) = delete;
    LayerStack(LayerStack&&) = delete;
    LayerStack& operator=(LayerStack&&) = delete;

    void PushLayer(std::unique_ptr<Layer> layer);
    void PushOverlay(std::unique_ptr<Layer> overlay);

    // Swaps old_layer for new_layer at whatever index old_layer currently
    // occupies (regular layer or overlay) -- an in-place unique_ptr
    // assignment, not remove-then-append, so ordering relative to every
    // other layer is preserved. Detaches old_layer (OnDetach()) before the
    // swap, attaches new_layer (OnAttach()) after. No-op (SDL_Log warning,
    // new_layer destroyed) if old_layer isn't on the stack. Like Clear(),
    // NOT safe to call while Application is mid-iteration over the stack --
    // see Application::TransitionTo, the only intended caller.
    void ReplaceLayer(Layer* old_layer, std::unique_ptr<Layer> new_layer);

    // Detaches (OnDetach()) and destroys layer, wherever it sits (regular layer
    // or overlay), and adjusts the regular/overlay split so later pushes still
    // land correctly. No-op (SDL_Log warning) if layer isn't on the stack. Like
    // ReplaceLayer/Clear(), NOT safe to call mid-iteration -- route it through
    // Application::RemoveLayer, which defers it to a safe point.
    void RemoveLayer(Layer* layer);

    // Detaches and destroys every layer, in top-to-bottom order. Callers
    // that need layer teardown to happen before releasing resources the
    // layers depend on (e.g. an SDL renderer) must call this explicitly
    // rather than relying on ~LayerStack, whose timing relative to sibling
    // members is fixed by declaration order.
    void Clear();

    auto begin() { return m_layers.begin(); }
    auto end() { return m_layers.end(); }
    auto rbegin() { return m_layers.rbegin(); }
    auto rend() { return m_layers.rend(); }

private:
    std::vector<std::unique_ptr<Layer>> m_layers;
    std::size_t m_layer_insert_index = 0;
};

} // namespace psr
