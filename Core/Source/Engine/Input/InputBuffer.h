#pragma once

#include <algorithm>
#include <optional>
#include <vector>

namespace psr {

// Collects held-key input and produces auto-repeating "fired" keys with a
// DAS ("delayed auto-shift") cadence: the first Press() fires immediately,
// then Update() waits an initial delay before the first repeat, then
// repeats at a steady faster interval. TKey is the caller's own key type
// (e.g. an SDL keycode int) -- InputBuffer knows nothing about SDL or any
// input scheme, mirroring ActionMap<TKey>.
//
// The caller drains at most one fired key per Pop(); fires coalesce, so a
// new fire replaces any still-undrained one rather than piling up a
// backlog. When several keys are held at once the most recently pressed one
// repeats (last-pressed-wins); releasing it hands the repeat back to the
// previously held key after a fresh initial delay.
template <typename TKey> class InputBuffer
{
public:
    InputBuffer(float initial_delay_seconds, float repeat_interval_seconds)
        : m_initial_delay(initial_delay_seconds), m_repeat_interval(repeat_interval_seconds)
    {
    }

    // A fresh (non-OS-repeat) key-down. Makes key the active repeater and
    // fires it once immediately, replacing any undrained fire.
    void Press(TKey key)
    {
        std::erase(m_held, key);
        m_held.push_back(key);
        RestartActiveTimer();
        m_buffered = key;
    }

    // A key-up. If the released key was the active repeater, the previously
    // held key (if any) takes over with a fresh initial delay; no fire is
    // produced by a release.
    void Release(TKey key)
    {
        const bool was_active = !m_held.empty() && m_held.back() == key;
        std::erase(m_held, key);
        if (was_active)
            RestartActiveTimer();
    }

    // Advances the active repeater's timer by delta_time and, on crossing
    // the current threshold (initial delay for the first repeat, then the
    // repeat interval), fires the active key. Fires coalesce -- at most one
    // per call.
    void Update(float delta_time)
    {
        if (m_held.empty())
            return;

        m_timer += delta_time;
        const float threshold = m_awaiting_first_repeat ? m_initial_delay : m_repeat_interval;
        if (m_timer >= threshold)
        {
            m_timer -= threshold;
            m_awaiting_first_repeat = false;
            m_buffered = m_held.back();
        }
    }

    // Returns and clears the buffered fired key, if any.
    std::optional<TKey> Pop()
    {
        std::optional<TKey> fired = m_buffered;
        m_buffered.reset();
        return fired;
    }

    // Drops all held keys and any buffered fire -- e.g. on layer detach.
    void Clear()
    {
        m_held.clear();
        m_buffered.reset();
        m_timer = 0.0f;
        m_awaiting_first_repeat = true;
    }

private:
    void RestartActiveTimer()
    {
        m_timer = 0.0f;
        m_awaiting_first_repeat = true;
    }

    float m_initial_delay;
    float m_repeat_interval;

    std::vector<TKey> m_held; // press order; back() is the active repeater
    float m_timer = 0.0f;     // time accrued toward the active key's next fire
    bool m_awaiting_first_repeat = true;
    std::optional<TKey> m_buffered; // single coalesced fired key awaiting Pop()
};

} // namespace psr
