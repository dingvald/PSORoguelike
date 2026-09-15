#include "Engine/Render/AnimationClock.h"

namespace psr {

void AnimationClock::Advance(Bucket& bucket, float frame_time, float delta_time)
{
    bucket.elapsed += delta_time;
    while (bucket.elapsed >= frame_time)
    {
        bucket.elapsed -= frame_time;
        ++bucket.tick;
    }
}

void AnimationClock::Update(float delta_time)
{
    for (auto& [frame_time, bucket] : m_clocks)
        Advance(bucket, frame_time, delta_time);

    for (auto& [entity, bucket] : m_entity_clocks)
        Advance(bucket, bucket.frame_time, delta_time);
}

int AnimationClock::GetFrameIndex(float frame_time, int frame_count) const
{
    if (frame_count <= 1 || frame_time <= 0.0f)
        return 0;

    return static_cast<int>(m_clocks[frame_time].tick % static_cast<std::uint64_t>(frame_count));
}

int AnimationClock::GetFrameIndex(entt::entity entity, float frame_time, int frame_count) const
{
    if (frame_count <= 1 || frame_time <= 0.0f)
        return 0;

    Bucket& bucket = m_entity_clocks[entity];
    bucket.frame_time = frame_time;
    return static_cast<int>(bucket.tick % static_cast<std::uint64_t>(frame_count));
}

void AnimationClock::Forget(entt::entity entity) { m_entity_clocks.erase(entity); }

} // namespace psr
