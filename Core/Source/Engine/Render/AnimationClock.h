#pragma once

#include <entt/entt.hpp>

#include <cstdint>
#include <unordered_map>

namespace psr {

// Drives sprite-strip animation for every RenderableComponent with
// frames > 1, in two modes selected by RenderableComponent::is_synced:
//   - synced (default): entities sharing the same frame_time read the same
//     bucket, so they change frame on the exact same Update() call -- no
//     per-entity clock, O(distinct frame_time values) work per frame. Good
//     for ambient/looping content that should stay in lockstep.
//   - unsynced: each entity reads its own bucket, keyed by entt::entity, so
//     it always plays its strip from its own spawn regardless of what else
//     shares its frame_time. Good for one-shot VFX (hit flashes, technique
//     effects) that must play in full rather than sync to siblings. Callers
//     must invoke Forget() when such an entity is destroyed, or its bucket
//     leaks for the rest of the session (see RegistryRenderableLookup).
// Theme-agnostic (frame_time/frame_count/entt::entity only), so it lives in
// Core alongside VisualEffectSystem/FloatingTextSystem, the other
// per-frame-unconditional cosmetic systems.
class AnimationClock
{
public:
    // Advances every bucket already seen by GetFrameIndex, carrying leftover
    // time across frame boundaries (same idiom as TweenSystem::UpdateTweens).
    // Call once per frame, unconditionally -- cheap and non-blocking.
    void Update(float delta_time);

    // Synced mode: returns the current frame index (0-based, looping) for a
    // bucket keyed by frame_time. 0 if frame_count <= 1 or frame_time <= 0
    // (not animated / malformed data -- never divides by zero).
    int GetFrameIndex(float frame_time, int frame_count) const;

    // Unsynced mode: same as above, but the bucket is keyed by entity, so
    // this entity's tick never collides with another entity's even if they
    // share frame_time.
    int GetFrameIndex(entt::entity entity, float frame_time, int frame_count) const;

    // Drops entity's unsynced bucket, if any. Call when an unsynced entity
    // is destroyed so m_entity_clocks doesn't grow for the rest of the
    // session. Harmless no-op if entity was never queried unsynced.
    void Forget(entt::entity entity);

private:
    struct Bucket
    {
        float elapsed = 0.0f;
        std::uint64_t tick = 0;
        // Only set/used for m_entity_clocks entries: m_clocks is keyed by
        // frame_time itself, so Update() reads the period straight off the
        // map key there. m_entity_clocks is keyed by entity, so each bucket
        // has to carry its own period for Update() to advance it by.
        float frame_time = 0.0f;
    };

    static void Advance(Bucket& bucket, float frame_time, float delta_time);

    // mutable: GetFrameIndex is logically const (a pure query) but lazily
    // creates a bucket on first sight of a new key so Update() starts
    // advancing it from the next frame on.
    mutable std::unordered_map<float, Bucket> m_clocks;
    mutable std::unordered_map<entt::entity, Bucket> m_entity_clocks;
};

} // namespace psr
