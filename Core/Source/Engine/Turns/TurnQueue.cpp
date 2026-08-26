#include "Engine/Turns/TurnQueue.h"

#include <algorithm>
#include <cassert>

namespace psr {

TurnQueue::TurnQueue(int action_threshold) : m_action_threshold(action_threshold) {}

TurnQueue::Key TurnQueue::MakeKey(const ActorState& state) const
{
    int ticks_needed = std::max(0, m_action_threshold - state.base_energy);
    return Key{state.base_time + ticks_needed, state.seq};
}

void TurnQueue::Enqueue(entt::entity id, int initial_energy)
{
    assert(!m_states.contains(id) && "TurnQueue: id is already queued");

    ActorState state{initial_energy, m_global_time, m_next_seq++};
    Key key = MakeKey(state);
    m_states.emplace(id, state);
    m_order.emplace(key, id);
}

void TurnQueue::Remove(entt::entity id)
{
    auto it = m_states.find(id);
    assert(it != m_states.end() && "TurnQueue: Remove called for an id that isn't queued");

    m_order.erase(MakeKey(it->second));
    m_states.erase(it);
}

entt::entity TurnQueue::NextActor()
{
    assert(!m_order.empty() && "TurnQueue: NextActor called on an empty queue");

    const auto& [key, id] = *m_order.begin();
    m_global_time = key.first;
    return id;
}

void TurnQueue::Requeue(entt::entity id, int remaining_energy)
{
    auto it = m_states.find(id);
    assert(it != m_states.end() && "TurnQueue: Requeue called for an id that isn't queued");

    m_order.erase(MakeKey(it->second));

    ActorState& state = it->second;
    state.base_energy = remaining_energy;
    state.base_time = m_global_time;

    m_order.emplace(MakeKey(state), id);
}

int TurnQueue::GetEnergy(entt::entity id) const
{
    auto it = m_states.find(id);
    assert(it != m_states.end() && "TurnQueue: GetEnergy called for an id that isn't queued");

    const ActorState& state = it->second;
    return state.base_energy + static_cast<int>(m_global_time - state.base_time);
}

bool TurnQueue::Contains(entt::entity id) const { return m_states.contains(id); }

bool TurnQueue::IsEmpty() const { return m_order.empty(); }

std::vector<TurnQueue::Snapshot> TurnQueue::GetSnapshot() const
{
    std::vector<Snapshot> result;
    result.reserve(m_order.size());

    for (const auto& [key, id] : m_order)
    {
        const ActorState& state = m_states.at(id);
        int energy = state.base_energy + static_cast<int>(m_global_time - state.base_time);
        result.push_back(Snapshot{id, energy, static_cast<int>(key.first - m_global_time)});
    }

    return result;
}

} // namespace psr
