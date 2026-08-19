#include "Engine/LayerStack.h"

#include "Engine/Layer.h"

#include <SDL3/SDL.h>

#include <algorithm>

namespace psr {

LayerStack::~LayerStack() { Clear(); }

void LayerStack::Clear()
{
    for (auto it = m_layers.rbegin(); it != m_layers.rend(); ++it)
        (*it)->OnDetach();
    m_layers.clear();
    m_layer_insert_index = 0;
}

void LayerStack::PushLayer(std::unique_ptr<Layer> layer)
{
    layer->OnAttach();
    m_layers.emplace(m_layers.begin() + static_cast<std::ptrdiff_t>(m_layer_insert_index), std::move(layer));
    ++m_layer_insert_index;
}

void LayerStack::PushOverlay(std::unique_ptr<Layer> overlay)
{
    overlay->OnAttach();
    m_layers.emplace_back(std::move(overlay));
}

void LayerStack::ReplaceLayer(Layer* old_layer, std::unique_ptr<Layer> new_layer)
{
    auto it = std::find_if(m_layers.begin(), m_layers.end(),
                           [old_layer](const std::unique_ptr<Layer>& layer) { return layer.get() == old_layer; });
    if (it == m_layers.end())
    {
        SDL_Log("LayerStack::ReplaceLayer: old_layer not found on the stack");
        return;
    }

    (*it)->OnDetach();
    *it = std::move(new_layer);
    (*it)->OnAttach();
}

void LayerStack::RemoveLayer(Layer* layer)
{
    auto it = std::find_if(m_layers.begin(), m_layers.end(),
                           [layer](const std::unique_ptr<Layer>& candidate) { return candidate.get() == layer; });
    if (it == m_layers.end())
    {
        SDL_Log("LayerStack::RemoveLayer: layer not found on the stack");
        return;
    }

    // A regular layer sits before the insert index; removing one shifts the
    // overlay region (and the index new regular layers insert at) down by one.
    const bool is_regular = static_cast<std::size_t>(it - m_layers.begin()) < m_layer_insert_index;

    (*it)->OnDetach();
    m_layers.erase(it);
    if (is_regular)
        --m_layer_insert_index;
}

} // namespace psr
