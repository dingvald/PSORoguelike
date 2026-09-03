#pragma once

#include <cstdint>
#include <unordered_map>

namespace psr {

// Drives synced sprite-strip animation for every RenderableComponent with
// frames > 1: entities sharing the same frame_time read the same bucket, so
// they change frame on the exact same Update() call -- no per-entity clock,
// O(distinct frame_time values) work per frame. Theme-agnostic (frame_time/
// frame_count only), so it lives in Core alongside VisualEffectSystem/
// FloatingTextSystem, the other per-frame-unconditional cosmetic systems.
class AnimationClock
{
public:
    // Advances every bucket already seen by GetFrameIndex, carrying leftover
    // time across frame boundaries (same idiom as TweenSystem::UpdateTweens).
    // Call once per frame, unconditionally -- cheap and non-blocking.
    void Update(float delta_time);

    // Returns the current frame index (0-based, looping) for a bucket keyed
    // by frame_time. 0 if frame_count <= 1 or frame_time <= 0 (not animated
    // / malformed data -- never divides by zero).
    int GetFrameIndex(float frame_time, int frame_count) const;

private:
    struct Bucket
    {
        float elapsed = 0.0f;
        std::uint64_t tick = 0;
    };

    // mutable: GetFrameIndex is logically const (a pure query) but lazily
    // creates a bucket on first sight of a new frame_time so Update() starts
    // advancing it from the next frame on.
    mutable std::unordered_map<float, Bucket> m_clocks;
};

} // namespace psr
